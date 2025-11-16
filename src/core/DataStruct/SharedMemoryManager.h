#pragma once
#include "DataStruct.h"
#include "../common/PlatformDefs.h"
#include <string>
#include "../usb/USBInfo.h"

#ifdef PLATFORM_WINDOWS
    #include <windows.h>
    #include "../Utils/WmiManager.h"
    #define TCMT_STRNCPY_S strncpy_s
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <semaphore.h>
    #include <cstring>
    #include <fstream>
    #include <sstream>
    #include <cstdio>
    #define TCMT_STRNCPY_S strncpy
#endif

// Shared memory management class to avoid multiple definitions
class SharedMemoryManager {
private:
#ifdef PLATFORM_WINDOWS
    static HANDLE hMapFile;
    static HANDLE hMutex;
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    static int shmFd;
    static sem_t* semaphore;
    static std::string shmName;
    static std::string semName;
#endif
    static SharedMemoryBlock* pBuffer;
    static std::string lastError; // Store last error message
    
    // 跨平台服务访问
    static void* GetSystemService();
    
    // 服务管理器实例（用于平台特定信息采集）
    static void* serviceManager;
    
    // USB监控管理器实例
    static USBInfoManager* usbManager;
    
    // 平台特定初始化函数
#ifdef PLATFORM_WINDOWS
    static bool InitWindowsSharedMemory();
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    static bool InitPosixSharedMemory();
    static void CollectMacMotherboardInfo();
    static void CollectLinuxMotherboardInfo();
#endif
    
    // 辅助函数
    static void SetDefaultMotherboardInfo();

public:
    // Initialize shared memory
    static bool InitSharedMemory();

    // Write system info to shared memory
    static void WriteToSharedMemory(const SystemInfo& sysInfo);

    // Clean up shared memory resources
    static void CleanupSharedMemory();

    // Get buffer pointer (if needed)
    static SharedMemoryBlock* GetBuffer() { return pBuffer; }
    
    // Get last error message
    static std::string GetLastError();
};
