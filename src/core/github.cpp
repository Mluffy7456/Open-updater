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
    const std::string& asset_name) {

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

        const HttpHeaders headers{
            {"Accept", "application/vnd.github+json"},
            {"X-GitHub-Api-Version", "2026-03-10"},
            {"User-Agent", "OpenUpdater"}};

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
            return GitHubRelease{version, tag, name, url, digest};
        }

        throw std::runtime_error(
            "GitHub release asset not found: " + asset_name);
    } catch (...) {
        cleanup();
        throw;
    }
}

GitHubRelease GitHubReleasesProvider::download_latest(
    const std::string& repository,
    const std::string& asset_name,
    const std::filesystem::path& destination,
    const std::string& expected_sha256) {

    const auto release = latest(repository, asset_name);
    Downloader::download(release.download_url, destination);

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
