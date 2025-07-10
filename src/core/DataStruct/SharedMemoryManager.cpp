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
    ss << "错误码: " << errorCode;
    return ss.str();
}
#endif


// Initialize static members
HANDLE SharedMemoryManager::hMapFile = NULL;
SharedMemoryBlock* SharedMemoryManager::pBuffer = nullptr;
std::string SharedMemoryManager::lastError = "";
// 跨进程互斥体用于同步共享内存写入
static HANDLE g_hMutex = NULL;

bool SharedMemoryManager::InitSharedMemory() {
    // Clear any previous error
    lastError.clear();
        
    try {
        // Try to enable privileges needed for creating global objects
        bool hasPrivileges = WinUtils::EnablePrivilege(L"SeCreateGlobalPrivilege");
        if (!hasPrivileges) {
            Logger::Warn("未能启用 SeCreateGlobalPrivilege - 尝试继续");
        }
    } catch(...) {
        Logger::Warn("启用 SeCreateGlobalPrivilege 时发生异常 - 尝试继续");
        // Continue execution as this is not critical
    }

    // 创建全局互斥体用于多进程同步
    if (!g_hMutex) {
        g_hMutex = CreateMutexW(NULL, FALSE, L"Global\\SystemMonitorSharedMemoryMutex");
        if (!g_hMutex) {
            Logger::Error("未能创建全局互斥体用于共享内存同步");
            return false;
        }
    }

    // Create security attributes to allow sharing between processes
    SECURITY_ATTRIBUTES securityAttributes;
    SECURITY_DESCRIPTOR securityDescriptor;

    // Initialize the security descriptor
    if (!InitializeSecurityDescriptor(&securityDescriptor, SECURITY_DESCRIPTOR_REVISION)) {
        DWORD errorCode = ::GetLastError();
        std::stringstream ss;
        ss << "未能初始化安全描述符。错误码: " << errorCode
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
        ss << "未能设置安全描述符 DACL。错误码: " << errorCode
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
        Logger::Warn("未能创建全局共享内存，尝试本地命名空间");

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
            ss << "未能创建共享内存。错误码: " << errorCode
               << " ("
               #ifdef WINUTILS_IMPLEMENTED
                    << WinUtils::FormatWindowsErrorMessage(errorCode)
               #else
                    << FallbackFormatWindowsErrorMessage(errorCode)
               #endif
               << ")";
            // Possibly shared memory already exists
            if (errorCode == ERROR_ALREADY_EXISTS) {
                ss << " (共享内存已存在)";
            }
            lastError = ss.str();
            Logger::Error(lastError);
            return false;
        }
    }

    // Check if we created a new mapping or opened an existing one
    DWORD errorCode = ::GetLastError();
    if (errorCode == ERROR_ALREADY_EXISTS) {
        Logger::Info("打开了现有的共享内存映射.");
    } else {
        Logger::Info("创建了新的共享内存映射.");
    }

    // Map to process address space
    pBuffer = static_cast<SharedMemoryBlock*>(
        MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedMemoryBlock))
    );
    if (pBuffer == nullptr) {
        DWORD errorCode = ::GetLastError();
        std::stringstream ss;
        ss << "未能映射共享内存视图。错误码: " << errorCode
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


    // 不再在共享内存结构体中初始化CriticalSection

    // Zero out the shared memory to avoid dirty data (only on first creation)
    if (errorCode != ERROR_ALREADY_EXISTS) {
        memset(pBuffer, 0, sizeof(SharedMemoryBlock));
    }

    Logger::Info("共享内存成功初始化.");
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
        lastError = "共享内存未初始化";
        Logger::Error(lastError);
        return;
    }

    // 跨进程同步：加互斥体
    DWORD waitResult = WaitForSingleObject(g_hMutex, 5000); // 最多等5秒
    if (waitResult != WAIT_OBJECT_0) {
        Logger::Error("未能获取共享内存互斥体");
        return;
    }
    try {
        // Clear all string fields first to ensure proper null termination
        memset(pBuffer->cpuName, 0, sizeof(pBuffer->cpuName));
        for (int i = 0; i < 2; ++i) {
            memset(pBuffer->gpus[i].name, 0, sizeof(pBuffer->gpus[i].name));
            memset(pBuffer->gpus[i].brand, 0, sizeof(pBuffer->gpus[i].brand));
        }
        for (int i = 0; i < 8; ++i) {
            memset(&pBuffer->disks[i], 0, sizeof(pBuffer->disks[i]));
        }
        for (int i = 0; i < 10; ++i) {
            memset(pBuffer->temperatures[i].sensorName, 0, sizeof(pBuffer->temperatures[i].sensorName));
        }
        
        // Copy CPU information with safe string conversion
        std::wstring cpuNameW = WinUtils::StringToWstring(systemInfo.cpuName);
        if (!cpuNameW.empty()) {
            size_t copyLen = std::min(cpuNameW.length(), static_cast<size_t>(127));
            wcsncpy_s(pBuffer->cpuName, sizeof(pBuffer->cpuName) / sizeof(wchar_t), 
                     cpuNameW.c_str(), copyLen);
            pBuffer->cpuName[copyLen] = L'\0'; // Ensure null termination
        }
        
        pBuffer->physicalCores = systemInfo.physicalCores;
        pBuffer->logicalCores = systemInfo.logicalCores;
        pBuffer->cpuUsage = static_cast<float>(systemInfo.cpuUsage);
        pBuffer->performanceCores = systemInfo.performanceCores;
        pBuffer->efficiencyCores = systemInfo.efficiencyCores;
        pBuffer->pCoreFreq = systemInfo.performanceCoreFreq;
        pBuffer->eCoreFreq = systemInfo.efficiencyCoreFreq;
        pBuffer->hyperThreading = systemInfo.hyperThreading;
        pBuffer->virtualization = systemInfo.virtualization;

        // Copy memory information
        pBuffer->totalMemory = systemInfo.totalMemory;
        pBuffer->usedMemory = systemInfo.usedMemory;
        pBuffer->availableMemory = systemInfo.availableMemory;

        // Copy GPU information with safe string handling
        pBuffer->gpuCount = 0;
        if (!systemInfo.gpuName.empty()) {
            std::wstring gpuNameW = WinUtils::StringToWstring(systemInfo.gpuName);
            std::wstring gpuBrandW = WinUtils::StringToWstring(systemInfo.gpuBrand);
            
            if (!gpuNameW.empty()) {
                size_t copyLen = std::min(gpuNameW.length(), static_cast<size_t>(127));
                wcsncpy_s(pBuffer->gpus[0].name, sizeof(pBuffer->gpus[0].name) / sizeof(wchar_t), 
                         gpuNameW.c_str(), copyLen);
                pBuffer->gpus[0].name[copyLen] = L'\0';
            }
            
            if (!gpuBrandW.empty()) {
                size_t copyLen = std::min(gpuBrandW.length(), static_cast<size_t>(63));
                wcsncpy_s(pBuffer->gpus[0].brand, sizeof(pBuffer->gpus[0].brand) / sizeof(wchar_t), 
                         gpuBrandW.c_str(), copyLen);
                pBuffer->gpus[0].brand[copyLen] = L'\0';
            }
            
            pBuffer->gpus[0].memory = systemInfo.gpuMemory;
            pBuffer->gpus[0].coreClock = systemInfo.gpuCoreFreq;
            pBuffer->gpuCount = 1;
        }

        // Copy network adapter information
        pBuffer->adapterCount = 0;
        if (!systemInfo.networkAdapterName.empty()) {
            std::wstring adapterNameW = WinUtils::StringToWstring(systemInfo.networkAdapterName);
            std::wstring adapterMacW = WinUtils::StringToWstring(systemInfo.networkAdapterMac);
            
            if (!adapterNameW.empty()) {
                size_t copyLen = std::min(adapterNameW.length(), static_cast<size_t>(127));
                wcsncpy_s(pBuffer->adapters[0].name, sizeof(pBuffer->adapters[0].name) / sizeof(wchar_t), 
                         adapterNameW.c_str(), copyLen);
                pBuffer->adapters[0].name[copyLen] = L'\0';
            }
            
            if (!adapterMacW.empty()) {
                size_t copyLen = std::min(adapterMacW.length(), static_cast<size_t>(31));
                wcsncpy_s(pBuffer->adapters[0].mac, sizeof(pBuffer->adapters[0].mac) / sizeof(wchar_t), 
                         adapterMacW.c_str(), copyLen);
                pBuffer->adapters[0].mac[copyLen] = L'\0';
            }
            
            pBuffer->adapters[0].speed = systemInfo.networkAdapterSpeed;
            pBuffer->adapterCount = 1;
        }

        // Copy disk information with safe string handling
        pBuffer->diskCount = static_cast<int>(std::min(systemInfo.disks.size(), static_cast<size_t>(8)));
        for (int i = 0; i < pBuffer->diskCount; ++i) {
            const auto& disk = systemInfo.disks[i];
            pBuffer->disks[i].letter = disk.letter;
            
            if (!disk.label.empty()) {
                std::wstring labelW = WinUtils::StringToWstring(disk.label);
                size_t copyLen = std::min(labelW.length(), static_cast<size_t>(127));
                wcsncpy_s(pBuffer->disks[i].label, sizeof(pBuffer->disks[i].label) / sizeof(wchar_t), 
                         labelW.c_str(), copyLen);
                pBuffer->disks[i].label[copyLen] = L'\0';
            }
            
            if (!disk.fileSystem.empty()) {
                std::wstring fileSystemW = WinUtils::StringToWstring(disk.fileSystem);
                size_t copyLen = std::min(fileSystemW.length(), static_cast<size_t>(31));
                wcsncpy_s(pBuffer->disks[i].fileSystem, sizeof(pBuffer->disks[i].fileSystem) / sizeof(wchar_t), 
                         fileSystemW.c_str(), copyLen);
                pBuffer->disks[i].fileSystem[copyLen] = L'\0';
            }
            
            pBuffer->disks[i].totalSize = disk.totalSize;
            pBuffer->disks[i].usedSpace = disk.usedSpace;
            pBuffer->disks[i].freeSpace = disk.freeSpace;
        }

        // Copy temperature information with safe string handling
        pBuffer->tempCount = static_cast<int>(std::min(systemInfo.temperatures.size(), static_cast<size_t>(10)));
        for (int i = 0; i < pBuffer->tempCount; ++i) {
            const auto& temp = systemInfo.temperatures[i];
            
            if (!temp.first.empty()) {
                std::wstring sensorNameW = WinUtils::StringToWstring(temp.first);
                size_t copyLen = std::min(sensorNameW.length(), static_cast<size_t>(63));
                wcsncpy_s(pBuffer->temperatures[i].sensorName, sizeof(pBuffer->temperatures[i].sensorName) / sizeof(wchar_t), 
                         sensorNameW.c_str(), copyLen);
                pBuffer->temperatures[i].sensorName[copyLen] = L'\0';
            }
            
            pBuffer->temperatures[i].temperature = temp.second;
        }

        // Update timestamp
        GetSystemTime(&pBuffer->lastUpdate);
        
        Logger::Trace("成功写入系统信息到共享内存");
    }
    catch (const std::exception& e) {
        lastError = "WriteToSharedMemory 中的异常: " + std::string(e.what());
        Logger::Error(lastError);
    }
    catch (...) {
        lastError = "WriteToSharedMemory 中的未知异常";
        Logger::Error(lastError);
    }
    ReleaseMutex(g_hMutex);
}
