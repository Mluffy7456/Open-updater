#include "openupdater/core/github.hpp"

#include "openupdater/core/sha256.hpp"
#include "openupdater/core/updater.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>

namespace openupdater {

namespace {

std::string decode_json_string(std::string_view value) {
    std::string result;
    result.reserve(value.size());

    bool escaped = false;
    for (const char character : value) {
        if (!escaped) {
            if (character == '\\')
                escaped = true;
            else
                result += character;
            continue;
        }

        switch (character) {
        case '"': result += '"'; break;
        case '\\': result += '\\'; break;
        case '/': result += '/'; break;
        case 'b': result += '\b'; break;
        case 'f': result += '\f'; break;
        case 'n': result += '\n'; break;
        case 'r': result += '\r'; break;
        case 't': result += '\t'; break;
        default:
            throw std::runtime_error("Unsupported JSON escape sequence.");
        }

        escaped = false;
    }

    if (escaped)
        throw std::runtime_error("Invalid JSON string escape.");

    return result;
}

std::string json_string(const std::string& json, std::string_view key,
                        std::size_t start = 0) {
    const auto key_position = json.find(
        std::string("\"") + std::string(key) + "\"", start);

    if (key_position == std::string::npos)
        return {};

    auto position = json.find(':', key_position + key.size() + 2);
    if (position == std::string::npos)
        return {};

    ++position;
    while (position < json.size() &&
           (json[position] == ' ' || json[position] == '\n' ||
            json[position] == '\r' || json[position] == '\t')) {
        ++position;
    }

    if (position >= json.size() || json[position] != '"')
        return {};

    ++position;
    const auto begin = position;
    bool escaped = false;

    while (position < json.size()) {
        if (!escaped && json[position] == '"')
            return decode_json_string(
                std::string_view(json).substr(begin, position - begin));

        if (!escaped && json[position] == '\\')
            escaped = true;
        else
            escaped = false;

        ++position;
    }

    return {};
}

std::vector<std::string_view> asset_objects(const std::string& json) {
    const auto assets_key = json.find("\"assets\"");
    if (assets_key == std::string::npos)
        throw std::runtime_error("GitHub release response has no assets.");

    const auto array_start = json.find('[', assets_key);
    if (array_start == std::string::npos)
        throw std::runtime_error("GitHub release assets are invalid.");

    std::vector<std::string_view> objects;

    bool in_string = false;
    bool escaped = false;
    std::size_t object_start = std::string::npos;
    unsigned depth = 0;

    for (std::size_t i = array_start + 1; i < json.size(); ++i) {
        const char character = json[i];

        if (in_string) {
            if (!escaped && character == '"')
                in_string = false;
            else if (!escaped && character == '\\')
                escaped = true;
            else
                escaped = false;
            continue;
        }

        if (character == '"') {
            in_string = true;
            continue;
        }

        if (character == '{') {
            if (depth == 0)
                object_start = i;
            ++depth;
            continue;
        }

        if (character == '}') {
            if (depth == 0)
                throw std::runtime_error("Invalid GitHub release JSON.");

            --depth;
            if (depth == 0)
                objects.emplace_back(
                    json.data() + object_start,
                    i - object_start + 1);
            continue;
        }

        if (character == ']' && depth == 0)
            break;
    }

    return objects;
}

std::string signature_url_for_asset(
    const std::string& json,
    const std::string& asset_name) {

    const auto signature_name = asset_name + ".sig";

    for (const auto object : asset_objects(json)) {
        const std::string object_json(object);
        if (json_string(object_json, "name") != signature_name)
            continue;

        return json_string(object_json, "browser_download_url");
    }

    return {};
}

void validate_repository(const std::string& repository) {
    const auto slash = repository.find('/');
    if (slash == std::string::npos || slash == 0 ||
        slash + 1 >= repository.size() ||
        repository.find('/', slash + 1) != std::string::npos) {
        throw std::invalid_argument(
            "GitHub repository must use the owner/repository format.");
    }
}

std::filesystem::path make_temp_path() {
    const auto now =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();

    return std::filesystem::temp_directory_path() /
        ("openupdater_github_" + std::to_string(now) + ".json");
}

} // namespace

GitHubRelease GitHubReleasesProvider::latest(
    const std::string& repository,
    const std::string& asset_name,
    const ApiHttpHeaders& custom_headers) {

    validate_repository(repository);

    if (asset_name.empty())
        throw std::invalid_argument("GitHub asset name cannot be empty.");

    const auto metadata_path = make_temp_path();
    const auto cleanup = [&]() {
        std::error_code error;
        std::filesystem::remove(metadata_path, error);
    };

    try {
        const auto api_url =
            "https://api.github.com/repos/" + repository + "/releases/latest";

        HttpHeaders headers{
            {"Accept", "application/vnd.github+json"},
            {"X-GitHub-Api-Version", "2026-03-10"},
            {"User-Agent", "OpenUpdater/2.0"}};
        headers.insert(headers.end(), custom_headers.begin(), custom_headers.end());

        Downloader::download(api_url, metadata_path, headers);

        std::ifstream input(metadata_path, std::ios::binary);
        if (!input)
            throw std::runtime_error("Cannot open GitHub release metadata.");

        const std::string json{
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};

        const auto tag = json_string(json, "tag_name");
        if (tag.empty())
            throw std::runtime_error("GitHub release has no tag_name.");

        const Version version(tag);
        if (!version.valid())
            throw std::runtime_error(
                "GitHub release tag is not a valid version: " + tag);

        for (const auto object : asset_objects(json)) {
            const std::string object_json(object);
            const auto name = json_string(object_json, "name");
            if (name != asset_name)
                continue;

            const auto url = json_string(
                object_json, "browser_download_url");
            if (url.empty())
                throw std::runtime_error(
                    "GitHub asset has no browser_download_url.");

            auto digest = json_string(object_json, "digest");
            if (digest.rfind("sha256:", 0) == 0)
                digest.erase(0, 7);
            else
                digest.clear();

            cleanup();
            return GitHubRelease{version, tag, name, url, digest, signature_url_for_asset(json, name)};
        }

        throw std::runtime_error(
            "GitHub release asset not found: " + asset_name);
    } catch (...) {
        cleanup();
        throw;
    }
}



std::vector<AvailableUpdate> GitHubReleasesProvider::discover(
    const std::string& repository,
    const std::vector<UpdateTarget>& targets,
    Platform platform,
    Architecture architecture,
    const ApiHttpHeaders& custom_headers) {

    validate_repository(repository);

    if (targets.empty())
        return {};

    if (platform == Platform::Unknown || architecture == Architecture::Unknown)
        throw std::invalid_argument(
            "Cannot discover compatible updates for an unknown platform or architecture.");

    for (const auto& target : targets) {
        if (target.component.empty())
            throw std::invalid_argument(
                "Update component name cannot be empty.");
        if (!target.current.valid())
            throw std::invalid_argument(
                "Update target has an invalid current version.");
    }

    const auto metadata_path = make_temp_path();
    const auto cleanup = [&]() {
        std::error_code error;
        std::filesystem::remove(metadata_path, error);
    };

    try {
        const auto api_url =
            "https://api.github.com/repos/" + repository + "/releases/latest";

        HttpHeaders headers{
            {"Accept", "application/vnd.github+json"},
            {"X-GitHub-Api-Version", "2026-03-10"},
            {"User-Agent", "OpenUpdater/2.0"}};
        headers.insert(headers.end(), custom_headers.begin(), custom_headers.end());

        Downloader::download(api_url, metadata_path, headers);

        std::ifstream input(metadata_path, std::ios::binary);
        if (!input)
            throw std::runtime_error("Cannot open GitHub release metadata.");

        const std::string json{
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};

        const auto tag = json_string(json, "tag_name");
        if (tag.empty())
            throw std::runtime_error("GitHub release has no tag_name.");

        const Version version(tag);
        if (!version.valid())
            throw std::runtime_error(
                "GitHub release tag is not a valid version: " + tag);

        std::vector<AvailableUpdate> result;

        for (const auto& target : targets) {
            const std::string prefix = target.component + "-";
            std::vector<GitHubRelease> matches;

            for (const auto object : asset_objects(json)) {
                const std::string object_json(object);
                const auto name = json_string(object_json, "name");

                if (name.size() <= prefix.size() ||
                    name.compare(0, prefix.size(), prefix) != 0 ||
                    !asset_matches_platform(name, platform, architecture)) {
                    continue;
                }

                const auto url =
                    json_string(object_json, "browser_download_url");
                if (url.empty())
                    throw std::runtime_error(
                        "GitHub compatible asset has no browser_download_url.");

                auto digest = json_string(object_json, "digest");
                if (digest.rfind("sha256:", 0) == 0)
                    digest.erase(0, 7);
                else
                    digest.clear();

                matches.push_back({version, tag, name, url, digest, signature_url_for_asset(json, name)});
            }

            if (matches.empty())
                continue;

            if (matches.size() > 1)
                throw std::runtime_error(
                    "Multiple compatible GitHub assets found for component " +
                    target.component + "; update discovery is ambiguous.");

            const auto& match = matches.front();
            result.push_back({
                target.component,
                target.current,
                match.version,
                match.asset,
                std::string(platform_name(platform)),
                std::string(architecture_name(architecture)),
                match.sha256,
                match.signature_url,
                match.download_url
            });
        }

        cleanup();
        return result;
    } catch (...) {
        cleanup();
        throw;
    }
}

GitHubRelease GitHubReleasesProvider::latest_compatible(
    const std::string& repository,
    const std::string& component,
    Platform platform,
    Architecture architecture,
    const ApiHttpHeaders& custom_headers) {

    validate_repository(repository);

    if (component.empty())
        throw std::invalid_argument("GitHub component name cannot be empty.");

    if (platform == Platform::Unknown || architecture == Architecture::Unknown)
        throw std::invalid_argument(
            "Cannot select a compatible asset for an unknown platform or architecture.");

    const auto metadata_path = make_temp_path();
    const auto cleanup = [&]() {
        std::error_code error;
        std::filesystem::remove(metadata_path, error);
    };

    try {
        const auto api_url =
            "https://api.github.com/repos/" + repository + "/releases/latest";

        HttpHeaders headers{
            {"Accept", "application/vnd.github+json"},
            {"X-GitHub-Api-Version", "2026-03-10"},
            {"User-Agent", "OpenUpdater/2.0"}};
        headers.insert(headers.end(), custom_headers.begin(), custom_headers.end());

        Downloader::download(api_url, metadata_path, headers);

        std::ifstream input(metadata_path, std::ios::binary);
        if (!input)
            throw std::runtime_error("Cannot open GitHub release metadata.");

        const std::string json{
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};

        const auto tag = json_string(json, "tag_name");
        if (tag.empty())
            throw std::runtime_error("GitHub release has no tag_name.");

        const Version version(tag);
        if (!version.valid())
            throw std::runtime_error(
                "GitHub release tag is not a valid version: " + tag);

        const std::string prefix = component + "-";
        std::vector<GitHubRelease> matches;

        for (const auto object : asset_objects(json)) {
            const std::string object_json(object);
            const auto name = json_string(object_json, "name");

            if (name.size() <= prefix.size() ||
                name.compare(0, prefix.size(), prefix) != 0 ||
                !asset_matches_platform(name, platform, architecture)) {
                continue;
            }

            const auto url = json_string(object_json, "browser_download_url");
            if (url.empty())
                throw std::runtime_error(
                    "GitHub compatible asset has no browser_download_url.");

            auto digest = json_string(object_json, "digest");
            if (digest.rfind("sha256:", 0) == 0)
                digest.erase(0, 7);
            else
                digest.clear();

            matches.push_back({version, tag, name, url, digest, signature_url_for_asset(json, name)});
        }

        if (matches.empty()) {
            throw std::runtime_error(
                "No compatible GitHub asset found for " +
                std::string(platform_name(platform)) + "-" +
                std::string(architecture_name(architecture)) +
                " and component " + component + ".");
        }

        if (matches.size() > 1) {
            throw std::runtime_error(
                "Multiple compatible GitHub assets found for component " +
                component + "; use the exact asset API.");
        }

        cleanup();
        return matches.front();
    } catch (...) {
        cleanup();
        throw;
    }
}

GitHubRelease GitHubReleasesProvider::download_latest(
    const std::string& repository,
    const std::string& asset_name,
    const std::filesystem::path& destination,
    const std::string& expected_sha256,
    const ApiHttpHeaders& custom_headers) {

    const auto release = latest(repository, asset_name, custom_headers);

    HttpHeaders headers{
        {"Accept", "application/octet-stream"},
        {"User-Agent", "OpenUpdater/2.0"}};
    headers.insert(headers.end(), custom_headers.begin(), custom_headers.end());
    Downloader::download(release.download_url, destination, headers);

    const auto digest = expected_sha256.empty()
        ? release.sha256
        : expected_sha256;

    if (!digest.empty() && !verify_sha256(destination, digest)) {
        std::error_code error;
        std::filesystem::remove(destination, error);
        throw std::runtime_error(
            "Downloaded GitHub asset SHA-256 verification failed.");
    }

    return release;
}

} // namespace openupdater
