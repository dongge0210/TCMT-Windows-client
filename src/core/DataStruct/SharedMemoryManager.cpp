// 首先，取消可能导致冲突的 Qt 宏定义
#ifdef QT_VERSION
    // 暂时保存 Qt 宏定义
#define SAVE_QT_KEYWORDS
#ifdef slots
#undef slots
#endif
#ifdef signals
#undef signals
#endif
#ifdef emit
#undef emit
#endif
#ifdef foreach
#undef foreach
#endif
#ifdef name
#undef name
#endif
#ifdef first
#undef first
#endif
#ifdef c_str
#undef c_str
#endif
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>

// Make sure Windows.h is included before any other headers that might redefine GetLastError
#include <Windows.h>

#include "SharedMemoryManager.h"
// Fix the include path case sensitivity
#include "../Utils/WinUtils.h"
#include "../Utils/Logger.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

#ifndef WINUTILS_IMPLEMENTED
// Fallback implementation for FormatWindowsErrorMessage
inline std::string FallbackFormatWindowsErrorMessage(DWORD errorCode) {
    std::stringstream ss;
    ss << "Error code: " << errorCode;
    return ss.str();
}
#endif

// Initialize static members
HANDLE SharedMemoryManager::hMapFile = NULL;
SharedMemoryBlock* SharedMemoryManager::pBuffer = nullptr;
std::string SharedMemoryManager::lastError = "";

