// SharedMemoryManager_macOS.cpp - macOS平台共享内存管理器实现
// 使用POSIX共享内存和Platform抽象层

#ifndef TCMT_MACOS
#error "This file should only be compiled for macOS platform (TCMT_MACOS defined)"
#endif

#include "SharedMemoryManager.h"
#include "../Platform/Platform.h"
#include "../Utils/Logger.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <algorithm>

// 定义静态成员（macOS平台）
void* SharedMemoryManager::shmPtr = nullptr;
SharedMemoryBlock* SharedMemoryManager::pBuffer = nullptr;
std::string SharedMemoryManager::lastError = "";
void* SharedMemoryManager::interprocessMutex = nullptr;

// 辅助函数：安全拷贝宽字符串
static void SafeCopyWideString(wchar_t* dest, size_t destSize, const std::wstring& src) {
    try {
        if (dest == nullptr || destSize == 0) return;
        memset(dest, 0, destSize * sizeof(wchar_t));
        if (src.empty()) { dest[0] = L'\0'; return; }
        size_t copyLen = std::min(src.length(), destSize - 1);
        for (size_t i = 0; i < copyLen; ++i) dest[i] = src[i];
        dest[copyLen] = L'\0';
    } catch (...) { if (dest && destSize > 0) dest[0] = L'\0'; }
}

// 辅助函数：从宽字符数组安全拷贝
static void SafeCopyFromWideArray(wchar_t* dest, size_t destSize, const wchar_t* src, size_t srcCapacity) {
    if (!dest || destSize == 0) return;
    memset(dest, 0, destSize * sizeof(wchar_t));
    if (!src) return;
    size_t len = 0;
    while (len < srcCapacity && src[len] != L'\0') ++len;
    if (len >= destSize) len = destSize - 1;
    for (size_t i = 0; i < len; ++i) dest[i] = src[i];
    dest[len] = L'\0';
}

bool SharedMemoryManager::InitSharedMemory() {
    // 清空之前的错误
    lastError.clear();

    // 创建或打开进程间互斥锁
    if (!interprocessMutex) {
        auto* mutex = new Platform::InterprocessMutex();
        if (!mutex->Create("SystemMonitorSharedMemoryMutex")) {
            lastError = "未能创建进程间互斥锁: " + mutex->GetLastError();
            Logger::Error(lastError);
            delete mutex;
            return false;
        }
        interprocessMutex = mutex;
    }

    // 创建共享内存对象
    if (!shmPtr) {
        auto* shm = new Platform::SharedMemory();
        if (!shm->Create("SystemMonitorSharedMemory", sizeof(SharedMemoryBlock))) {
            lastError = "未能创建共享内存: " + shm->GetLastError();
            Logger::Error(lastError);
            delete shm;
            return false;
        }

        // 映射到进程地址空间
        if (!shm->Map()) {
            lastError = "未能映射共享内存: " + shm->GetLastError();
            Logger::Error(lastError);
            delete shm;
            return false;
        }

        shmPtr = shm;
        pBuffer = static_cast<SharedMemoryBlock*>(shm->GetAddress());

        // 如果是新创建的共享内存，清零
        if (shm->IsCreated()) {
            memset(pBuffer, 0, sizeof(SharedMemoryBlock));
            Logger::Info("创建了新的共享内存映射.");
        } else {
            Logger::Info("打开了现有的共享内存映射.");
        }

        Logger::Info("共享内存成功初始化.");
        return true;
    }

    // 如果已经初始化，检查是否仍然有效
    auto* shm = static_cast<Platform::SharedMemory*>(shmPtr);
    if (!shm->GetAddress()) {
        lastError = "共享内存已初始化但映射无效";
        Logger::Error(lastError);
        return false;
    }

    return true;
}

