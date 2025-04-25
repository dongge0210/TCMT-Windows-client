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

#include "SharedMemoryManager.h"
// Fix the include path case sensitivity
#include "../Utils/WinUtils.h"
#include "../Utils/Logger.h"
#include <iostream>

// Initialize static members
HANDLE SharedMemoryManager::hMapFile = NULL;
SharedMemoryBlock* SharedMemoryManager::pBuffer = nullptr;

bool SharedMemoryManager::InitSharedMemory() {
    // Create file mapping object
    hMapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(SharedMemoryBlock),
        L"Global\\SystemMonitorSharedMemory"
    );

    if (!hMapFile) {
        std::cerr << "Unable to create shared memory (Error: " << GetLastError() << ")" << std::endl;
        return false;
    }

    // Map to process address space
    pBuffer = (SharedMemoryBlock*)MapViewOfFile(
        hMapFile,
        FILE_MAP_ALL_ACCESS,
        0, 0, sizeof(SharedMemoryBlock)
    );

    if (!pBuffer) {
        CloseHandle(hMapFile);
        std::cerr << "Unable to map shared memory view" << std::endl;
        return false;
    }

    // Initialize critical section
    InitializeCriticalSection(&pBuffer->lock);
    return true;
}

void SharedMemoryManager::WriteToSharedMemory(const SystemInfo& sysInfo) {
    if (!pBuffer) return;

    EnterCriticalSection(&pBuffer->lock);
    {
        // Write CPU information
        // Use the class static method correctly without extra scope resolution
        wcscpy_s(pBuffer->cpuName, sizeof(pBuffer->cpuName) / sizeof(wchar_t),
            WinUtils::StringToWstring(sysInfo.cpuName).c_str());
        pBuffer->physicalCores = sysInfo.physicalCores;
        pBuffer->logicalCores = sysInfo.logicalCores;
        pBuffer->cpuUsage = static_cast<float>(sysInfo.cpuUsage);
        pBuffer->performanceCores = sysInfo.performanceCores;
        pBuffer->efficiencyCores = sysInfo.efficiencyCores;
        pBuffer->pCoreFreq = sysInfo.performanceCoreFreq;
        pBuffer->eCoreFreq = sysInfo.efficiencyCoreFreq;
        pBuffer->hyperThreading = sysInfo.hyperThreading;
        pBuffer->virtualization = sysInfo.virtualization;

        // Write memory information
        pBuffer->totalMemory = sysInfo.totalMemory;
        pBuffer->usedMemory = sysInfo.usedMemory;
        pBuffer->availableMemory = sysInfo.availableMemory;

        // Write GPU information
        pBuffer->gpuCount = std::min<int>(2, static_cast<int>(sysInfo.gpus.size()));
        for (int i = 0; i < pBuffer->gpuCount; i++) {
            const auto& gpu = sysInfo.gpus[i];
            wcscpy_s(pBuffer->gpus[i].name, sizeof(pBuffer->gpus[i].name) / sizeof(wchar_t), gpu.name);
            wcscpy_s(pBuffer->gpus[i].brand, sizeof(pBuffer->gpus[i].brand) / sizeof(wchar_t), gpu.brand);
            pBuffer->gpus[i].memory = gpu.memory;
            pBuffer->gpus[i].coreClock = gpu.coreClock;
        }

        // Write network adapter information
        pBuffer->adapterCount = std::min<int>(4, static_cast<int>(sysInfo.adapters.size()));
        for (int i = 0; i < pBuffer->adapterCount; i++) {
            const auto& adapter = sysInfo.adapters[i];
            wcscpy_s(pBuffer->adapters[i].name, sizeof(pBuffer->adapters[i].name) / sizeof(wchar_t), adapter.name);
            wcscpy_s(pBuffer->adapters[i].mac, sizeof(pBuffer->adapters[i].mac) / sizeof(wchar_t), adapter.mac);
            pBuffer->adapters[i].speed = adapter.speed;
        }

        // Write disk information
        pBuffer->diskCount = std::min<int>(8, static_cast<int>(sysInfo.disks.size()));
        for (int i = 0; i < pBuffer->diskCount; i++) {
            const auto& disk = sysInfo.disks[i];
            pBuffer->disks[i].letter = disk.letter;

            // Convert std::string to std::wstring for label and fileSystem
            std::wstring wLabel = WinUtils::StringToWstring(disk.label);
            std::wstring wFileSystem = WinUtils::StringToWstring(disk.fileSystem);
            wcscpy_s(pBuffer->disks[i].label, sizeof(pBuffer->disks[i].label) / sizeof(wchar_t), wLabel.c_str());
            wcscpy_s(pBuffer->disks[i].fileSystem, sizeof(pBuffer->disks[i].fileSystem) / sizeof(wchar_t), wFileSystem.c_str());

            pBuffer->disks[i].totalSize = disk.totalSize;
            pBuffer->disks[i].usedSpace = disk.usedSpace;
            pBuffer->disks[i].freeSpace = disk.totalSize - disk.usedSpace;
        }

        // Write temperature data
        pBuffer->tempCount = std::min<int>(10, static_cast<int>(sysInfo.temperatures.size()));
        for (int i = 0; i < pBuffer->tempCount; i++) {
            const auto& temp = sysInfo.temperatures[i];
            wcscpy_s(pBuffer->temperatures[i].sensorName, sizeof(pBuffer->temperatures[i].sensorName) / sizeof(wchar_t),
                WinUtils::StringToWstring(temp.first).c_str());
            pBuffer->temperatures[i].temperature = temp.second;
        }

        // Update timestamp
        GetSystemTime(&pBuffer->lastUpdate);
    }
    LeaveCriticalSection(&pBuffer->lock);
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

// 如果需要，恢复 Qt 宏定义
#ifdef SAVE_QT_KEYWORDS
#undef SAVE_QT_KEYWORDS
// 可以在这里恢复特定的宏定义（如果需要的话）
#endif
