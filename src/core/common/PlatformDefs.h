#pragma once

// 平台检测宏
#if defined(_WIN32) || defined(_WIN64)
    #ifndef PLATFORM_WINDOWS
        #define PLATFORM_WINDOWS
        #define PLATFORM_NAME "Windows"
        #define PLATFORM_WINDOWS_VERSION 1
    #endif
#elif defined(__APPLE__)
    #ifndef PLATFORM_MACOS
        #define PLATFORM_MACOS
        #define PLATFORM_NAME "macOS"
        #define PLATFORM_MACOS_VERSION 1
    #endif
#elif defined(__linux__)
    #ifndef PLATFORM_LINUX
        #define PLATFORM_LINUX
        #define PLATFORM_NAME "Linux"
        #define PLATFORM_LINUX_VERSION 1
    #endif
#else
    #error "Unsupported platform"
#endif

// 编译器检测
#if defined(_MSC_VER)
    #define COMPILER_MSVC
    #define COMPILER_NAME "MSVC"
    #define COMPILER_MSVC_VERSION _MSC_VER
#elif defined(__clang__)
    #define COMPILER_CLANG
    #define COMPILER_NAME "Clang"
    #define COMPILER_CLANG_VERSION __clang_major__
#elif defined(__GNUC__)
    #define COMPILER_GCC
    #define COMPILER_NAME "GCC"
    #define COMPILER_GCC_VERSION __GNUC__
#else
    #error "Unsupported compiler"
#endif

// 架构检测
#if defined(_M_X64) || defined(__x86_64__)
    #define ARCH_X64
    #define ARCH_NAME "x64"
#elif defined(_M_IX86) || defined(__i386__)
    #define ARCH_X86
    #define ARCH_NAME "x86"
#elif defined(_M_ARM64) || defined(__aarch64__)
    #define ARCH_ARM64
    #define ARCH_NAME "ARM64"
#elif defined(_M_ARM) || defined(__arm__)
    #define ARCH_ARM
    #define ARCH_NAME "ARM"
#else
    #error "Unsupported architecture"
#endif

// 平台特定宏定义
#ifdef PLATFORM_WINDOWS
    #define PATH_SEPARATOR "\\"
    #define LINE_SEPARATOR "\r\n"
    #define DLL_EXPORT __declspec(dllexport)
    #define DLL_IMPORT __declspec(dllimport)
    #define THREAD_LOCAL __declspec(thread)
    #define FORCE_INLINE __forceinline
    #define NO_RETURN __declspec(noreturn)
    #define ALIGN(n) __declspec(align(n))
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    #define PATH_SEPARATOR "/"
    #define LINE_SEPARATOR "\n"
    #define DLL_EXPORT __attribute__((visibility("default")))
    #define DLL_IMPORT __attribute__((visibility("default")))
    #define THREAD_LOCAL __thread
    #define FORCE_INLINE inline __attribute__((always_inline))
    #define NO_RETURN __attribute__((noreturn))
    #define ALIGN(n) __attribute__((aligned(n)))
#endif

// 调试宏
#ifdef _DEBUG
    #define DEBUG_MODE 1
    #ifdef PLATFORM_WINDOWS
        #define DEBUG_BREAK() __debugbreak()
    #elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
        #define DEBUG_BREAK() __builtin_trap()
    #endif
#else
    #define DEBUG_MODE 0
    #define DEBUG_BREAK() do {} while(0)
#endif

// 字符串处理宏
#ifdef PLATFORM_WINDOWS
    #define UNICODE_STRING 1
    #define TCHAR wchar_t
    #define _T(x) L ## x
    #define tcout std::wcout
    #define tcin std::wcin
    #define tstring std::wstring
#else
    #define UNICODE_STRING 0
    #define TCHAR char
    #define _T(x) x
    #define tcout std::cout
    #define tcin std::cin
    #define tstring std::string
#endif

// 内存对齐宏
#define PACKED_STRUCT __attribute__((packed))
#ifndef PRAGMA_PACK
    #ifdef _MSC_VER
        #define PRAGMA_PACK_PUSH(n) __pragma(pack(push, n))
        #define PRAGMA_PACK_POP() __pragma(pack(pop))
    #else
        #define PRAGMA_PACK_PUSH(n) _Pragma("pack(push, n)")
        #define PRAGMA_PACK_POP() _Pragma("pack(pop)")
    #endif
#endif

// 为了兼容性，保留旧的宏定义（仅在未定义时）
#ifndef PRAGMA_PACK
#define PRAGMA_PACK(push, n) PRAGMA_PACK_PUSH(n)
#define PRAGMA_PACK(pop) PRAGMA_PACK_POP()
#endif

// 函数调用约定
#ifdef PLATFORM_WINDOWS
    #define API_CALL __stdcall
    #define API_CDECL __cdecl
    #define API_FASTCALL __fastcall
