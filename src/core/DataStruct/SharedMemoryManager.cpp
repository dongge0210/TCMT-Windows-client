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
#include <cctype>

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
// 跨进程互斥体用于同步共享内存
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
        Logger::Critical(lastError);
        return;
    }

    DWORD waitResult = WaitForSingleObject(g_hMutex, 5000); // 最多等5秒
    if (waitResult != WAIT_OBJECT_0) {
        Logger::Critical("未能获取共享内存互斥体");
        return;
    }
    auto SafeCopyWideString = [](wchar_t* dest, size_t destSize, const std::wstring& src) {
        try {
            if (dest == nullptr || destSize == 0) return;
            memset(dest, 0, destSize * sizeof(wchar_t));
            if (src.empty()) { dest[0] = L'\0'; return; }
            size_t copyLen = std::min(src.length(), destSize - 1);
            for (size_t i = 0; i < copyLen; ++i) dest[i] = src[i];
            dest[copyLen] = L'\0';
        } catch (...) { if (dest && destSize > 0) dest[0] = L'\0'; }
    };
    auto SafeCopyFromWideArray = [](wchar_t* dest, size_t destSize, const wchar_t* src, size_t srcCapacity) {
        if (!dest || destSize == 0) return;
        memset(dest, 0, destSize * sizeof(wchar_t));
        if (!src) return;
        size_t len = 0;
        while (len < srcCapacity && src[len] != L'\0') ++len;
        if (len >= destSize) len = destSize - 1;
        for (size_t i = 0; i < len; ++i) dest[i] = src[i];
        dest[len] = L'\0';
    };
    try {
        // 清零主要字符串/数组区域
        memset(pBuffer->cpuName, 0, sizeof(pBuffer->cpuName));
        for (int i = 0; i < 2; ++i) { memset(pBuffer->gpus[i].name, 0, sizeof(pBuffer->gpus[i].name)); memset(pBuffer->gpus[i].brand, 0, sizeof(pBuffer->gpus[i].brand)); }
        for (int i = 0; i < 8; ++i) { memset(&pBuffer->disks[i], 0, sizeof(pBuffer->disks[i])); memset(&pBuffer->physicalDisks[i], 0, sizeof(pBuffer->physicalDisks[i])); }
        for (int i = 0; i < 10; ++i) { memset(pBuffer->temperatures[i].sensorName, 0, sizeof(pBuffer->temperatures[i].sensorName)); }

        // CPU
        SafeCopyWideString(pBuffer->cpuName, 128, WinUtils::StringToWstring(systemInfo.cpuName));
        pBuffer->physicalCores = systemInfo.physicalCores;
        pBuffer->logicalCores = systemInfo.logicalCores;
        pBuffer->cpuUsage = systemInfo.cpuUsage;
        pBuffer->performanceCores = systemInfo.performanceCores;
        pBuffer->efficiencyCores = systemInfo.efficiencyCores;
        pBuffer->pCoreFreq = systemInfo.performanceCoreFreq;
        pBuffer->eCoreFreq = systemInfo.efficiencyCoreFreq;
        pBuffer->hyperThreading = systemInfo.hyperThreading;
        pBuffer->virtualization = systemInfo.virtualization;

        // 内存
        pBuffer->totalMemory = systemInfo.totalMemory;
        pBuffer->usedMemory = systemInfo.usedMemory;
        pBuffer->availableMemory = systemInfo.availableMemory;

        // GPU（兼容旧字段）
        pBuffer->gpuCount = 0;
        if (!systemInfo.gpuName.empty()) {
            SafeCopyWideString(pBuffer->gpus[0].name, 128, WinUtils::StringToWstring(systemInfo.gpuName));
            SafeCopyWideString(pBuffer->gpus[0].brand, 64, WinUtils::StringToWstring(systemInfo.gpuBrand));
            pBuffer->gpus[0].memory = systemInfo.gpuMemory;
            pBuffer->gpus[0].coreClock = systemInfo.gpuCoreFreq;
            pBuffer->gpus[0].isVirtual = systemInfo.gpuIsVirtual;
            pBuffer->gpuCount = 1;
        }
        // 如后续要支持 vector<GPUData> 可在此扩展

        // 网络适配器（SystemInfo.adapters 里的 NetworkAdapterData 为 wchar_t 数组字段）
        pBuffer->adapterCount = 0;
        int adapterWriteCount = static_cast<int>(std::min(systemInfo.adapters.size(), size_t(4)));
        for (int i = 0; i < adapterWriteCount; ++i) {
            const auto& src = systemInfo.adapters[i];
            SafeCopyFromWideArray(pBuffer->adapters[i].name, 128, src.name, 128);
            SafeCopyFromWideArray(pBuffer->adapters[i].mac, 32, src.mac, 32);
            SafeCopyFromWideArray(pBuffer->adapters[i].ipAddress, 64, src.ipAddress, 64);
            SafeCopyFromWideArray(pBuffer->adapters[i].adapterType, 32, src.adapterType, 32);
            pBuffer->adapters[i].speed = src.speed;
        }
        pBuffer->adapterCount = adapterWriteCount;
        if (adapterWriteCount == 0 && !systemInfo.networkAdapterName.empty()) {
            SafeCopyWideString(pBuffer->adapters[0].name, 128, WinUtils::StringToWstring(systemInfo.networkAdapterName));
            SafeCopyWideString(pBuffer->adapters[0].mac, 32, WinUtils::StringToWstring(systemInfo.networkAdapterMac));
            SafeCopyWideString(pBuffer->adapters[0].ipAddress, 64, WinUtils::StringToWstring(systemInfo.networkAdapterIp));
            SafeCopyWideString(pBuffer->adapters[0].adapterType, 32, WinUtils::StringToWstring(systemInfo.networkAdapterType));
            pBuffer->adapters[0].speed = systemInfo.networkAdapterSpeed;
            pBuffer->adapterCount = 1;
        }

        // 逻辑磁盘（SystemInfo.disks 中 label / fileSystem 是 std::string）
        pBuffer->diskCount = static_cast<int>(std::min(systemInfo.disks.size(), static_cast<size_t>(8)));
        for (int i = 0; i < pBuffer->diskCount; ++i) {
            const auto& disk = systemInfo.disks[i];
            pBuffer->disks[i].letter = disk.letter;
            std::string safeLabel = disk.label;
            if (safeLabel.empty()) safeLabel = ""; // 未命名允许为空，在UI端替换
            else if (!WinUtils::IsLikelyUtf8(safeLabel)) {
                // 退化处理：按当前ACP转 wide 再回 UTF-8，尽量 salvage
                std::wstring w = WinUtils::Utf8ToWstring(safeLabel); // 若不是utf8会得到空
                if (w.empty()) {
                    int len = MultiByteToWideChar(CP_ACP, 0, safeLabel.c_str(), (int)safeLabel.size(), nullptr, 0);
                    if (len > 0) { w.resize(len); MultiByteToWideChar(CP_ACP, 0, safeLabel.c_str(), (int)safeLabel.size(), w.data(), len); }
                }
                safeLabel = WinUtils::WstringToUtf8(w);
            }
            SafeCopyWideString(pBuffer->disks[i].label, 128, WinUtils::StringToWstring(safeLabel));
            SafeCopyWideString(pBuffer->disks[i].fileSystem, 32, WinUtils::StringToWstring(disk.fileSystem));
            pBuffer->disks[i].totalSize = disk.totalSize;
            pBuffer->disks[i].usedSpace = disk.usedSpace;
            pBuffer->disks[i].freeSpace = disk.freeSpace;
        }

        // 物理磁盘 + SMART（SystemInfo.physicalDisks 里字段已为 wchar_t 数组）
        pBuffer->physicalDiskCount = static_cast<int>(std::min(systemInfo.physicalDisks.size(), static_cast<size_t>(8)));
        for (int i = 0; i < pBuffer->physicalDiskCount; ++i) {
            const auto& src = systemInfo.physicalDisks[i];
            SafeCopyFromWideArray(pBuffer->physicalDisks[i].model, 128, src.model, 128);
            SafeCopyFromWideArray(pBuffer->physicalDisks[i].serialNumber, 64, src.serialNumber, 64);
            SafeCopyFromWideArray(pBuffer->physicalDisks[i].firmwareVersion, 32, src.firmwareVersion, 32);
            SafeCopyFromWideArray(pBuffer->physicalDisks[i].interfaceType, 32, src.interfaceType, 32);
            SafeCopyFromWideArray(pBuffer->physicalDisks[i].diskType, 16, src.diskType, 16);
            pBuffer->physicalDisks[i].capacity = src.capacity;
            pBuffer->physicalDisks[i].temperature = src.temperature;
            pBuffer->physicalDisks[i].healthPercentage = src.healthPercentage;
            pBuffer->physicalDisks[i].isSystemDisk = src.isSystemDisk;
            pBuffer->physicalDisks[i].smartEnabled = src.smartEnabled;
            pBuffer->physicalDisks[i].smartSupported = src.smartSupported;
            pBuffer->physicalDisks[i].powerOnHours = src.powerOnHours;
            pBuffer->physicalDisks[i].powerCycleCount = src.powerCycleCount;
            pBuffer->physicalDisks[i].reallocatedSectorCount = src.reallocatedSectorCount;
            pBuffer->physicalDisks[i].currentPendingSector = src.currentPendingSector;
            pBuffer->physicalDisks[i].uncorrectableErrors = src.uncorrectableErrors;
            pBuffer->physicalDisks[i].wearLeveling = src.wearLeveling;
            pBuffer->physicalDisks[i].totalBytesWritten = src.totalBytesWritten;
            pBuffer->physicalDisks[i].totalBytesRead = src.totalBytesRead;
            int ldCount = 0;
            for (char l : src.logicalDriveLetters) {
                if (ldCount >= 8 || l == 0) break;
                if (std::isalpha(static_cast<unsigned char>(l))) pBuffer->physicalDisks[i].logicalDriveLetters[ldCount++] = l;
            }
            pBuffer->physicalDisks[i].logicalDriveCount = ldCount;
            int attrCount = src.attributeCount;
            if (attrCount < 0) attrCount = 0; if (attrCount > 32) attrCount = 32;
            pBuffer->physicalDisks[i].attributeCount = attrCount;
            for (int a = 0; a < attrCount; ++a) {
                const auto& sa = src.attributes[a];
                auto& dst = pBuffer->physicalDisks[i].attributes[a];
                dst.id = sa.id;
                dst.flags = sa.flags;
                dst.current = sa.current;
                dst.worst = sa.worst;
                dst.threshold = sa.threshold;
                dst.rawValue = sa.rawValue;
                dst.isCritical = sa.isCritical;
                dst.physicalValue = sa.physicalValue;
                SafeCopyFromWideArray(dst.name, 64, sa.name, 64);
                SafeCopyFromWideArray(dst.description, 128, sa.description, 128);
                SafeCopyFromWideArray(dst.units, 16, sa.units, 16);
            }
        }

        // 温度数组（传感器名字在 vector<pair<string,double>> 中）
        pBuffer->tempCount = static_cast<int>(std::min(systemInfo.temperatures.size(), static_cast<size_t>(10)));
        for (int i = 0; i < pBuffer->tempCount; ++i) {
            const auto& temp = systemInfo.temperatures[i];
            SafeCopyWideString(pBuffer->temperatures[i].sensorName, 64, WinUtils::StringToWstring(temp.first));
            pBuffer->temperatures[i].temperature = temp.second;
        }

        // 独立 CPU / GPU 温度
        pBuffer->cpuTemperature = systemInfo.cpuTemperature;
        pBuffer->gpuTemperature = systemInfo.gpuTemperature;
        pBuffer->cpuUsageSampleIntervalMs = systemInfo.cpuUsageSampleIntervalMs;

        GetSystemTime(&pBuffer->lastUpdate);
        Logger::Trace("成功写入系统/磁盘/SMART 信息到共享内存");
    } catch (const std::exception& e) {
        lastError = std::string("WriteToSharedMemory 中的异常: ") + e.what();
        Logger::Error(lastError);
    } catch (...) {
        lastError = "WriteToSharedMemory 中的未知异常";
        Logger::Error(lastError);
    }
    ReleaseMutex(g_hMutex);
}
