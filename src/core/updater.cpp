#include "openupdater/core/updater.hpp"

#include "openupdater/core/backup.hpp"
#include "openupdater/core/github.hpp"
#include "openupdater/core/error.hpp"

#include <array>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <curl/curl.h>
#endif

namespace openupdater {

#ifdef _WIN32

namespace {

std::wstring widen(const std::string& value) {
    if (value.empty()) return {};

    const int size = MultiByteToWideChar(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        nullptr, 0);

    if (size <= 0)
        throw std::runtime_error("Failed to convert URL to UTF-16.");

    std::wstring result(static_cast<std::size_t>(size), L'\0');

    if (MultiByteToWideChar(
            CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
            result.data(), size) <= 0) {
        throw std::runtime_error("Failed to convert URL to UTF-16.");
    }

    return result;
}

void download_windows(
    const std::string& url,
    const std::filesystem::path& destination,
    const HttpHeaders& headers) {

    const auto wide_url = widen(url);

    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);

    wchar_t host[256]{};
    wchar_t path[2048]{};
    components.lpszHostName = host;
    components.dwHostNameLength = static_cast<DWORD>(std::size(host));
    components.lpszUrlPath = path;
    components.dwUrlPathLength = static_cast<DWORD>(std::size(path));

    if (!WinHttpCrackUrl(
            wide_url.c_str(),
            static_cast<DWORD>(wide_url.size()),
            0,
            &components)) {
        throw std::runtime_error("Invalid HTTP(S) URL.");
    }

    HINTERNET session = WinHttpOpen(
        L"OpenUpdater/1.1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);

    if (!session)
        throw std::runtime_error("Failed to initialize WinHTTP.");

    const auto close_session = [&]() { WinHttpCloseHandle(session); };

    HINTERNET connection = WinHttpConnect(
        session,
        components.lpszHostName,
        components.nPort,
        0);

    if (!connection) {
        close_session();
        throw std::runtime_error("Failed to connect to update server.");
    }

    const auto close_connection = [&]() {
        WinHttpCloseHandle(connection);
        close_session();
    };

    const DWORD flags =
        components.nScheme == INTERNET_SCHEME_HTTPS
            ? WINHTTP_FLAG_SECURE
            : 0;

    HINTERNET request = WinHttpOpenRequest(
        connection,
        L"GET",
        components.lpszUrlPath,
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags);

    if (!request) {
        close_connection();
        throw std::runtime_error("Failed to create HTTP request.");
    }

    std::wstring additional_headers;
    for (const auto& [name, value] : headers) {
        additional_headers += widen(name + ": " + value);
        additional_headers += L"\r\n";
    }

    const LPCWSTR header_data =
        additional_headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS
                                    : additional_headers.c_str();

    if (!WinHttpSendRequest(
            request,
            header_data,
            additional_headers.empty()
                ? 0
                : static_cast<DWORD>(additional_headers.size()),
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0) ||
        !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        close_connection();
        throw std::runtime_error("HTTP request failed.");
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);

    if (!WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status_code,
            &status_size,
            WINHTTP_NO_HEADER_INDEX) ||
        status_code < 200 || status_code >= 300) {
        WinHttpCloseHandle(request);
        close_connection();
        throw std::runtime_error("HTTP server returned a non-success status.");
    }

    std::ofstream output(destination, std::ios::binary);
    if (!output) {
        WinHttpCloseHandle(request);
        close_connection();
        throw std::runtime_error(
            "Cannot open download destination: " + destination.string());
    }

    std::array<char, 64 * 1024> buffer{};

    while (true) {
        DWORD available = 0;

        if (!WinHttpQueryDataAvailable(request, &available)) {
            WinHttpCloseHandle(request);
            close_connection();
            throw std::runtime_error("Failed to read HTTP response.");
        }

        if (available == 0)
            break;

        std::vector<char> chunk(available);
        DWORD read = 0;

        if (!WinHttpReadData(request, chunk.data(), available, &read)) {
            WinHttpCloseHandle(request);
            close_connection();
            throw std::runtime_error("Failed to download response.");
        }

        output.write(chunk.data(), static_cast<std::streamsize>(read));
        if (!output) {
            WinHttpCloseHandle(request);
            close_connection();
            throw std::runtime_error("Failed to write downloaded file.");
        }
    }