bool SharedMemoryManager::InitSharedMemory() {
    // Clear any previous error
    lastError.clear();
        
    try {
        // Try to enable privileges needed for creating global objects
        bool hasPrivileges = WinUtils::EnablePrivilege(L"SeCreateGlobalPrivilege");
        if (!hasPrivileges) {
            Logger::Warning("Failed to enable SeCreateGlobalPrivilege - attempting to continue anyway");
        }
    } catch(...) {
        Logger::Warning("Exception when enabling SeCreateGlobalPrivilege - attempting to continue anyway");
        // Continue execution as this is not critical
    }

    // Create security attributes to allow sharing between processes
    SECURITY_ATTRIBUTES securityAttributes;
    SECURITY_DESCRIPTOR securityDescriptor;

    // Initialize the security descriptor
    if (!InitializeSecurityDescriptor(&securityDescriptor, SECURITY_DESCRIPTOR_REVISION)) {
        DWORD errorCode = ::GetLastError();
        std::stringstream ss;
        ss << "Failed to initialize security descriptor. Error code: " << errorCode
           << " ("
           #ifdef WINUTILS_IMPLEMENTED
                << WinUtils::FormatWindowsErrorMessage(errorCode)
           #else
                << FallbackFormatWindowsErrorMessage(errorCode)
           #endif
           << ")";
        lastError = ss.str();
        Logger::Error(lastError);
        return false;
    }

    // Set the DACL to NULL for unrestricted access
    if (!SetSecurityDescriptorDacl(&securityDescriptor, TRUE, NULL, FALSE)) {
        DWORD errorCode = ::GetLastError();
        std::stringstream ss;
        ss << "Failed to set security descriptor DACL. Error code: " << errorCode
           << " ("
           #ifdef WINUTILS_IMPLEMENTED
                << WinUtils::FormatWindowsErrorMessage(errorCode)
           #else
                << FallbackFormatWindowsErrorMessage(errorCode)
           #endif
           << ")";
        lastError = ss.str();
        Logger::Error(lastError);
        return false;
    }

    // Setup security attributes
    securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    securityAttributes.lpSecurityDescriptor = &securityDescriptor;
    securityAttributes.bInheritHandle = FALSE;

    // Create file mapping object in Global namespace
    hMapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        &securityAttributes,
        PAGE_READWRITE,
        0,
        sizeof(SharedMemoryBlock),
        L"Global\\SystemMonitorSharedMemory"
    );
    if (hMapFile == NULL) {
        DWORD errorCode = ::GetLastError();
        // Fallback if Global is not permitted, try Local or no prefix
        Logger::Warning("Failed to create global shared memory, trying local namespace");

        hMapFile = CreateFileMapping(
            INVALID_HANDLE_VALUE,
            &securityAttributes,
            PAGE_READWRITE,
            0,
            sizeof(SharedMemoryBlock),
            L"Local\\SystemMonitorSharedMemory"
        );
        if (hMapFile == NULL) {
            hMapFile = CreateFileMapping(
                INVALID_HANDLE_VALUE,
                &securityAttributes,
                PAGE_READWRITE,
                0,
                sizeof(SharedMemoryBlock),
                L"SystemMonitorSharedMemory"
            );
        }

        // If still NULL after fallbacks, report error
        if (hMapFile == NULL) {
            errorCode = ::GetLastError();
            std::stringstream ss;
            ss << "Unable to create shared memory. Error code: " << errorCode
               << " ("
               #ifdef WINUTILS_IMPLEMENTED
                    << WinUtils::FormatWindowsErrorMessage(errorCode)
               #else
                    << FallbackFormatWindowsErrorMessage(errorCode)
               #endif
               << ")";
            // Possibly shared memory already exists
            if (errorCode == ERROR_ALREADY_EXISTS) {
                ss << " (Shared memory already exists)";
            }
            lastError = ss.str();
            Logger::Error(lastError);
            return false;
        }
    }

    // Check if we created a new mapping or opened an existing one
    DWORD errorCode = ::GetLastError();
    if (errorCode == ERROR_ALREADY_EXISTS) {
        Logger::Info("Opened existing shared memory mapping.");
    } else {
        Logger::Info("Created new shared memory mapping.");
    }

    // Map to process address space
    pBuffer = static_cast<SharedMemoryBlock*>(
        MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedMemoryBlock))
    );
    if (pBuffer == nullptr) {
        DWORD errorCode = ::GetLastError();
        std::stringstream ss;
        ss << "Unable to map shared memory view. Error code: " << errorCode
           << " ("
           #ifdef WINUTILS_IMPLEMENTED
                << WinUtils::FormatWindowsErrorMessage(errorCode)
           #else
                << FallbackFormatWindowsErrorMessage(errorCode)
           #endif
           << ")";
        lastError = ss.str();
        Logger::Error(lastError);
        CloseHandle(hMapFile);
        hMapFile = NULL;
        return false;
    }

    // Initialize critical section
    try {
        if (!InitializeCriticalSectionAndSpinCount(&pBuffer->lock, 4000)) {
            DWORD errorCode = ::GetLastError();
            std::stringstream ss;
            ss << "Failed to initialize critical section in shared memory. Error code: "
               << errorCode
               << " ("
               #ifdef WINUTILS_IMPLEMENTED
                    << WinUtils::FormatWindowsErrorMessage(errorCode)
               #else
                    << FallbackFormatWindowsErrorMessage(errorCode)
               #endif
               << ")";
            lastError = ss.str();
            Logger::Error(lastError);
            UnmapViewOfFile(pBuffer);
            pBuffer = nullptr;
            CloseHandle(hMapFile);
            hMapFile = NULL;
            return false;
        }
    } catch (...) {
        Logger::Error("Exception during critical section initialization.");
        UnmapViewOfFile(pBuffer);
        pBuffer = nullptr;
        CloseHandle(hMapFile);
        hMapFile = NULL;
        return false;
    }

    Logger::Info("Shared memory initialized successfully.");
    return true;
}

void SharedMemoryManager::CleanupSharedMemory() {
    if (pBuffer) {
        UnmapViewOfFile(pBuffer);
        pBuffer = nullptr;
    }
    if (hMapFile) {
        CloseHandle(hMapFile);
        hMapFile = NULL;
    }
}

std::string SharedMemoryManager::GetLastError() {
    return lastError;
}

void SharedMemoryManager::WriteToSharedMemory(const SystemInfo& systemInfo) {
    if (!pBuffer) {
        Logger::Error("Shared memory not initialized.");
        return;
    }
    EnterCriticalSection(&pBuffer->lock);
    // ...copy fields from systemInfo to pBuffer...
    LeaveCriticalSection(&pBuffer->lock);
}
