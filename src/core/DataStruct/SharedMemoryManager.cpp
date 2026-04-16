// SharedMemoryManager.cpp - 通过条件编译包含平台特定实现
// 注意：此文件不包含实际实现，仅作为平台特定实现的分发器

#include "SharedMemoryManager.h"
#include "../Utils/Logger.h"

// 自动检测平台并定义相应的TCMT_宏
#if defined(_WIN32) || defined(_WIN64)
    #ifndef TCMT_WINDOWS
        #define TCMT_WINDOWS
    #endif
#elif defined(__APPLE__) && defined(__MACH__)
    #ifndef TCMT_MACOS
        #define TCMT_MACOS
    #endif
#elif defined(__linux__)
    #ifndef TCMT_LINUX
        #define TCMT_LINUX
    #endif
#endif

// 如果仍未定义任何平台宏，尝试根据其他宏猜测
#if !defined(TCMT_WINDOWS) && !defined(TCMT_MACOS) && !defined(TCMT_LINUX)
    #if defined(_WIN32) || defined(_WIN64)
        #define TCMT_WINDOWS
    #elif defined(__APPLE__) && defined(__MACH__)
        #define TCMT_MACOS
    #elif defined(__linux__)
        #define TCMT_LINUX
    #else
        // 无法检测平台，默认使用macOS（因为当前开发目标）
        #define TCMT_MACOS
    #endif
#endif

// 根据平台包含相应的实现
#ifdef TCMT_WINDOWS
#include "SharedMemoryManager_Windows.cpp"
#elif defined(TCMT_MACOS)
#include "SharedMemoryManager_macOS.cpp"
#elif defined(TCMT_LINUX)
#include "SharedMemoryManager_Linux.cpp"
#else
#error "Unsupported platform - please define TCMT_WINDOWS, TCMT_MACOS, or TCMT_LINUX"
#endif