    WinHttpCloseHandle(request);
    close_connection();
}

} // namespace

#endif

void Downloader::download(
    const std::string& url,
    const std::filesystem::path& destination,
    const HttpHeaders& headers) {

    if (url.empty())
        throw std::runtime_error("Download URL cannot be empty.");

    if (destination.empty())
        throw std::runtime_error("Download destination cannot be empty.");

    if (destination.has_parent_path())
        std::filesystem::create_directories(destination.parent_path());

#ifdef _WIN32
    download_windows(url, destination, headers);
#else
    CURL* curl = curl_easy_init();
    if (!curl)
        throw std::runtime_error("Failed to initialize libcurl.");

    std::ofstream output(destination, std::ios::binary);
    if (!output) {
        curl_easy_cleanup(curl);
        throw std::runtime_error(
            "Cannot open download destination: " + destination.string());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    curl_slist* request_headers = nullptr;
    for (const auto& [name, value] : headers) {
        const auto header = name + ": " + value;
        request_headers = curl_slist_append(request_headers, header.c_str());
    }

    if (request_headers)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, request_headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
        +[](char* data, std::size_t size, std::size_t count, void* user) {
            auto* stream = static_cast<std::ofstream*>(user);
            stream->write(data, static_cast<std::streamsize>(size * count));
            return stream->good() ? size * count : 0;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output);

    const auto result = curl_easy_perform(curl);
    if (request_headers)
        curl_slist_free_all(request_headers);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK)
        throw std::runtime_error(
            std::string("Download failed: ") + curl_easy_strerror(result));
#endif
}



void UpdateSelection::select(const std::string& component) {
    if (component.empty())
        throw std::invalid_argument("Update component name cannot be empty.");

    if (!selected(component))
        components.push_back(component);
}

void UpdateSelection::deselect(const std::string& component) {
    components.erase(
        std::remove(components.begin(), components.end(), component),
        components.end());
}

void UpdateSelection::clear() {
    components.clear();
}

void UpdateSelection::select_all(
    const std::vector<AvailableUpdate>& updates) {
    components.clear();
    for (const auto& update : updates)
        select(update.component);
}

bool UpdateSelection::selected(const std::string& component) const noexcept {
    return std::find(components.begin(), components.end(), component) !=
        components.end();
}

bool UpdateSelection::empty() const noexcept {
    return components.empty();
}

std::vector<AvailableUpdate> Updater::discover_updates(
    const UpdateDiscoveryRequest& request) {

    if (request.repository.empty() ||
        request.repository.find('/') == std::string::npos) {
        throw UpdateError(
            ErrorCode::InvalidArgument,
            "GitHub repository must use the owner/repository format.");
    }

    if (request.targets.empty())
        throw UpdateError(
            ErrorCode::InvalidArgument,
            "At least one update target is required.");

    if (request.platform == Platform::Unknown ||
        request.architecture == Architecture::Unknown) {
        throw UpdateError(
            ErrorCode::InvalidArgument,
            "Platform and architecture must be known.");
    }

    try {
        for (const auto& target : request.targets) {
            if (target.component.empty())
                throw UpdateError(
                    ErrorCode::InvalidArgument,
                    "Update component name cannot be empty.");

            if (!target.current.valid())
                throw UpdateError(
                    ErrorCode::InvalidVersion,
                    "Update target has an invalid current version.");
        }

        const auto candidates = GitHubReleasesProvider::discover(
            request.repository,
            request.targets,
            request.platform,
            request.architecture,
            request.headers);

        std::vector<AvailableUpdate> result;
        result.reserve(candidates.size());

        for (const auto& candidate : candidates) {
            if (candidate.available > candidate.current)
                result.push_back(candidate);
        }

        return result;
    } catch (const UpdateError&) {
        throw;
    } catch (const std::exception& error) {
        throw UpdateError(
            ErrorCode::UpdateCheckFailed,
            error.what());
    }
}

UpdateCheck Updater::check(const Version& current, const Manifest& manifest) {
    if (!current.valid())
        throw std::runtime_error("Current version is invalid.");

    if (manifest.version > current)
        return {current, manifest.version, UpdateState::UpdateAvailable};

    return {current, manifest.version, UpdateState::UpToDate};
}

