// ConfigManager.cpp - Application configuration manager
// Uses nlohmann/json for JSON file I/O and typed access.
// CPP-parsers (IConfigParser, JsonConfigParser) is compiled alongside
// this module for future multi-format support.
//
// The data is stored as UTF-8 text in JSON format with 2-space indent.
// Key resolution supports dotted notation (e.g., "display.refreshRate").

#include "ConfigManager.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>

// =========================================================================
// Helper: split dotted key "foo.bar.baz" -> ["foo", "bar", "baz"]
// =========================================================================
static std::vector<std::string> SplitKey(const std::string& key) {
    std::vector<std::string> parts;
    std::stringstream ss(key);
    std::string part;
    while (std::getline(ss, part, '.')) {
        if (!part.empty())
            parts.push_back(part);
    }
    return parts;
}

// =========================================================================
// Construction / Loading / Saving
// =========================================================================

ConfigManager::ConfigManager(const std::string& path)
    : path_(path)
{
}

bool ConfigManager::Load() {
    std::ifstream in(path_);
    if (!in.is_open()) {
        // File doesn't exist yet — that's fine, start with empty config
        data_ = nlohmann::json::object();
        loaded_ = true;
        return true;
    }

    try {
        in >> data_;
        loaded_ = true;
        return true;
    } catch (const nlohmann::json::parse_error&) {
        // Corrupt JSON file — warn and reset
        data_ = nlohmann::json::object();
        loaded_ = true;
        return false;
    }
}

bool ConfigManager::Save() const {
    if (!loaded_) {
        // Nothing to save — config was never loaded
        return false;
    }
    try {
        std::ofstream out(path_);
        if (!out.is_open()) return false;
        out << data_.dump(2) << std::endl;
        return true;
    } catch (...) {
        return false;
    }
}

// =========================================================================
// Key resolution (dotted notation)
// =========================================================================

const nlohmann::json* ConfigManager::ResolveKey(
    const std::string& key) const
{
    auto parts = SplitKey(key);
    if (parts.empty()) {
        return nullptr;
    }

    // Walk the JSON tree
    const nlohmann::json* current = &data_;
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        if (!current->is_object()) return nullptr;
        auto it = current->find(parts[i]);
        if (it == current->end() || !it->is_object()) {
            return nullptr;
        }
        current = &it.value();
    }

    // Final part — use pointer check instead of iterator comparison
    if (!current->is_object()) return nullptr;
    auto it = current->find(parts.back());
    if (it == current->end()) return nullptr;
    // Return pointer to the found value (same container as current, safe)
    return &it.value();
}

// =========================================================================
// Getters
// =========================================================================

std::string ConfigManager::GetString(const std::string& key,
                                     const std::string& defaultVal) const
{
    if (!loaded_) return defaultVal;
    auto* val = ResolveKey(key);
    if (val == nullptr || !val->is_string())
        return defaultVal;
    return val->get<std::string>();
}

int ConfigManager::GetInt(const std::string& key, int defaultVal) const {
    if (!loaded_) return defaultVal;
    auto* val = ResolveKey(key);
    if (val == nullptr || !val->is_number_integer())
        return defaultVal;
    return val->get<int>();
}

double ConfigManager::GetDouble(const std::string& key,
                                double defaultVal) const {
    if (!loaded_) return defaultVal;
    auto* val = ResolveKey(key);
    if (val == nullptr || !val->is_number_float())
        return defaultVal;
    return val->get<double>();
}

bool ConfigManager::GetBool(const std::string& key,
                            bool defaultVal) const {
    if (!loaded_) return defaultVal;
    auto* val = ResolveKey(key);
    if (val == nullptr || !val->is_boolean())
        return defaultVal;
    return val->get<bool>();
}

// =========================================================================
// Setters
// =========================================================================

static void SetNested(nlohmann::json& root,
                      const std::string& key,
                      nlohmann::json&& value)
{
    auto parts = SplitKey(key);
    if (parts.empty()) return;

    nlohmann::json* current = &root;
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        if (!current->is_object()) {
            *current = nlohmann::json::object();
        }
        auto it = current->find(parts[i]);
        if (it == current->end() || !it->is_object()) {
            (*current)[parts[i]] = nlohmann::json::object();
        }
        current = &(*current)[parts[i]];
    }
    (*current)[parts.back()] = std::move(value);
}

void ConfigManager::SetString(const std::string& key,
                              const std::string& value) {
    SetNested(data_, key, value);
}

void ConfigManager::SetInt(const std::string& key, int value) {
    SetNested(data_, key, value);
}

void ConfigManager::SetDouble(const std::string& key, double value) {
    SetNested(data_, key, value);
}

void ConfigManager::SetBool(const std::string& key, bool value) {
    SetNested(data_, key, value);
}