#else
    #define API_CALL
    #define API_CDECL
    #define API_FASTCALL
#endif

// 常用平台特定类型定义
#ifdef PLATFORM_WINDOWS
    // Avoid redefining fixed-width types; include standard headers instead.
    #include <stdint.h>
    #include <cstdint>
#endif

// 系统相关宏
#define MAX_PATH_LENGTH 260
#define MAX_STRING_LENGTH 1024
#define DEFAULT_BUFFER_SIZE 4096

// 时间相关宏
#ifdef PLATFORM_WINDOWS
    #define HIGH_RESOLUTION_TIMER_AVAILABLE 1
    #define USE_QUERY_PERFORMANCE_COUNTER 1
#else
    #define HIGH_RESOLUTION_TIMER_AVAILABLE 1
    #define USE_CLOCK_GETTIME 1
#endif

// 网络相关宏
#ifdef PLATFORM_WINDOWS
    #define SOCKET_TYPE SOCKET
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
#else
    #define SOCKET_TYPE int
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
#endif

// 文件系统相关宏
#ifdef PLATFORM_WINDOWS
    #define FILE_HANDLE_TYPE HANDLE
    #define INVALID_FILE_HANDLE INVALID_HANDLE_VALUE
#else
    #define FILE_HANDLE_TYPE int
    #define INVALID_FILE_HANDLE -1
#endif

// 线程相关宏
#ifdef PLATFORM_WINDOWS
    #define THREAD_TYPE HANDLE
    #define MUTEX_TYPE HANDLE
    #define CONDITION_VARIABLE_TYPE CONDITION_VARIABLE
#else
    #include <pthread.h>
    #define THREAD_TYPE pthread_t
    #define MUTEX_TYPE pthread_mutex_t
    #define CONDITION_VARIABLE_TYPE pthread_cond_t
#endif

// 平台特定功能检测
#ifdef PLATFORM_WINDOWS
    #define HAS_WMI_SUPPORT 1
    #define HAS_REGISTRY_SUPPORT 1
    #define HAS_COM_SUPPORT 1
    #define HAS_PDH_SUPPORT 1
#elif defined(PLATFORM_MACOS)
    #define HAS_IOKIT_SUPPORT 1
    #define HAS_FOUNDATION_SUPPORT 1
    #define HAS_COREFOUNDATION_SUPPORT 1
    #define HAS_SYSCTL_SUPPORT 1
#elif defined(PLATFORM_LINUX)
    #define HAS_PROCFS_SUPPORT 1
    #define HAS_SYSFS_SUPPORT 1
    #define HAS_UDEV_SUPPORT 1
#endif

// 编译器特定优化
#ifdef COMPILER_MSVC
    #define LIKELY(x) (x)
    #define UNLIKELY(x) (x)
    #define PREFETCH(addr) do {} while(0)
#else
    #define LIKELY(x) __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define PREFETCH(addr) __builtin_prefetch(addr, 0, 3)
#endif

// 断言宏
#ifdef _DEBUG
    #define ASSERT(condition) \
        do { \
            if (!(condition)) { \
                std::cerr << "Assertion failed: " << #condition \
                          << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
                DEBUG_BREAK(); \
            } \
        } while(0)
#else
    #define ASSERT(condition) do {} while(0)
#endif

// 日志宏
#define LOG_INFO(msg) std::cout << "[INFO] " << msg << std::endl
#define LOG_WARNING(msg) std::cout << "[WARNING] " << msg << std::endl
#define LOG_ERROR(msg) std::cerr << "[ERROR] " << msg << std::endl
#ifdef _DEBUG
    #define LOG_DEBUG(msg) std::cout << "[DEBUG] " << msg << std::endl
#else
    #define LOG_DEBUG(msg) do {} while(0)
#endif

// 工具宏
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define OFFSET_OF(type, member) ((size_t)&(((type*)0)->member))
#define CONTAINER_OF(ptr, type, member) ((type*)((char*)(ptr) - OFFSET_OF(type, member)))

// 字符串转换宏
#ifdef PLATFORM_WINDOWS
    #define TO_PLATFORM_STRING(str) WinUtils::Utf8ToWstring(str)
    #define FROM_PLATFORM_STRING(wstr) WinUtils::WstringToUtf8(wstr)
#else
    #define TO_PLATFORM_STRING(str) (str)
    #define FROM_PLATFORM_STRING(str) (str)
#endif

// 版本信息宏
#define PLATFORM_VERSION_STRING PLATFORM_NAME " " ARCH_NAME
#define COMPILER_VERSION_STRING COMPILER_NAME " " COMPILER_VERSION

// 构建时间戳
#define BUILD_TIMESTAMP __DATE__ " " __TIME__
#define COMPILER_TIMESTAMP __TIMESTAMP__