namespace {

std::string read_signature(
    const std::filesystem::path& path) {

    std::ifstream input(path);
    if (!input)
        throw std::runtime_error(
            "Cannot open signature file: " + path.string());

    std::string value(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());

    value.erase(
        std::remove_if(
            value.begin(),
            value.end(),
            [](unsigned char character) {
                return std::isspace(character) != 0;
            }),
        value.end());

    return value;
}

std::filesystem::path install_package(
    const std::filesystem::path& package,
    const std::filesystem::path& destination,
    const std::string& expected_sha256,
    const UpdateOptions& options) {

    if (!std::filesystem::exists(package))
        throw std::runtime_error("Package does not exist: " + package.string());

    if (!expected_sha256.empty() && !verify_sha256(package, expected_sha256))
        throw std::runtime_error("Package SHA-256 verification failed.");

    std::filesystem::create_directories(destination);

    const auto target = destination / package.filename();
    const auto state_directory = destination / ".openupdater";
    const auto staging_directory = state_directory / "staging";
    const auto backup_directory = state_directory / "backups";

    std::filesystem::create_directories(staging_directory);

    const auto stamp =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto staging =
        staging_directory /
        (package.filename().string() + "." + std::to_string(stamp) + ".tmp");

    std::error_code error;
    std::filesystem::copy_file(
        package,
        staging,
        std::filesystem::copy_options::none,
        error);

    if (error)
        throw std::runtime_error(
            "Failed to stage package: " + error.message());

    BackupInfo backup;
    bool has_backup = false;

    try {
        if (options.backup_existing && std::filesystem::exists(target)) {
            backup = BackupManager::create(target, backup_directory);
            has_backup = true;
        }

        std::filesystem::remove(target, error);
        if (error)
            throw std::runtime_error(
                "Failed to prepare installation target: " + error.message());

        std::filesystem::rename(staging, target, error);
        if (error)
            throw std::runtime_error(
                "Failed to activate staged package: " + error.message());

        return has_backup ? backup.backup : std::filesystem::path{};
    } catch (...) {
        std::error_code cleanup_error;
        std::filesystem::remove(staging, cleanup_error);

        if (has_backup && options.automatic_rollback) {
            try {
                BackupManager::rollback(backup.backup, target);
            } catch (const std::exception& rollback_error) {
                throw UpdateError(
                    ErrorCode::RollbackFailed,
                    "Installation failed and automatic rollback failed: " +
                    std::string(rollback_error.what()));
            }
        }

        throw;
    }
}

} // namespace


