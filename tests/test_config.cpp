#include <gtest/gtest.h>
#include "core/Config/ConfigManager.h"
#include <fstream>
#include <cstdio>

// Cross-platform temp file helper
#ifdef _WIN32
#include <windows.h>
#include <cstdlib>
static std::string WriteTempFile(const std::string& content) {
    char path[MAX_PATH + 1];
    char fname[MAX_PATH + 1];
    GetTempPathA(MAX_PATH, path);
    if (GetTempFileNameA(path, "tcm", 0, fname) == 0) return "";
    std::ofstream out(fname);
    out << content;
    out.close();
    return fname;
}
#else
#include <unistd.h>
static std::string WriteTempFile(const std::string& content) {
    char path[] = "/tmp/tcmt_test_XXXXXX";
    int fd = mkstemp(path);
    if (fd == -1) return "";
    close(fd);
    std::ofstream out(path);
    out << content;
    out.close();
    return path;
}
#endif

// ── Default / unloaded state ──
TEST(ConfigManagerTest, UnloadedReturnsDefaults) {
    ConfigManager cfg("/nonexistent/config.json");
    EXPECT_EQ(cfg.GetString("key", "default"), "default");
    EXPECT_EQ(cfg.GetInt("key", 42), 42);
    EXPECT_DOUBLE_EQ(cfg.GetDouble("key", 3.14), 3.14);
    EXPECT_EQ(cfg.GetBool("key", true), true);
    EXPECT_FALSE(cfg.IsLoaded());
}

// ── Load from file ──
TEST(ConfigManagerTest, LoadFromFile) {
    auto path = WriteTempFile(R"({"name":"test","count":5})");
    ConfigManager cfg(path);

    EXPECT_TRUE(cfg.Load());
    EXPECT_TRUE(cfg.IsLoaded());
    EXPECT_EQ(cfg.GetString("name", ""), "test");
    EXPECT_EQ(cfg.GetInt("count", 0), 5);

    std::remove(path.c_str());
}

// ── File not found => empty config, not crash ──
TEST(ConfigManagerTest, FileNotFound) {
    auto path = WriteTempFile("");
    std::remove(path.c_str());
    ConfigManager cfg(path);
    EXPECT_TRUE(cfg.Load()); // returns true with empty config
    EXPECT_TRUE(cfg.IsLoaded());
    EXPECT_EQ(cfg.GetString("anything", "fallback"), "fallback");
}

// ── Corrupt JSON ──
TEST(ConfigManagerTest, CorruptJson) {
    auto path = WriteTempFile("{invalid json!!!}");
    ConfigManager cfg(path);
    EXPECT_FALSE(cfg.Load()); // returns false on parse error
    EXPECT_FALSE(cfg.IsLoaded());
    // Should have empty data, all gets return defaults
    EXPECT_EQ(cfg.GetString("key", "default"), "default");

    std::remove(path.c_str());
}

// ── Nested dotted key ──
TEST(ConfigManagerTest, NestedKey) {
    auto path = WriteTempFile(R"({"display":{"refreshRate":144,"mode":"dark"}})");
    ConfigManager cfg(path);
    cfg.Load();

    EXPECT_EQ(cfg.GetInt("display.refreshRate", 60), 144);
    EXPECT_EQ(cfg.GetString("display.mode", "light"), "dark");
    EXPECT_EQ(cfg.GetString("display.nonexistent", "fallback"), "fallback");

    std::remove(path.c_str());
}

// ── Type safety ──
TEST(ConfigManagerTest, TypeSafety) {
    auto path = WriteTempFile(R"({"strKey":"hello","intKey":42,"floatKey":3.14,"boolKey":true})");
    ConfigManager cfg(path);
    cfg.Load();

    // Wrong type returns default
    EXPECT_EQ(cfg.GetInt("strKey", -1), -1);
    EXPECT_DOUBLE_EQ(cfg.GetDouble("strKey", -1.0), -1.0);
    EXPECT_EQ(cfg.GetBool("strKey", false), false);

    // Correct types succeed
    EXPECT_EQ(cfg.GetInt("intKey", -1), 42);
    EXPECT_DOUBLE_EQ(cfg.GetDouble("floatKey", -1.0), 3.14);
    EXPECT_EQ(cfg.GetBool("boolKey", false), true);

    std::remove(path.c_str());
}

// ── Set / Get roundtrip ──
TEST(ConfigManagerTest, SetGetRoundtrip) {
    auto path = WriteTempFile("{}");
    ConfigManager cfg(path);
    cfg.Load(); // start with empty config

    cfg.SetString("name", "TCMT");
    cfg.SetInt("version", 1);
    cfg.SetDouble("threshold", 0.85);
    cfg.SetBool("enabled", true);

    EXPECT_EQ(cfg.GetString("name", ""), "TCMT");
    EXPECT_EQ(cfg.GetInt("version", 0), 1);
    EXPECT_DOUBLE_EQ(cfg.GetDouble("threshold", 0.0), 0.85);
    EXPECT_EQ(cfg.GetBool("enabled", false), true);

    // Overwrite
    cfg.SetString("name", "TCMT-M");
    EXPECT_EQ(cfg.GetString("name", ""), "TCMT-M");

    std::remove(path.c_str());
}

// ── Nested set ──
TEST(ConfigManagerTest, SetNestedKey) {
    auto path = WriteTempFile("{}");
    ConfigManager cfg(path);
    cfg.Load();

    cfg.SetInt("display.refreshRate", 120);
    cfg.SetString("display.theme.mode", "dark");

    EXPECT_EQ(cfg.GetInt("display.refreshRate", 0), 120);
    EXPECT_EQ(cfg.GetString("display.theme.mode", ""), "dark");

    std::remove(path.c_str());
}

// ── Save and reload ──
TEST(ConfigManagerTest, SaveAndReload) {
    auto path = WriteTempFile("{}");
    {
        ConfigManager cfg(path);
        cfg.Load();
        cfg.SetString("name", "persisted");
        cfg.Save();
    }
    {
        ConfigManager cfg(path);
        cfg.Load();
        EXPECT_EQ(cfg.GetString("name", ""), "persisted");
    }
    std::remove(path.c_str());
}
