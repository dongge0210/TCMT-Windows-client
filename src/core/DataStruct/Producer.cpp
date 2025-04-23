#include "DataStruct.h"
#include "../utils/WinUtils.h"
#include <iostream>

HANDLE hMapFile = NULL;
SharedMemoryBlock* pBuffer = nullptr;

bool InitSharedMemory() {
    // 创建文件映射对象
    hMapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(SharedMemoryBlock),
        L"Global\\SystemMonitorSharedMemory"
    );

    if (!hMapFile) {
        std::cerr << "无法创建共享内存 (Error: " << GetLastError() << ")" << std::endl;
        return false;
    }

    // 映射到进程地址空间
    pBuffer = (SharedMemoryBlock*)MapViewOfFile(
        hMapFile,
        FILE_MAP_ALL_ACCESS,
        0, 0, sizeof(SharedMemoryBlock)
    );

    if (!pBuffer) {
        CloseHandle(hMapFile);
        std::cerr << "无法映射共享内存视图" << std::endl;
        return false;
    }

    // 初始化临界区
    InitializeCriticalSection(&pBuffer->lock);
    return true;
}

// Ensure data is written correctly to shared memory
void WriteToSharedMemory(const SystemInfo& sysInfo) {
    if (!pBuffer) return;

    EnterCriticalSection(&pBuffer->lock);
    {
        // Write CPU information
        strcpy_s(pBuffer->cpuName, sizeof(pBuffer->cpuName), sysInfo.cpuName.c_str());
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
        pBuffer->gpuCount = std::min(2, static_cast<int>(sysInfo.gpus.size()));
        for (int i = 0; i < pBuffer->gpuCount; i++) {
            const auto& gpu = sysInfo.gpus[i];
            wcscpy_s(pBuffer->gpus[i].name, sizeof(pBuffer->gpus[i].name)/sizeof(wchar_t), gpu.name.c_str());
            wcscpy_s(pBuffer->gpus[i].brand, sizeof(pBuffer->gpus[i].brand)/sizeof(wchar_t), gpu.brand.c_str());
            pBuffer->gpus[i].memory = gpu.memory;
            pBuffer->gpus[i].coreClock = gpu.coreClock;
        }

        // Write network adapter information
        pBuffer->adapterCount = std::min(4, static_cast<int>(sysInfo.adapters.size()));
        for (int i = 0; i < pBuffer->adapterCount; i++) {
            const auto& adapter = sysInfo.adapters[i];
            wcscpy_s(pBuffer->adapters[i].name, sizeof(pBuffer->adapters[i].name)/sizeof(wchar_t), adapter.name.c_str());
            wcscpy_s(pBuffer->adapters[i].mac, sizeof(pBuffer->adapters[i].mac)/sizeof(wchar_t), adapter.mac.c_str());
            pBuffer->adapters[i].speed = adapter.speed;
        }

        // Write disk information
        pBuffer->diskCount = std::min(8, static_cast<int>(sysInfo.disks.size()));
        for (int i = 0; i < pBuffer->diskCount; i++) {
            const auto& disk = sysInfo.disks[i];
            pBuffer->disks[i].letter = disk.letter;
            wcscpy_s(pBuffer->disks[i].label, sizeof(pBuffer->disks[i].label)/sizeof(wchar_t), disk.label.c_str());
            wcscpy_s(pBuffer->disks[i].fileSystem, sizeof(pBuffer->disks[i].fileSystem)/sizeof(wchar_t), disk.fileSystem.c_str());
            pBuffer->disks[i].totalSize = disk.totalSize;
            pBuffer->disks[i].usedSpace = disk.usedSpace;
            pBuffer->disks[i].freeSpace = disk.totalSize - disk.usedSpace; // Calculate freeSpace
        }

        // Write temperature data
        pBuffer->tempCount = std::min(10, static_cast<int>(sysInfo.temperatures.size()));
        for (int i = 0; i < pBuffer->tempCount; i++) {
            const auto& temp = sysInfo.temperatures[i];
            wcscpy_s(pBuffer->temperatures[i].sensorName, sizeof(pBuffer->temperatures[i].sensorName)/sizeof(wchar_t),
                     WinUtils::StringToWstring(temp.first).c_str());
            pBuffer->temperatures[i].temperature = temp.second;
        }

        // Update timestamp
        GetSystemTime(&pBuffer->lastUpdate);
    }
    LeaveCriticalSection(&pBuffer->lock);
}

void CleanupSharedMemory() {
    if (pBuffer) {
        UnmapViewOfFile(pBuffer);
        pBuffer = nullptr;
    }
    if (hMapFile) {
        CloseHandle(hMapFile);
        hMapFile = NULL;
    }
}