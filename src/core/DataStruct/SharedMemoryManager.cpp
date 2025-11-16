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

#ifdef PLATFORM_WINDOWS
// Make sure Windows.h is included before any other headers that might redefine GetLastError
#include <windows.h>
#include <wincrypt.h>
#endif

#include "SharedMemoryManager.h"
// Fix the include path case sensitivity
#include "../Utils/WinUtils.h"
#include "../Utils/Logger.h"
#include "../Utils/WmiManager.h"
#include "../Utils/MotherboardInfo.h"
#include "../usb/USBInfo.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#define SHARED_MEMORY_MANAGER_INCLUDED
#define SHARED_MEMORY_MANAGER_INCLUDED
#include "DiagnosticsPipe.h"




// Initialize static members
#ifdef PLATFORM_WINDOWS
HANDLE SharedMemoryManager::hMapFile = NULL;
HANDLE SharedMemoryManager::hMutex = NULL;
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
int SharedMemoryManager::shmFd = -1;
sem_t* SharedMemoryManager::semaphore = nullptr;
std::string SharedMemoryManager::shmName = "/SystemMonitorSharedMemory";
std::string SharedMemoryManager::semName = "/SystemMonitorSemaphore";
#endif
SharedMemoryBlock* SharedMemoryManager::pBuffer = nullptr;
std::string SharedMemoryManager::lastError = "";
void* SharedMemoryManager::serviceManager = nullptr;
USBInfoManager* SharedMemoryManager::usbManager = nullptr;

// Cross-platform TCMT_STRNCPY_S replacement
#ifdef PLATFORM_WINDOWS
    #define TCMT_STRNCPY_S(dest, destsz, src, count) strncpy_s(dest, destsz, src, count)
    #define TCMT_STRCPY_S(dest, destsz, src) TCMT_STRCPY_S(dest, destsz, src)
#else
    #define TCMT_STRNCPY_S(dest, destsz, src, count) \
        do { \
            strncpy(dest, src, std::min((size_t)(count), (size_t)(destsz))); \
            dest[std::min((size_t)(count) - 1, (size_t)(destsz) - 1)] = '\0'; \
        } while(0)
    #define TCMT_STRCPY_S(dest, destsz, src) \
        do { \
            strncpy(dest, src, (destsz) - 1); \
            dest[(destsz) - 1] = '\0'; \
        } while(0)
#endif

bool SharedMemoryManager::InitSharedMemory() {
    // Clear any previous error
    lastError.clear();
    
    // 初始化USB监控管理器
    if (!usbManager) {
        usbManager = new USBInfoManager();
        if (usbManager->Initialize()) {
            usbManager->StartMonitoring();
            Logger::Info("USB监控管理器初始化成功");
        } else {
            Logger::Warn("USB监控管理器初始化失败");
        }
    }

#ifdef PLATFORM_WINDOWS
    // Windows特定初始化
    return InitWindowsSharedMemory();
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    // POSIX共享内存初始化
    return InitPosixSharedMemory();
#else
    lastError = "不支持的平台";
    Logger::Error(lastError);
    return false;
#endif
}