std::vector<UpdateReport> Updater::update_selected(
    const SelectedUpdateRequest& request) {

    if (request.updates.empty())
        throw UpdateError(
            ErrorCode::InvalidArgument,
            "No available updates were supplied.");

    if (request.selection.empty())
        throw UpdateError(
            ErrorCode::InvalidArgument,
            "No updates were selected.");

    if (request.destination.empty())
        throw UpdateError(
            ErrorCode::InvalidArgument,
            "Update destination cannot be empty.");

    std::vector<const AvailableUpdate*> selected_updates;
    selected_updates.reserve(request.selection.components.size());

    for (const auto& component : request.selection.components) {
        const auto match = std::find_if(
            request.updates.begin(),
            request.updates.end(),
            [&](const AvailableUpdate& update) {
                return update.component == component;
            });

        if (match == request.updates.end())
            throw UpdateError(
                ErrorCode::InvalidArgument,
                "Selected update is not present in discovery results: " +
                component);

        selected_updates.push_back(&*match);
    }

    std::vector<UpdateReport> reports;
    reports.reserve(selected_updates.size());

    const auto staging_directory =
        request.destination / ".openupdater" / "staging";
    std::filesystem::create_directories(staging_directory);

    for (const auto* update : selected_updates) {
        const auto stamp =
            std::chrono::high_resolution_clock::now()
                .time_since_epoch().count();

        const auto package_path =
            staging_directory /
            (update->asset + "." + std::to_string(stamp) + ".download");

        const auto digest = update->sha256;

        const auto signature_path =
            staging_directory /
            (update->asset + "." + std::to_string(stamp) + ".sig");

        HttpHeaders headers(
            request.options.headers.begin(),
            request.options.headers.end());

        try {
            Downloader::download(
                update->download_url,
                package_path,
                headers);

            if (request.options.verify_download && !digest.empty() &&
                !verify_sha256(package_path, digest)) {
                std::error_code cleanup_error;
                std::filesystem::remove(package_path, cleanup_error);
                throw UpdateError(
                    ErrorCode::VerificationFailed,
                    "SHA-256 verification failed for selected update: " +
                    update->component);
            }

            if (request.options.verify_signature) {
                if (request.options.trusted_public_key.empty())
                    throw UpdateError(
                        ErrorCode::VerificationFailed,
                        "Trusted Ed25519 public key is required for signature verification.");

                if (update->signature_url.empty())
                    throw UpdateError(
                        ErrorCode::VerificationFailed,
                        "Selected update has no detached signature asset: " +
                        update->component);

                Downloader::download(
                    update->signature_url,
                    signature_path,
                    headers);

                const auto signature = read_signature(signature_path);
                if (!SignatureVerifier::verify_ed25519_sha256(
                        package_path,
                        signature,
                        request.options.trusted_public_key)) {
                    throw UpdateError(
                        ErrorCode::VerificationFailed,
                        "Ed25519 signature verification failed for selected update: " +
                        update->component);
                }
            }

            const auto install_digest =
                request.options.verify_download ? digest : std::string{};

            const auto backup = install_package(
                package_path,
                request.destination,
                install_digest,
                request.options);

            std::error_code cleanup_error;
            std::filesystem::remove(package_path, cleanup_error);

            reports.push_back({
                update->current,
                update->available,
                UpdateState::UpdateAvailable,
                request.destination / update->asset,
                backup
            });
        } catch (const UpdateError&) {
            std::error_code cleanup_error;
            std::filesystem::remove(package_path, cleanup_error);
            std::filesystem::remove(signature_path, cleanup_error);
            throw;
        } catch (const std::exception& error) {
            std::error_code cleanup_error;
            std::filesystem::remove(package_path, cleanup_error);
            std::filesystem::remove(signature_path, cleanup_error);
            throw UpdateError(
                ErrorCode::InstallationFailed,
                "Failed to install selected update " +
                update->component + ": " + error.what());
        }
    }

    return reports;
}

std::filesystem::path Updater::install(
    const std::filesystem::path& package,
    const std::filesystem::path& destination,
    const std::string& expected_sha256) {

    return install_package(
        package,
        destination,
        expected_sha256,
        UpdateOptions{});
}

UpdateResult Updater::update_from_github(
    const Version& current,
    const std::string& repository,
    const std::string& asset_name,
    const std::filesystem::path& destination,
    const std::string& expected_sha256) {

    if (!current.valid())
        throw std::runtime_error("Current version is invalid.");

    const auto release =
        GitHubReleasesProvider::latest(repository, asset_name);

    if (release.version <= current)
        return {current, release.version, UpdateState::UpToDate, {}};

    const auto staging_directory =
        destination / ".openupdater" / "staging";
    std::filesystem::create_directories(staging_directory);

    const auto stamp =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto package_path =
        staging_directory /
        (asset_name + "." + std::to_string(stamp) + ".download");

    const auto digest = expected_sha256.empty()
        ? release.sha256
        : expected_sha256;

    try {
        GitHubReleasesProvider::download_latest(
            repository,
            asset_name,
            package_path,
            digest);

        const auto backup = install(package_path, destination, digest);

        std::error_code cleanup_error;
        std::filesystem::remove(package_path, cleanup_error);
        std::filesystem::remove(signature_path, cleanup_error);

        return {
            current,
            release.version,
            UpdateState::UpdateAvailable,
            backup
        };
    } catch (...) {
        std::error_code cleanup_error;
        std::filesystem::remove(package_path, cleanup_error);
        throw;
    }
}

