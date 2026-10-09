#pragma once

#include <filesystem>
#include <string>
#include <string_view>

// User-tunable values that persist between runs. Stored as plain `key=value` lines.
// Unknown keys, comments (#), and unparsable or non-finite values are ignored, so a hand-edited
// or older file never breaks startup: anything missing just keeps its default.
struct Settings {
    float moveSpeed        = 3.0f;    // world units per second
    float mouseSensitivity = 0.0018f; // radians per raw mouse count

    // $XDG_CONFIG_HOME/forge3d/settings.cfg, else ~/.config/forge3d/settings.cfg
    static std::filesystem::path defaultPath();

    static Settings parse(std::string_view pText);
    std::string     serialize() const;

    // load: a missing/unreadable file gives defaults. save: creates the folder; false on failure.
    static Settings load(const std::filesystem::path& pPath);
    bool            save(const std::filesystem::path& pPath) const;
};