void SharedMemoryManager::CleanupSharedMemory() {
    // 清理共享内存
    if (shmPtr) {
        auto* shm = static_cast<Platform::SharedMemory*>(shmPtr);
        shm->Unmap();
        delete shm;
        shmPtr = nullptr;
        pBuffer = nullptr;
    }

    // 清理互斥锁
    if (interprocessMutex) {
        auto* mutex = static_cast<Platform::InterprocessMutex*>(interprocessMutex);
        delete mutex;
        interprocessMutex = nullptr;
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

    // 获取互斥锁
    auto* mutex = static_cast<Platform::InterprocessMutex*>(interprocessMutex);
    if (!mutex) {
        lastError = "进程间互斥锁未初始化";
        Logger::Critical(lastError);
        return;
    }

    if (!mutex->Lock(5000)) { // 最多等5秒
        Logger::Critical("未能获取共享内存互斥锁");
        return;
    }

    try {
        // 清零主要字符串/数组区域
        memset(pBuffer->cpuName, 0, sizeof(pBuffer->cpuName));
        for (int i = 0; i < 2; ++i) {
            memset(pBuffer->gpus[i].name, 0, sizeof(pBuffer->gpus[i].name));
            memset(pBuffer->gpus[i].brand, 0, sizeof(pBuffer->gpus[i].brand));
        }
        for (int i = 0; i < 8; ++i) {
            memset(&pBuffer->disks[i], 0, sizeof(pBuffer->disks[i]));
            memset(&pBuffer->physicalDisks[i], 0, sizeof(pBuffer->physicalDisks[i]));
        }
        for (int i = 0; i < 10; ++i) {
            memset(pBuffer->temperatures[i].sensorName, 0, sizeof(pBuffer->temperatures[i].sensorName));
        }

        // CPU信息
        SafeCopyWideString(pBuffer->cpuName, 128,
                          Platform::StringConverter::Utf8ToWide(systemInfo.cpuName));
        pBuffer->physicalCores = systemInfo.physicalCores;
        pBuffer->logicalCores = systemInfo.logicalCores;
        pBuffer->cpuUsage = systemInfo.cpuUsage;
        pBuffer->performanceCores = systemInfo.performanceCores;
        pBuffer->efficiencyCores = systemInfo.efficiencyCores;
        pBuffer->pCoreFreq = systemInfo.performanceCoreFreq;
        pBuffer->eCoreFreq = systemInfo.efficiencyCoreFreq;
        pBuffer->hyperThreading = systemInfo.hyperThreading;
        pBuffer->virtualization = systemInfo.virtualization;

        // 内存信息
        pBuffer->totalMemory = systemInfo.totalMemory;
        pBuffer->usedMemory = systemInfo.usedMemory;
        pBuffer->availableMemory = systemInfo.availableMemory;

        // GPU信息（兼容旧字段）
        pBuffer->gpuCount = 0;
        if (!systemInfo.gpuName.empty()) {
            SafeCopyWideString(pBuffer->gpus[0].name, 128,
                              Platform::StringConverter::Utf8ToWide(systemInfo.gpuName));
            SafeCopyWideString(pBuffer->gpus[0].brand, 64,
                              Platform::StringConverter::Utf8ToWide(systemInfo.gpuBrand));
            pBuffer->gpus[0].memory = systemInfo.gpuMemory;
            pBuffer->gpus[0].coreClock = systemInfo.gpuCoreFreq;
            pBuffer->gpus[0].isVirtual = systemInfo.gpuIsVirtual;
            pBuffer->gpus[0].usage = systemInfo.gpuUsage;
            pBuffer->gpuCount = 1;
        }
        // 如后续要支持 vector<GPUData> 可在此扩展

        // 网络适配器信息
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

        // 兼容旧的网络适配器字段
        if (adapterWriteCount == 0 && !systemInfo.networkAdapterName.empty()) {
            SafeCopyWideString(pBuffer->adapters[0].name, 128,
                              Platform::StringConverter::Utf8ToWide(systemInfo.networkAdapterName));
            SafeCopyWideString(pBuffer->adapters[0].mac, 32,
                              Platform::StringConverter::Utf8ToWide(systemInfo.networkAdapterMac));
            SafeCopyWideString(pBuffer->adapters[0].ipAddress, 64,
                              Platform::StringConverter::Utf8ToWide(systemInfo.networkAdapterIp));
            SafeCopyWideString(pBuffer->adapters[0].adapterType, 32,
                              Platform::StringConverter::Utf8ToWide(systemInfo.networkAdapterType));
            pBuffer->adapters[0].speed = systemInfo.networkAdapterSpeed;
            pBuffer->adapterCount = 1;
        }

        // 逻辑磁盘信息
        pBuffer->diskCount = static_cast<int>(std::min(systemInfo.disks.size(), static_cast<size_t>(8)));
        for (int i = 0; i < pBuffer->diskCount; ++i) {
            const auto& disk = systemInfo.disks[i];
            pBuffer->disks[i].letter = disk.letter;

            // 处理卷标编码
            std::string safeLabel = disk.label;
            if (safeLabel.empty()) {
                safeLabel = "";
            } else if (!Platform::StringConverter::IsValidUtf8(safeLabel)) {
                // 如果不是有效的UTF-8，尝试使用当前locale转换
                // 简化处理：直接使用原始字符串
            }

            SafeCopyWideString(pBuffer->disks[i].label, 128,
                              Platform::StringConverter::Utf8ToWide(safeLabel));
            SafeCopyWideString(pBuffer->disks[i].fileSystem, 32,
                              Platform::StringConverter::Utf8ToWide(disk.fileSystem));
            pBuffer->disks[i].totalSize = disk.totalSize;
            pBuffer->disks[i].usedSpace = disk.usedSpace;
            pBuffer->disks[i].freeSpace = disk.freeSpace;
        }

        // 物理磁盘SMART信息
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

            // 逻辑驱动器盘符
            int ldCount = 0;
            for (char l : src.logicalDriveLetters) {
                if (ldCount >= 8 || l == 0) break;
                if (std::isalpha(static_cast<unsigned char>(l))) {
                    pBuffer->physicalDisks[i].logicalDriveLetters[ldCount++] = l;
                }
            }
            pBuffer->physicalDisks[i].logicalDriveCount = ldCount;

            // SMART属性
            int attrCount = src.attributeCount;
            if (attrCount < 0) attrCount = 0;
            if (attrCount > 32) attrCount = 32;
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

        // 温度传感器信息
        pBuffer->tempCount = static_cast<int>(std::min(systemInfo.temperatures.size(), static_cast<size_t>(10)));
        for (int i = 0; i < pBuffer->tempCount; ++i) {
            const auto& temp = systemInfo.temperatures[i];
            SafeCopyWideString(pBuffer->temperatures[i].sensorName, 64,
                              Platform::StringConverter::Utf8ToWide(temp.first));
            pBuffer->temperatures[i].temperature = temp.second;
        }

        // 独立 CPU/GPU 温度
        pBuffer->cpuTemperature = systemInfo.cpuTemperature;
        pBuffer->gpuTemperature = systemInfo.gpuTemperature;
        pBuffer->cpuUsageSampleIntervalMs = systemInfo.cpuUsageSampleIntervalMs;

        // TPM 数据 (macOS 无 TPM，清零)
        memset(&pBuffer->tpm, 0, sizeof(TpmInfo));
        pBuffer->tpmCount = 0;

        // 更新时间戳
        pBuffer->lastUpdate = Platform::SystemTime::Now();

        Logger::Trace("成功写入系统/磁盘/SMART 信息到共享内存");
    } catch (const std::exception& e) {
        lastError = std::string("WriteToSharedMemory 中的异常: ") + e.what();
        Logger::Error(lastError);
    } catch (...) {
        lastError = "WriteToSharedMemory 中的未知异常";
        Logger::Error(lastError);
    }

    // 释放互斥锁
    mutex->Unlock();
}