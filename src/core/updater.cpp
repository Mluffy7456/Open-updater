#include "openupdater/core/updater.hpp"

#include <array>
#include <filesystem>
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
    const std::filesystem::path& destination) {

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
        L"OpenUpdater/0.2.0",
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

    if (!WinHttpSendRequest(
            request,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
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
    const std::filesystem::path& destination) {

    if (url.empty())
        throw std::runtime_error("Download URL cannot be empty.");

    if (destination.empty())
        throw std::runtime_error("Download destination cannot be empty.");

    if (destination.has_parent_path())
        std::filesystem::create_directories(destination.parent_path());

#ifdef _WIN32
    download_windows(url, destination);
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
    curl_easy_cleanup(curl);

    if (result != CURLE_OK)
        throw std::runtime_error(
            std::string("Download failed: ") + curl_easy_strerror(result));
#endif
}

UpdateCheck Updater::check(const Version& current, const Manifest& manifest) {
    if (!current.valid())
        throw std::runtime_error("Current version is invalid.");

    if (manifest.version > current)
        return {current, manifest.version, UpdateState::UpdateAvailable};

    return {current, manifest.version, UpdateState::UpToDate};
}

void Updater::install(
    const std::filesystem::path& package,
    const std::filesystem::path& destination) {

    if (!std::filesystem::exists(package))
        throw std::runtime_error("Package does not exist: " + package.string());

    std::filesystem::create_directories(destination);

    const auto target = destination / package.filename();
    std::error_code ec;

    std::filesystem::copy_file(
        package,
        target,
        std::filesystem::copy_options::overwrite_existing,
        ec);

    if (ec)
        throw std::runtime_error("Failed to install package: " + ec.message());
}

} // namespace openupdater
