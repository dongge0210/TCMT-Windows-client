// ConfigManager.h - Application configuration manager
// Wraps nlohmann/json with typed access, defaults, and JSON file I/O.
// CPP-parsers IConfigParser integration point: this class can be refactored
// to use IConfigParser/JsonConfigParser for multi-format config support.
//
// For macOS, CPP-parsers (IConfigParser.h, JsonConfigParser.h) is compiled
// and available as part of TCMTCore. See src/CMakeLists.txt.

#pragma once

#include <string>
#include <nlohmann/json.hpp>

class ConfigManager {
public:
    /// Construct with path to config file (default: "config.json")
    explicit ConfigManager(const std::string& path = "config.json");

    /// Load config from file. Returns true on success.
    bool Load();

    /// Save current config to file. Returns true on success.
    bool Save() const;

    // ---- Typed getters with defaults ----

    /// Get a string value. Returns defaultVal if key not found.
    std::string GetString(const std::string& key,
                          const std::string& defaultVal = "") const;

    /// Get an integer value. Returns defaultVal if key missing or not a number.
    int GetInt(const std::string& key, int defaultVal = 0) const;

    /// Get a double value. Returns defaultVal if key missing or not a number.
    double GetDouble(const std::string& key, double defaultVal = 0.0) const;

    /// Get a boolean value. Returns defaultVal if key missing.
    bool GetBool(const std::string& key, bool defaultVal = false) const;

    // ---- Typed setters ----

    void SetString(const std::string& key, const std::string& value);
    void SetInt(const std::string& key, int value);
    void SetDouble(const std::string& key, double value);
    void SetBool(const std::string& key, bool value);

    /// Returns true if Load() has been called successfully.
    bool IsLoaded() const { return loaded_; }

    /// The config file path (resolved).
    const std::string& GetPath() const { return path_; }

private:
    /// Resolve a dotted key like "display.refreshRate" into nested json pointers.
    /// Returns pointer to the value, or nullptr if not found.
    /// Uses pointer (not iterator) to avoid cross-container comparison issues.
    const nlohmann::json* ResolveKey(const std::string& key) const;

    nlohmann::json data_;
    std::string    path_;
    bool           loaded_ = false;
};