UpdateReport Updater::update(const UpdateRequest& request) {
    if (!request.current.valid())
        throw UpdateError(
            ErrorCode::InvalidVersion,
            "Current version is invalid.");

    if (request.repository.empty() ||
        request.repository.find('/') == std::string::npos) {
        throw UpdateError(
            ErrorCode::InvalidArgument,
            "GitHub repository must use the owner/repository format.");
    }

    if (request.asset_name.empty())
        throw UpdateError(
            ErrorCode::InvalidArgument,
            "GitHub asset name cannot be empty.");

    if (request.destination.empty())
        throw UpdateError(
            ErrorCode::InvalidArgument,
            "Update destination cannot be empty.");

    GitHubRelease release;
    try {
        release = GitHubReleasesProvider::latest(
            request.repository,
            request.asset_name,
            request.options.headers);
    } catch (const std::exception& error) {
        throw UpdateError(
            ErrorCode::UpdateCheckFailed,
            error.what());
    }

    if (release.version <= request.current) {
        return {
            request.current,
            release.version,
            UpdateState::UpToDate,
            {},
            {}
        };
    }

    const auto staging_directory =
        request.destination / ".openupdater" / "staging";
    std::filesystem::create_directories(staging_directory);

    const auto stamp =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto package_path =
        staging_directory /
        (request.asset_name + "." + std::to_string(stamp) + ".download");

    const auto digest = request.expected_sha256.empty()
        ? release.sha256
        : request.expected_sha256;

    HttpHeaders headers(
        request.options.headers.begin(),
        request.options.headers.end());

    try {
        Downloader::download(
            release.download_url,
            package_path,
            headers);
    } catch (const std::exception& error) {
        std::error_code cleanup_error;
        std::filesystem::remove(package_path, cleanup_error);
        throw UpdateError(
            ErrorCode::DownloadFailed,
            error.what());
    }

    if (request.options.verify_download && !digest.empty() &&
        !verify_sha256(package_path, digest)) {
        std::error_code cleanup_error;
        std::filesystem::remove(package_path, cleanup_error);
        throw UpdateError(
            ErrorCode::VerificationFailed,
            "Downloaded GitHub asset SHA-256 verification failed.");
    }

    const auto signature_path =
        staging_directory /
        (request.asset_name + "." + std::to_string(stamp) + ".sig");

    if (request.options.verify_signature) {
        if (request.options.trusted_public_key.empty()) {
            std::error_code cleanup_error;
            std::filesystem::remove(package_path, cleanup_error);
            throw UpdateError(
                ErrorCode::VerificationFailed,
                "Trusted Ed25519 public key is required for signature verification.");
        }

        if (release.signature_url.empty()) {
            std::error_code cleanup_error;
            std::filesystem::remove(package_path, cleanup_error);
            throw UpdateError(
                ErrorCode::VerificationFailed,
                "GitHub release has no detached signature asset.");
        }

        try {
            Downloader::download(
                release.signature_url,
                signature_path,
                headers);

            const auto signature = read_signature(signature_path);
            if (!SignatureVerifier::verify_ed25519_sha256(
                    package_path,
                    signature,
                    request.options.trusted_public_key)) {
                std::error_code cleanup_error;
                std::filesystem::remove(package_path, cleanup_error);
                std::filesystem::remove(signature_path, cleanup_error);
                throw UpdateError(
                    ErrorCode::VerificationFailed,
                    "Downloaded GitHub asset Ed25519 signature verification failed.");
            }
        } catch (const UpdateError&) {
            throw;
        } catch (const std::exception& error) {
            std::error_code cleanup_error;
            std::filesystem::remove(package_path, cleanup_error);
            std::filesystem::remove(signature_path, cleanup_error);
            throw UpdateError(
                ErrorCode::VerificationFailed,
                error.what());
        }
    }

    try {
        const auto install_digest =
            request.options.verify_download ? digest : std::string{};

        const auto backup = install_package(
            package_path,
            request.destination,
            install_digest,
            request.options);

        std::error_code cleanup_error;
        std::filesystem::remove(package_path, cleanup_error);

        return {
            request.current,
            release.version,
            UpdateState::UpdateAvailable,
            request.destination / request.asset_name,
            backup
        };
    } catch (const UpdateError&) {
        std::error_code cleanup_error;
        std::filesystem::remove(package_path, cleanup_error);
        throw;
    } catch (const std::exception& error) {
        std::error_code cleanup_error;
        std::filesystem::remove(package_path, cleanup_error);
        throw UpdateError(
            ErrorCode::InstallationFailed,
            error.what());
    }
}

} // namespace openupdater
