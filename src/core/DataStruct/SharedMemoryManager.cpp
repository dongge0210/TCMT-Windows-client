// SharedMemoryManager.cpp - 通过条件编译包含平台特定实现
// 注意：此文件不包含实际实现，仅作为平台特定实现的分发器

#include "SharedMemoryManager.h"
#include "../Utils/Logger.h"

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