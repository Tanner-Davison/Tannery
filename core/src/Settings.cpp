#include "Settings.hpp"

#include <cmath>
#include <cstdlib>
#include <format>
#include <fstream>
#include <sstream>

namespace {
// Parses a whole string as a finite float. Returns false for "", "abc", "1.0x", "nan", "inf".
bool parseFloat(const std::string& pText, float& pOut) {
    if (pText.empty()) {
        return false;
    }
    char*       end   = nullptr;
    const float value = std::strtof(pText.c_str(), &end);
    if (end != pText.c_str() + pText.size() || !std::isfinite(value)) {
        return false;
    }
    pOut = value;
    return true;
}

std::string trim(const std::string& pText) {
    const auto first = pText.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = pText.find_last_not_of(" \t\r");
    return pText.substr(first, last - first + 1);
}
} // namespace

std::filesystem::path Settings::defaultPath() {
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && *xdg != '\0') {
        return std::filesystem::path(xdg) / "forge3d" / "settings.cfg";
    }
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return std::filesystem::path(home) / ".config" / "forge3d" / "settings.cfg";
    }
    return "settings.cfg"; // no home directory known: fall back to the working directory
}

Settings Settings::parse(std::string_view pText) {
    Settings           result;
    std::istringstream stream{std::string(pText)};
    std::string        line;
    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const std::string key   = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1));

        if (key == "moveSpeed") {
            parseFloat(value, result.moveSpeed);
        } else if (key == "mouseSensitivity") {
            parseFloat(value, result.mouseSensitivity);
        }
        // unknown keys are ignored on purpose (forward/backward compatibility)
    }
    return result;
}

std::string Settings::serialize() const {
    // {} on a float prints the shortest text that round-trips to the same value
    return std::format("# Forge3D settings\nmoveSpeed={}\nmouseSensitivity={}\n",
                       moveSpeed,
                       mouseSensitivity);
}

Settings Settings::load(const std::filesystem::path& pPath) {
    std::ifstream file(pPath);
    if (!file) {
        return {};
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse(buffer.str());
}

bool Settings::save(const std::filesystem::path& pPath) const {
    std::error_code ec;
    if (pPath.has_parent_path()) {
        std::filesystem::create_directories(pPath.parent_path(), ec);
        if (ec) {
            return false;
        }
    }
    std::ofstream file(pPath, std::ios::trunc);
    if (!file) {
        return false;
    }
    file << serialize();
    return static_cast<bool>(file);
}
