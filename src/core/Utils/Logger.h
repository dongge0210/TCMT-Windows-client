#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <mutex>
#include <fstream>
#include <chrono>
#include <initializer_list>
#include "../common/PlatformDefs.h"

#ifdef PLATFORM_WINDOWS
    #include <windows.h>
#elif defined(PLATFORM_MACOS)
    #include <unistd.h>
    #include <syslog.h>
    #include <ctime>
    #include <sys/time.h>
#else
    #include <ctime>
    #include <sys/time.h>
#endif
#if __has_include(<format>)
  #include <format>
  #define LOGGER_HAS_STD_FORMAT 1
#else
  #define LOGGER_HAS_STD_FORMAT 0
#endif

// Severity levels
enum LogLevel {
    LL_TRACE = 0,
    LL_DEBUG = 1,
    LL_INFO = 2,
    LL_WARNING = 3,
    LL_ERROR = 4,
    LL_CRITICAL = 5,
    LL_FATAL = 6
};

// Console colors (Windows attributes)
#ifdef PLATFORM_WINDOWS
    enum class ConsoleColor : WORD {
#else
    enum class ConsoleColor : uint16_t {
#endif
    DEFAULT       = 7,
    TRACE_COLOR   = 13,  // magenta
    DEBUG_COLOR   = 9,   // blue
    INFO_COLOR    = 10,  // green
    WARN_COLOR    = 14,  // yellow
    ERROR_COLOR   = 12,  // light red
    CRITICAL_COLOR= 5,   // purple
    FATAL_COLOR   = 4    // dark red
};

struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string category;   // component / module name
    std::string message;    // already UTF-8
    std::string file;       // source file
    int line = 0;           // source line
};

class Logger {
public:
    // Initialization (simple and with rotation)
    static bool Initialize(const std::string& filePath);
    static bool InitializeWithRotation(const std::string& baseFilePath, size_t maxFileSizeBytes, int maxFiles);
    
    // Cross-platform initialization
    static bool Initialize(const std::string& filePath, bool enableSyslog = false);

    // Configuration
    static void SetLogLevel(LogLevel level);
    static LogLevel GetLogLevel();
    static void EnableConsole(bool enable);
    static void SetRingBufferSize(size_t capacity); // in-memory recent logs for UI

    // Generic logging API
    static void Log(LogLevel level, std::string_view category, std::string_view message,
                    std::string_view file = {}, int line = 0);

    // Key/Value enriched logging
    static void LogKV(LogLevel level, std::string_view category, std::string_view message,
                      std::initializer_list<std::pair<std::string_view, std::string_view>> kv,
                      std::string_view file = {}, int line = 0);

#if LOGGER_HAS_STD_FORMAT
    template<typename... Args>
    static void Logf(LogLevel logLevel, std::string_view categoryTag, std::string_view fmtText,
                     std::string_view sourceFile, int sourceLine, Args&&... args) {
        if (logLevel < currentLevel) return;
        try {
            std::string msg = std::vformat(fmtText, std::make_format_args(args...));
            Log(logLevel, categoryTag, msg, sourceFile, sourceLine);
        } catch (...) {
            Log(logLevel, categoryTag, std::string("<Logf format error> ") + std::string(fmtText), sourceFile, sourceLine);
        }
    }
#endif

    // Backward compatible helpers (category defaults to "default")
    static void Trace(const std::string& m); static void Debug(const std::string& m);
    static void Info(const std::string& m);  static void Warn(const std::string& m);
    static void Error(const std::string& m); static void Critical(const std::string& m);
    static void Fatal(const std::string& m);

    // Wide string helpers
    static void Trace(const std::wstring& m); static void Debug(const std::wstring& m);
    static void Info(const std::wstring& m);  static void Warn(const std::wstring& m);
    static void Error(const std::wstring& m); static void Critical(const std::wstring& m);
    static void Fatal(const std::wstring& m);

    // Retrieval for UI / diagnostics
    static std::vector<LogEntry> GetRecentEntries(); // snapshot copy
    static bool IsInitialized();
    
    // Cleanup
    static void Shutdown();

private:
    static std::mutex mutex;
    static std::ofstream stream;
    static bool consoleEnabled;
    static LogLevel currentLevel;
    #ifdef PLATFORM_WINDOWS
        static HANDLE consoleHandle;
    #elif defined(PLATFORM_MACOS)
        static bool useSyslog;
        static FILE* consoleHandle;
    #else
        static bool useSyslog;
        static FILE* consoleHandle;
    #endif

    // Rotation
    static size_t maxFileSize;
    static int maxFileCount;
    static std::string basePath; // base file path without index

    // Ring buffer
    static std::vector<LogEntry> ring;
    static size_t ringCapacity;
    static size_t ringIndex;

    // Internal helpers
    static void WriteInternal(const LogEntry& entry);
    static void RotateIfNeeded();
    static std::string LevelToString(LogLevel l);
    static ConsoleColor LevelToColor(LogLevel l);
    static std::string NormalizeToUTF8(std::string_view input);
    static std::string WStringToUtf8(const std::wstring& w);
    static void PushRing(const LogEntry& e);
};

// Macros capturing source location & formatting
#if LOGGER_HAS_STD_FORMAT
#define LOG_T(cat, fmt, ...) Logger::Logf(LL_TRACE,    cat, fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_D(cat, fmt, ...) Logger::Logf(LL_DEBUG,    cat, fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_I(cat, fmt, ...) Logger::Logf(LL_INFO,     cat, fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_W(cat, fmt, ...) Logger::Logf(LL_WARNING,  cat, fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_E(cat, fmt, ...) Logger::Logf(LL_ERROR,    cat, fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_C(cat, fmt, ...) Logger::Logf(LL_CRITICAL, cat, fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_F(cat, fmt, ...) Logger::Logf(LL_FATAL,    cat, fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define LOG_T(cat, msg) Logger::Log(LL_TRACE,    cat, msg, __FILE__, __LINE__)
#define LOG_D(cat, msg) Logger::Log(LL_DEBUG,    cat, msg, __FILE__, __LINE__)
#define LOG_I(cat, msg) Logger::Log(LL_INFO,     cat, msg, __FILE__, __LINE__)
#define LOG_W(cat, msg) Logger::Log(LL_WARNING,  cat, msg, __FILE__, __LINE__)
#define LOG_E(cat, msg) Logger::Log(LL_ERROR,    cat, msg, __FILE__, __LINE__)
#define LOG_C(cat, msg) Logger::Log(LL_CRITICAL, cat, msg, __FILE__, __LINE__)
#define LOG_F(cat, msg) Logger::Log(LL_FATAL,    cat, msg, __FILE__, __LINE__)
#endif
