#include "openupdater/providers/winget.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace openupdater {

namespace {

#ifdef _WIN32

struct CommandResult {
    int exit_code = -1;
    std::string output;
};

CommandResult run_winget(const std::string& arguments) {
    const std::string command =
        "winget.exe " + arguments + " 2>&1";

    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe)
        throw std::runtime_error("Unable to start winget.exe.");

    std::string output;
    char buffer[4096];
    while (std::fgets(buffer, sizeof(buffer), pipe))
        output += buffer;

    const int exit_code = _pclose(pipe);
    return {exit_code, output};
}

std::string trim(std::string value) {
    const auto not_space = [](unsigned char c) {
        return !std::isspace(c);
    };

    value.erase(
        value.begin(),
        std::find_if(value.begin(), value.end(), not_space));
    value.erase(
        std::find_if(value.rbegin(), value.rend(), not_space).base(),
        value.end());
    return value;
}

bool is_separator(const std::string& line) {
    const auto first = line.find_first_not_of(" -\r\n\t");
    return first == std::string::npos;
}

bool valid_id(const std::string& id) {
    if (id.empty())
        return false;

    return std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '.' || c == '-' || c == '_';
    });
}

UpdateCategory classify(const std::string& id, const std::string& name) {
    const auto lower = [](std::string value) {
        std::transform(
            value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    };

    const auto text = lower(id + " " + name);

    static constexpr const char* programming_markers[] = {
        "python", "nodejs", "node.js", "golang", " go ",
        "rust", "cargo", "dotnet", ".net", "visualstudio",
        "visual studio", "git.git", "cmake", "llvm", "clang",
        "mingw", "msys2", "powershell", "jdk", "openjdk",
        "java", "npm", "yarn", "pnpm", "vscode", "visualstudiocode"
    };

    for (const auto* marker : programming_markers) {
        if (text.find(marker) != std::string::npos)
            return UpdateCategory::Programming;
    }

    return UpdateCategory::Application;
}

std::vector<DiscoveredUpdate> parse_upgrade_table(const std::string& output) {
    std::istringstream input(output);
    std::string line;
    std::size_t name_column = 0;
    std::size_t id_column = std::string::npos;
    std::size_t version_column = std::string::npos;
    std::size_t available_column = std::string::npos;
    std::size_t source_column = std::string::npos;
    bool header_found = false;
    std::vector<DiscoveredUpdate> result;

    while (std::getline(input, line)) {
        if (!header_found &&
            line.find("Name") != std::string::npos &&
            line.find("Id") != std::string::npos &&
            line.find("Version") != std::string::npos &&
            line.find("Available") != std::string::npos &&
            line.find("Source") != std::string::npos) {
            name_column = line.find("Name");
            id_column = line.find("Id");
            version_column = line.find("Version");
            available_column = line.find("Available");
            source_column = line.find("Source");
            header_found = true;
            continue;
        }

        if (!header_found || is_separator(line) ||
            line.find("upgrades available") != std::string::npos ||
            line.find("upgrade available") != std::string::npos) {
            continue;
        }

        if (line.size() <= source_column)
            continue;

        const auto slice = [&](std::size_t begin, std::size_t end) {
            if (begin >= line.size())
                return std::string{};
            end = std::min(end, line.size());
            return trim(line.substr(begin, end - begin));
        };

        const auto name = slice(name_column, id_column);
        const auto id = slice(id_column, version_column);
        const auto current = slice(version_column, available_column);
        const auto available = slice(available_column, source_column);
        const auto source = trim(line.substr(source_column));

        if (name.empty() || !valid_id(id) ||
            current.empty() || available.empty())
            continue;

        DiscoveredUpdate update;
        update.id = id;
        update.name = name;
        update.current_version = current;
        update.available_version = available;
        update.category = classify(id, name);
        update.source = UpdateSource::WinGet;
        update.provider = "WinGet";
        update.platform = "Windows";
        (void)source;
        result.push_back(std::move(update));
    }

    return result;
}

#endif

} // namespace

std::string WinGetProvider::name() const {
    return "WinGet";
}

bool WinGetProvider::available() const {
#ifdef _WIN32
    const auto result = run_winget(
        "--version --disable-interactivity");
    return result.exit_code == 0 && !result.output.empty();
#else
    return false;
#endif
}

std::vector<DiscoveredUpdate> WinGetProvider::check_updates() {
#ifdef _WIN32
    const auto result = run_winget(
        "upgrade --source winget --locale en-US "
        "--accept-source-agreements --disable-interactivity");

    if (result.exit_code != 0) {
        throw std::runtime_error(
            "winget upgrade failed: " + trim(result.output));
    }

    return parse_upgrade_table(result.output);
#else
    return {};
#endif
}

void WinGetProvider::install(const DiscoveredUpdate& update) {
#ifdef _WIN32
    if (update.source != UpdateSource::WinGet ||
        update.provider != name() ||
        !valid_id(update.id)) {
        throw std::invalid_argument("Invalid WinGet update identifier.");
    }

    const auto result = run_winget(
        "upgrade --id "" + update.id +
        "" --exact --source winget --locale en-US "
        "--accept-source-agreements --accept-package-agreements "
        "--disable-interactivity");

    if (result.exit_code != 0) {
        throw std::runtime_error(
            "winget upgrade failed for " + update.id + ": " +
            trim(result.output));
    }
#else
    (void)update;
    throw std::runtime_error("WinGet is only available on Windows.");
#endif
}

} // namespace openupdater
