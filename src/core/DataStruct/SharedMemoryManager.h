#pragma once
#include "DataStruct.h"
#include <windows.h>
#include <string>

// Shared memory management class to avoid multiple definitions
class SharedMemoryManager {
private:
    static HANDLE hMapFile;
    static SharedMemoryBlock* pBuffer;
    static std::string lastError; // Store last error message

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