#ifdef PLATFORM_WINDOWS
bool SharedMemoryManager::InitWindowsSharedMemory() {
    // 初始化WMI管理器
    if (!serviceManager) {
        serviceManager = new WmiManager();
    }
    
    try {
        // Try to enable privileges needed for creating global objects
        bool hasPrivileges = WinUtils::EnablePrivilege(L"SeCreateGlobalPrivilege");
        if (!hasPrivileges) {
            Logger::Warn("未能启用 SeCreateGlobalPrivilege - 尝试继续");
        }
    } catch(...) {
        Logger::Warn("启用 SeCreateGlobalPrivilege 时发生异常 - 尝试继续");
    }

    // 创建全局互斥体用于多进程同步
    if (!hMutex) {
        hMutex = CreateMutexW(NULL, FALSE, L"Global\\SystemMonitorSharedMemoryMutex");
        if (!hMutex) {
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
        lastError = "未能初始化安全描述符。错误码: " + std::to_string(errorCode);
        Logger::Error(lastError);
        return false;
    }

    // Set the DACL to NULL for unrestricted access
    if (!SetSecurityDescriptorDacl(&securityDescriptor, TRUE, NULL, FALSE)) {
        DWORD errorCode = ::GetLastError();
        lastError = "未能设置安全描述符 DACL。错误码: " + std::to_string(errorCode);
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
            lastError = "未能创建共享内存。错误码: " + std::to_string(errorCode);
            if (errorCode == ERROR_ALREADY_EXISTS) {
                lastError += " (共享内存已存在)";
            }
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
        errorCode = ::GetLastError();
        lastError = "未能映射共享内存视图。错误码: " + std::to_string(errorCode);
        Logger::Error(lastError);
        CloseHandle(hMapFile);
        hMapFile = NULL;
        return false;
    }

    // Zero out the shared memory to avoid dirty data (only on first creation)
    if (errorCode != ERROR_ALREADY_EXISTS) {
        memset(pBuffer, 0, sizeof(SharedMemoryBlock));
    }

    Logger::Info("Windows共享内存成功初始化.");
    StartDiagnosticsPipeThread();
    DiagnosticsPipeAppendLog("SharedMemory initialized");
    return true;
}
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
bool SharedMemoryManager::InitPosixSharedMemory() {
    // 创建或打开POSIX共享内存
    shmFd = shm_open(shmName.c_str(), O_CREAT | O_RDWR, 0666);
    if (shmFd == -1) {
        lastError = "无法打开共享内存: " + std::string(strerror(errno));
        Logger::Error(lastError);
        return false;
    }

    // 先检查当前大小
    struct stat shm_stat;
    if (fstat(shmFd, &shm_stat) == 0) {
        if (static_cast<size_t>(shm_stat.st_size) < sizeof(SharedMemoryBlock)) {
            // 只有当当前大小小于所需大小时才调整大小
            if (ftruncate(shmFd, sizeof(SharedMemoryBlock)) == -1) {
                lastError = "无法设置共享内存大小: " + std::string(strerror(errno));
                Logger::Error(lastError);
                close(shmFd);
                shmFd = -1;
                return false;
            }
        }
    } else {
        // 无法获取状态，尝试设置大小
        if (ftruncate(shmFd, sizeof(SharedMemoryBlock)) == -1) {
            lastError = "无法设置共享内存大小: " + std::string(strerror(errno));
            Logger::Error(lastError);
            close(shmFd);
            shmFd = -1;
            return false;
        }
    }

    // 映射到进程地址空间
    pBuffer = static_cast<SharedMemoryBlock*>(
        mmap(nullptr, sizeof(SharedMemoryBlock), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0)
    );
    if (pBuffer == MAP_FAILED) {
        lastError = "无法映射共享内存: " + std::string(strerror(errno));
        Logger::Error(lastError);
        close(shmFd);
        shmFd = -1;
        pBuffer = nullptr;
        return false;
    }

    // 创建或打开信号量用于同步
    semaphore = sem_open(semName.c_str(), O_CREAT, 0666, 1);
    if (semaphore == SEM_FAILED) {
        lastError = "无法创建信号量: " + std::string(strerror(errno));
        Logger::Error(lastError);
        munmap(pBuffer, sizeof(SharedMemoryBlock));
        close(shmFd);
        shmFd = -1;
        pBuffer = nullptr;
        return false;
    }

    Logger::Info("POSIX共享内存成功初始化.");
    return true;
}
#endif

void SharedMemoryManager::CleanupSharedMemory() {
    StopDiagnosticsPipeThread();
    
#ifdef PLATFORM_WINDOWS
    if (pBuffer) {
        UnmapViewOfFile(pBuffer);
        pBuffer = nullptr;
    }
    if (hMapFile) {
        CloseHandle(hMapFile);
        hMapFile = NULL;
    }
    if (hMutex) {
        CloseHandle(hMutex);
        hMutex = NULL;
    }
    if (serviceManager) {
        delete static_cast<WmiManager*>(serviceManager);
        serviceManager = nullptr;
    }
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    if (pBuffer) {
        munmap(pBuffer, sizeof(SharedMemoryBlock));
        pBuffer = nullptr;
    }
    if (shmFd != -1) {
        close(shmFd);
        shmFd = -1;
    }
    if (semaphore) {
        sem_close(semaphore);
        semaphore = nullptr;
        sem_unlink(semName.c_str());
    }
    shm_unlink(shmName.c_str());
    if (serviceManager) {
        delete serviceManager;
        serviceManager = nullptr;
    }
#endif
    
    if (usbManager) {
        usbManager->Cleanup();
        delete usbManager;
        usbManager = nullptr;
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

    // 跨平台同步
    bool lockAcquired = false;
#ifdef PLATFORM_WINDOWS
    DWORD waitResult = WaitForSingleObject(hMutex, 5000); // 最多等5秒
    if (waitResult != WAIT_OBJECT_0) {
        Logger::Critical("未能获取共享内存互斥体");
        return;
    }
    lockAcquired = true;
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    if (sem_wait(semaphore) != 0) {
        Logger::Critical("未能获取共享内存信号量");
        return;
    }
    lockAcquired = true;
#endif

    if (!lockAcquired) {
        return;
    }

    try {
        // --- writeSequence 奇偶协议：写入前设置为奇数 ---
        if (pBuffer->writeSequence % 2 == 0) {
            pBuffer->writeSequence++;
        } else {
            pBuffer->writeSequence += 2; // 保证为奇数
        }

        // 保存旧的snapshotVersion用于比较
        uint32_t oldSnapshotVersion = pBuffer->snapshotVersion;
        
        // --- 从SystemInfo转换数据到SharedMemoryBlock ---
        
        // 基础头部信息
        pBuffer->abiVersion = 0x00010014;
        pBuffer->reservedHeader = 0;

        // CPU信息
        pBuffer->cpuLogicalCores = static_cast<uint16_t>(systemInfo.logicalCores);
        if (systemInfo.cpuUsage >= 0.0 && systemInfo.cpuUsage <= 100.0) {
            pBuffer->cpuUsagePercent_x10 = static_cast<int16_t>(systemInfo.cpuUsage * 10);
        } else {
            pBuffer->cpuUsagePercent_x10 = -1; // 未实现或异常值
        }

        // 内存信息（转换为MB）
        pBuffer->memoryTotalMB = systemInfo.totalMemory / (1024 * 1024);
        pBuffer->memoryUsedMB = systemInfo.usedMemory / (1024 * 1024);

        // 温度传感器数据
        pBuffer->tempSensorCount = 0;
        memset(pBuffer->tempSensors, 0, sizeof(pBuffer->tempSensors));
        
        // 转换温度数据到TemperatureSensor结构
        for (const auto& temp : systemInfo.temperatures) {
            if (pBuffer->tempSensorCount >= 32) break; // 最多32个传感器
            
            TemperatureSensor& sensor = pBuffer->tempSensors[pBuffer->tempSensorCount];
            
            // 复制传感器名称（限制长度）
            size_t nameLen = std::min(temp.first.length(), sizeof(sensor.name) - 1);
            TCMT_STRNCPY_S(sensor.name, sizeof(sensor.name), temp.first.c_str(), nameLen);
            sensor.name[nameLen] = '\0';
            
            // 设置温度值（转换为0.1°C精度）
            if (temp.second >= -50.0 && temp.second <= 150.0) {
                sensor.valueC_x10 = static_cast<int16_t>(temp.second * 10);
                sensor.flags = 0x01; // bit0=valid
            } else {
                sensor.valueC_x10 = -1; // 无效值
                sensor.flags = 0x00;
            }
            
            pBuffer->tempSensorCount++;
        }

        // SMART磁盘数据（暂时为空，等待SMART模块实现）
        pBuffer->smartDiskCount = 0;
        memset(pBuffer->smartDisks, 0, sizeof(pBuffer->smartDisks));

        // 主板/BIOS信息采集（跨平台）
        try {
#ifdef PLATFORM_WINDOWS
            MotherboardInfo motherboardInfo = MotherboardInfoCollector::CollectMotherboardInfo(static_cast<IWbemServices*>(GetSystemService()));
            if (motherboardInfo.isValid) {
                TCMT_STRNCPY_S(pBuffer->baseboardManufacturer, sizeof(pBuffer->baseboardManufacturer), 
                           motherboardInfo.manufacturer.c_str(), 255);
                TCMT_STRNCPY_S(pBuffer->baseboardProduct, sizeof(pBuffer->baseboardProduct), 
                           motherboardInfo.product.c_str(), 255);
                TCMT_STRNCPY_S(pBuffer->baseboardVersion, sizeof(pBuffer->baseboardVersion), 
                           motherboardInfo.version.c_str(), 255);
                TCMT_STRNCPY_S(pBuffer->baseboardSerial, sizeof(pBuffer->baseboardSerial), 
                           motherboardInfo.serialNumber.c_str(), 255);
                TCMT_STRNCPY_S(pBuffer->biosVendor, sizeof(pBuffer->biosVendor), 
                           motherboardInfo.biosVendor.c_str(), 255);
                TCMT_STRNCPY_S(pBuffer->biosVersion, sizeof(pBuffer->biosVersion), 
                           motherboardInfo.biosVersion.c_str(), 255);
                TCMT_STRNCPY_S(pBuffer->biosDate, sizeof(pBuffer->biosDate), 
                           motherboardInfo.biosReleaseDate.c_str(), 255);
            } else {
                SetDefaultMotherboardInfo();
            }
#elif defined(PLATFORM_MACOS)
            CollectMacMotherboardInfo();
#elif defined(PLATFORM_LINUX)
            CollectLinuxMotherboardInfo();
#endif
        } catch (const std::exception& e) {
            Logger::Error("采集主板/BIOS信息异常: " + std::string(e.what()));
            SetDefaultMotherboardInfo();
        }

        // TPM信息
        pBuffer->secureBootEnabled = 0; // 待实现
        pBuffer->tpmPresent = systemInfo.hasTpm ? 1 : 0;
        pBuffer->memorySlotsTotal = 0; // 待实现
        pBuffer->memorySlotsUsed = 0;  // 待实现

        // USB设备信息
        pBuffer->usbDeviceCount = 0;
        memset(pBuffer->usbDevices, 0, sizeof(pBuffer->usbDevices));
        
        if (usbManager) {
            try {
                auto usbDevices = usbManager->GetCurrentUSBDevices();
                pBuffer->usbDeviceCount = static_cast<uint8_t>(std::min(usbDevices.size(), size_t(8)));
                
                for (size_t i = 0; i < pBuffer->usbDeviceCount; ++i) {
                    const auto& usbDevice = usbDevices[i];
                    auto& usbData = pBuffer->usbDevices[i];
                    
                    // 复制驱动器路径
                    TCMT_STRNCPY_S(usbData.drivePath, sizeof(usbData.drivePath), 
                              usbDevice.drivePath.c_str(), 255);
                    
                    // 复制卷标名称
                    TCMT_STRNCPY_S(usbData.volumeLabel, sizeof(usbData.volumeLabel), 
                              usbDevice.volumeLabel.c_str(), 255);
                    
                    // 设置容量信息
                    usbData.totalSize = usbDevice.totalSize;
                    usbData.freeSpace = usbDevice.freeSpace;
                    
                    // 设置状态
                    usbData.isUpdateReady = usbDevice.isUpdateReady ? 1 : 0;
                    usbData.state = static_cast<uint8_t>(usbDevice.state);
                    usbData.reserved = 0;
                    
                    // 设置时间
#ifdef PLATFORM_WINDOWS
                    usbData.lastUpdate.windowsTime = usbDevice.lastUpdate;
#else
                    usbData.lastUpdate.unixTime = usbDevice.lastUpdate;
                    usbData.lastUpdate.milliseconds = 0;
#endif
                }
            } catch (const std::exception& e) {
                Logger::Error("获取USB设备信息异常: " + std::string(e.what()));
                pBuffer->usbDeviceCount = 0;
            }
        }

        // 预留字段
        memset(pBuffer->futureReserved, 0, sizeof(pBuffer->futureReserved));
        memset(pBuffer->extensionPad, 0, sizeof(pBuffer->extensionPad));

        // 计算SHA256哈希
#ifdef PLATFORM_WINDOWS
        try {
            // 使用Windows Cryptography API计算SHA256
            HCRYPTPROV hCryptProv = NULL;
            HCRYPTHASH hHash = NULL;
            BYTE hash[32];
            DWORD hashLen = sizeof(hash);
            
            // 获取加密服务提供者
            if (CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
                // 创建哈希对象
                if (CryptCreateHash(hCryptProv, CALG_SHA_256, 0, 0, &hHash)) {
                    // 计算哈希值（排除哈希字段本身）
                    DWORD dataSize = sizeof(SharedMemoryBlock) - sizeof(pBuffer->sharedmemHash);
                    if (CryptHashData(hHash, (BYTE*)pBuffer, dataSize, 0)) {
                        // 获取哈希值
                        if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
                            // 复制哈希值到共享内存结构
                            memcpy(pBuffer->sharedmemHash, hash, sizeof(pBuffer->sharedmemHash));
                        } else {
                            Logger::Error("获取SHA256哈希值失败");
                        }
                    } else {
                        Logger::Error("计算SHA256哈希失败");
                    }
                    
                    CryptDestroyHash(hHash);
                } else {
                    Logger::Error("创建SHA256哈希对象失败");
                }
                
                CryptReleaseContext(hCryptProv, 0);
            } else {
                Logger::Error("获取加密服务提供者失败");
            }
        }
        catch (const std::exception& e) {
            Logger::Error("SHA256计算异常: " + std::string(e.what()));
            // 异常时保持哈希字段为0
            memset(pBuffer->sharedmemHash, 0, sizeof(pBuffer->sharedmemHash));
        }
        catch (...) {
            Logger::Error("SHA256计算未知异常");
            memset(pBuffer->sharedmemHash, 0, sizeof(pBuffer->sharedmemHash));
        }
#else
        // 非Windows平台暂时不计算哈希，保持为0
        memset(pBuffer->sharedmemHash, 0, sizeof(pBuffer->sharedmemHash));
#endif

        // 检查是否有实际数据更新，如果有则增加snapshotVersion
        bool hasDataUpdate = false;
        // 简单的更新检测：CPU使用率、内存使用、温度传感器数量或数据变化
        if (pBuffer->cpuUsagePercent_x10 != -1 || 
            pBuffer->memoryUsedMB > 0 || 
            pBuffer->tempSensorCount > 0) {
            hasDataUpdate = true;
        }

        if (hasDataUpdate) {
            pBuffer->snapshotVersion = oldSnapshotVersion + 1;
        }

        // --- 写入完成后设置为偶数 ---
        if (pBuffer->writeSequence % 2 == 1) {
            pBuffer->writeSequence++;
        }

        Logger::Trace("共享内存数据更新完成");
        DiagnosticsPipeAppendLog("Write sequence=" + std::to_string(pBuffer->writeSequence) + 
                                " Snapshot=" + std::to_string(pBuffer->snapshotVersion) +
                                " TempSensors=" + std::to_string(pBuffer->tempSensorCount));
    }
    catch (const std::exception& e) {
        lastError = std::string("WriteToSharedMemory 中的异常: ") + e.what();
        Logger::Error(lastError);
        
        // 异常情况下恢复偶数序列
        if (pBuffer->writeSequence % 2 == 1) {
            pBuffer->writeSequence++;
        }
    }
    catch (...) {
        lastError = "WriteToSharedMemory 中的未知异常";
        Logger::Error(lastError);
        
        // 异常情况下恢复偶数序列
        if (pBuffer->writeSequence % 2 == 1) {
            pBuffer->writeSequence++;
        }
    }
    
    // 释放跨平台同步锁
#ifdef PLATFORM_WINDOWS
    ReleaseMutex(hMutex);
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    sem_post(semaphore);
#endif
}

void* SharedMemoryManager::GetSystemService() {
#ifdef PLATFORM_WINDOWS
    if (serviceManager && static_cast<WmiManager*>(serviceManager)->IsInitialized()) {
        return static_cast<WmiManager*>(serviceManager)->GetWmiService();
    }
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    // macOS/Linux系统服务访问
    return serviceManager;
#endif
    return nullptr;
}

#ifdef PLATFORM_MACOS
void SharedMemoryManager::CollectMacMotherboardInfo() {
    // 使用system_profiler获取主板信息
    FILE* pipe = popen("system_profiler SPHardwareDataType", "r");
    if (!pipe) {
        SetDefaultMotherboardInfo();
        return;
    }
    
    char buffer[512];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    pclose(pipe);
    
    // 解析主板信息
    std::istringstream iss(result);
    std::string line;
    bool foundManufacturer = false;
    
    while (std::getline(iss, line)) {
        if (line.find("Manufacturer:") != std::string::npos) {
            std::string manufacturer = line.substr(line.find(":") + 2);
            manufacturer.erase(0, manufacturer.find_first_not_of(" \t"));
            manufacturer.erase(manufacturer.find_last_not_of(" \t\r\n") + 1);
            TCMT_STRNCPY_S(pBuffer->baseboardManufacturer, sizeof(pBuffer->baseboardManufacturer), 
                       manufacturer.c_str(), 255);
            foundManufacturer = true;
        }
        // 可以添加更多字段解析
    }
    
    if (!foundManufacturer) {
        SetDefaultMotherboardInfo();
    }
}
#elif defined(PLATFORM_LINUX)
void SharedMemoryManager::CollectLinuxMotherboardInfo() {
    // 从/proc/cpuinfo和DMI获取主板信息
    std::ifstream dmi("/sys/class/dmi/id/board_vendor");
    if (dmi.is_open()) {
        std::string vendor;
        std::getline(dmi, vendor);
        TCMT_STRNCPY_S(pBuffer->baseboardManufacturer, sizeof(pBuffer->baseboardManufacturer), 
                   vendor.c_str(), 255);
        dmi.close();
    } else {
        SetDefaultMotherboardInfo();
    }
}
#endif

void SharedMemoryManager::SetDefaultMotherboardInfo() {
    TCMT_STRCPY_S(pBuffer->baseboardManufacturer, sizeof(pBuffer->baseboardManufacturer), "未知");
    TCMT_STRCPY_S(pBuffer->baseboardProduct, sizeof(pBuffer->baseboardProduct), "未知");
    TCMT_STRCPY_S(pBuffer->baseboardVersion, sizeof(pBuffer->baseboardVersion), "未知");
    TCMT_STRCPY_S(pBuffer->baseboardSerial, sizeof(pBuffer->baseboardSerial), "未知");
    TCMT_STRCPY_S(pBuffer->biosVendor, sizeof(pBuffer->biosVendor), "未知");
    TCMT_STRCPY_S(pBuffer->biosVersion, sizeof(pBuffer->biosVersion), "未知");
    TCMT_STRCPY_S(pBuffer->biosDate, sizeof(pBuffer->biosDate), "未知");
}
