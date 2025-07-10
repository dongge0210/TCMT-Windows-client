#include "core/cpu/CpuInfo.h"
#include "core/gpu/GpuInfo.h"
#include "core/memory/MemoryInfo.h"
#include "core/network/NetworkAdapter.h"
#include "core/os/OSInfo.h"
#include "core/utils/Logger.h"
#include "core/utils/TimeUtils.h"
#include "core/utils/WinUtils.h"
#include "core/temperature/LibreHardwareMonitorBridge.h"
#include "core/utils/WmiManager.h"
#include "core/disk/DiskInfo.h"
#include "core/DataStruct/DataStruct.h"
#include "core/DataStruct/SharedMemoryManager.h"  // Include the new shared memory manager
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <windows.h>
#include <utility>
#include <thread>
#include <io.h>
#include <fcntl.h>
#include <algorithm> // Include for std::transform
#include <shellapi.h>
#include <sddl.h>
#include <Aclapi.h>

//QT已在本体软件因为COM证实为无效，QTUI将单独UI通过内存共享显示

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")

// Remove the global variables for shared memory - now in SharedMemoryManager

//辅助函数
// 硬件名称翻译
std::string TranslateHardwareName(const std::string& name) {
    if (name.find("CPU Package") != std::string::npos) return "CPU温度";
    if (name.find("GPU Core") != std::string::npos) return "GPU温度";
    return name;
}

// 品牌判断
std::string GetGpuBrand(const std::wstring& name) {
    if (name.find(L"NVIDIA") != std::wstring::npos) return "NVIDIA";
    if (name.find(L"AMD") != std::wstring::npos) return "AMD";
    if (name.find(L"Intel") != std::wstring::npos) return "Intel";
    return "未知";
}

// 网络速度单位
std::string FormatNetworkSpeed(double speedBps) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1);

    if (speedBps >= 1000000000) {
        ss << (speedBps / 1000000000) << " Gbps";
    }
    else if (speedBps >= 1000000) {
        ss << (speedBps / 1000000) << " Mbps";
    }
    else if (speedBps >= 1000) {
        ss << (speedBps / 1000) << " Kbps";
    }
    else {
        ss << speedBps << " bps";
    }
    return ss.str();
}

// 时间格式化
std::string FormatDateTime(const std::chrono::system_clock::time_point& tp) {
    auto time = std::chrono::system_clock::to_time_t(tp);
    struct tm timeinfo;
    localtime_s(&timeinfo, &time);  // 使用安全版本

    std::stringstream ss;
    ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string FormatFrequency(double value) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1);

    if (value >= 1000) {
        ss << (value / 1000.0) << " GHz";
    }
    else {
        ss << value << " MHz";
    }
    return ss.str();
}

std::string FormatPercentage(double value) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << value << "%";
    return ss.str();
}

std::string FormatTemperature(double value) {
    std::stringstream ss;
    ss << static_cast<int>(value) << "°C";  // 显示整数温度
    return ss.str();
}

std::string FormatSize(uint64_t bytes, bool useBinary = true) {
    const double kb = useBinary ? 1024.0 : 1000.0;
    const double mb = kb * kb;
    const double gb = mb * kb;
    const double tb = gb * kb;

    std::stringstream ss;
    ss << std::fixed << std::setprecision(1);

    if (bytes >= tb) ss << (bytes / tb) << " TB";
    else if (bytes >= gb) ss << (bytes / gb) << " GB";
    else if (bytes >= mb) ss << (bytes / mb) << " MB";
    else if (bytes >= kb) ss << (bytes / kb) << " KB";
    else ss << bytes << " B";

    return ss.str();
}

std::string FormatDiskUsage(uint64_t used, uint64_t total) {
    if (total == 0) return "0%";
    double percentage = (static_cast<double>(used) / total) * 100.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << percentage << "%";
    return ss.str();
}

static void PrintSectionHeader(const std::string& title) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, 14); // 黄色
    std::cout << "\n=== " << title << " ===" << std::endl;
    SetConsoleTextAttribute(hConsole, 7); // 恢复默认颜色
}

static void PrintInfoItem(const std::string& label, const std::string& value, int indent = 2) {
    std::cout << std::string(indent, ' ')
        << std::left << std::setw(25) << label
        << ": " << value << std::endl;
}

// Remove InitSharedMemory and WriteToSharedMemory functions - now in SharedMemoryManager

// 将SystemInfo转换为共享内存格式
void ConvertToSharedData(const SystemInfo& sysInfo, SharedMemoryBlock& sharedData) {
    wcscpy_s(sharedData.cpuName, sizeof(sharedData.cpuName) / sizeof(wchar_t), WinUtils::StringToWstring(sysInfo.cpuName).c_str());

    if (!sysInfo.gpus.empty()) {
        // Use the correct types and properly calculate the size for wcscpy_s
        wcscpy_s(sharedData.gpus[0].name, sizeof(sharedData.gpus[0].name) / sizeof(wchar_t),
                 WinUtils::StringToWstring(sysInfo.gpuName).c_str());
        wcscpy_s(sharedData.gpus[0].brand, sizeof(sharedData.gpus[0].brand) / sizeof(wchar_t),
                 WinUtils::StringToWstring(sysInfo.gpuBrand).c_str());
    }
}

//主要函数
// 检查是否以管理员身份运行
bool IsRunAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

int main(int argc, char* argv[]) {
    try {
        // Set console output to UTF-8
        _setmode(_fileno(stdout), _O_U8TEXT);

        Logger::EnableConsoleOutput(true); // Enable console output for Logger
        Logger::Initialize("system_monitor.log");
        Logger::SetLogLevel(LOG_INFO); // 设置日志等级为INFO，减少DEBUG日志
        Logger::Info("程序启动");

        // 检查管理员权限
        if (!IsRunAsAdmin()) {
    wchar_t szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, MAX_PATH);

    // 以管理员权限重启自身
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = szPath;
    sei.hwnd = NULL;
    sei.nShow = SW_NORMAL;

    if (ShellExecuteExW(&sei)) {
        // 启动成功，退出当前进程
        exit(0);
    } else {
        // 启动失败，弹窗提示
        MessageBoxW(NULL, L"自动提权失败，请右键以管理员身份运行。", L"权限不足", MB_OK | MB_ICONERROR);
        exit(1);
    }
}

        // 初始化COM为多线程模式
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr)) {
            if (hr == RPC_E_CHANGED_MODE) {
                Logger::Warn("COM初始化模式冲突: 线程已初始化为不同的模式");
            }
            else {
                Logger::Error("COM初始化失败: 0x" + std::to_string(hr));
            }
            return -1;
        }

        struct ComCleanup {
            ~ComCleanup() { CoUninitialize(); }
        } comCleanup;

        // Use SharedMemoryManager instead of local functions
        if (!SharedMemoryManager::InitSharedMemory()) {
            CoUninitialize();
            return 1;
        }

        // 创建WMI管理器并初始化
        WmiManager wmiManager;
        if (!wmiManager.IsInitialized()) {
            Logger::Error("WMI初始化失败");
            MessageBoxA(NULL, "WMI初始化失败，无法获取系统信息。", "错误", MB_OK | MB_ICONERROR);
            return 1;
        }

        LibreHardwareMonitorBridge::Initialize();

        Logger::Info("程序启动");

        // 初始化循环计数器，减少频繁的日志记录
        int loopCounter = 0;
        
        while (true) {
            auto loopStart = std::chrono::high_resolution_clock::now();
            
            // 只在每10次循环记录一次详细信息
            bool isDetailedLogging = (loopCounter % 10 == 0);
            
            if (isDetailedLogging) {
                Logger::Debug("开始主循环第 #" + std::to_string(loopCounter) + " 次迭代");
            }
            
            // 获取系统信息
            SystemInfo sysInfo;

            // 操作系统信息
            OSInfo os;
            sysInfo.osVersion = os.GetVersion();

            // CPU信息 - 改进CPU使用率获取
            CpuInfo cpu;
            sysInfo.cpuName = cpu.GetName();
            sysInfo.physicalCores = cpu.GetLargeCores() + cpu.GetSmallCores();
            sysInfo.logicalCores = cpu.GetTotalCores();
            sysInfo.performanceCores = cpu.GetLargeCores();
            sysInfo.efficiencyCores = cpu.GetSmallCores();
            
            // 确保CPU使用率为double类型
            sysInfo.cpuUsage = cpu.GetUsage();
            
            // 只在详细日志模式下记录CPU使用率
            if (isDetailedLogging) {
                Logger::Debug("主程序CPU使用率: " + std::to_string(sysInfo.cpuUsage) + "%");
            }
            
            sysInfo.hyperThreading = cpu.IsHyperThreadingEnabled();
            sysInfo.virtualization = cpu.IsVirtualizationEnabled();
            sysInfo.performanceCoreFreq = cpu.GetLargeCoreSpeed();
            sysInfo.efficiencyCoreFreq = cpu.GetSmallCoreSpeed() * 0.8;

            // 内存信息
            MemoryInfo mem;
            sysInfo.totalMemory = mem.GetTotalPhysical();
            sysInfo.usedMemory = mem.GetTotalPhysical() - mem.GetAvailablePhysical();
            sysInfo.availableMemory = mem.GetAvailablePhysical();

            // GPU信息 - 改进虚拟显卡处理（使用缓存机制避免重复检测）
            static bool gpuInfoInitialized = false;
            static std::string cachedGpuName = "未检测到GPU";
            static std::string cachedGpuBrand = "未知";
            static uint64_t cachedGpuMemory = 0;
            static uint32_t cachedGpuCoreFreq = 0;
            static bool cachedGpuIsVirtual = false;
            
            if (!gpuInfoInitialized) {
                // 第一次循环时进行GPU检测并记录信息
                Logger::Info("正在初始化GPU信息（仅执行一次）...");
                
                GpuInfo gpuInfo(wmiManager);
                const auto& gpus = gpuInfo.GetGpuData();
                
                // 记录所有检测到的GPU
                for (const auto& gpu : gpus) {
                    std::string gpuName = WinUtils::WstringToString(gpu.name);
                    Logger::Info("检测到GPU: " + gpuName + 
                               " (虚拟: " + (gpu.isVirtual ? "是" : "否") + 
                               ", NVIDIA: " + (gpuName.find("NVIDIA") != std::string::npos ? "是" : "否") + 
                               ", 集成: " + (gpuName.find("Intel") != std::string::npos || 
                                           gpuName.find("AMD") != std::string::npos ? "是" : "否") + ")");
                }
                
                // 优先选择非虚拟GPU
                const GpuInfo::GpuData* selectedGpu = nullptr;
                for (const auto& gpu : gpus) {
                    if (!gpu.isVirtual) {
                        selectedGpu = &gpu;
                        break;
                    }
                }
                
                // 如果没有非虚拟GPU，选择第一个GPU
                if (!selectedGpu && !gpus.empty()) {
                    selectedGpu = &gpus[0];
                }
                
                if (selectedGpu) {
                    cachedGpuName = WinUtils::WstringToString(selectedGpu->name);
                    cachedGpuBrand = GetGpuBrand(selectedGpu->name);
                    cachedGpuMemory = selectedGpu->dedicatedMemory;
                    cachedGpuCoreFreq = selectedGpu->coreClock;
                    cachedGpuIsVirtual = selectedGpu->isVirtual;
                    
                    Logger::Info("选择主GPU: " + cachedGpuName + 
                               " (虚拟: " + (cachedGpuIsVirtual ? "是" : "否") + ")");
                } else {
                    Logger::Warn("未检测到任何GPU");
                }
                
                gpuInfoInitialized = true;
                Logger::Info("GPU信息初始化完成，后续循环将使用缓存信息");
            }
            
            // 使用缓存的GPU信息
            sysInfo.gpuName = cachedGpuName;
            sysInfo.gpuBrand = cachedGpuBrand;
            sysInfo.gpuMemory = cachedGpuMemory;
            sysInfo.gpuCoreFreq = cachedGpuCoreFreq;
            sysInfo.gpuIsVirtual = cachedGpuIsVirtual;

            // 添加温度数据采集（减少频率）
            if (loopCounter % 5 == 0) { // 每5秒采集一次温度数据
                try {
                    auto temperatures = LibreHardwareMonitorBridge::GetTemperatures();
                    sysInfo.temperatures.clear();
                    for (const auto& temp : temperatures) {
                        sysInfo.temperatures.push_back({temp.first, temp.second});
                    }
                    if (isDetailedLogging) {
                        Logger::Info("收集到 " + std::to_string(temperatures.size()) + " 个温度读数");
                    }
                }
                catch (const std::exception& e) {
                    Logger::Error("获取温度数据失败: " + std::string(e.what()));
                }
            }

            // 添加磁盘信息采集（减少频率）
            if (loopCounter % 10 == 0) { // 每10秒采集一次磁盘数据
                try {
                    DiskInfo diskInfo;
                    sysInfo.disks = diskInfo.GetDisks();
                    if (isDetailedLogging) {
                        Logger::Info("收集到 " + std::to_string(sysInfo.disks.size()) + " 个磁盘条目");
                    }

                    // Validate disk data
                    if (sysInfo.disks.size() > 8) {
                        Logger::Error("Disk count exceeds maximum allowed (8). Skipping disk data update.");
                        continue;
                    }

                    if (isDetailedLogging) {
                        for (size_t i = 0; i < sysInfo.disks.size(); ++i) {
                            const auto& disk = sysInfo.disks[i];

                            // Ensure proper type handling for disk.label and disk.fileSystem
                            std::wstring labelW = WinUtils::StringToWstring(disk.label);
                            std::wstring fsW = WinUtils::StringToWstring(disk.fileSystem);

                            if (labelW.length() >= sizeof(disk.label) / sizeof(wchar_t) ||
                                fsW.length() >= sizeof(disk.fileSystem) / sizeof(wchar_t)) {
                                Logger::Error("Invalid disk data detected at index " + std::to_string(i));
                                continue;
                            }

                            Logger::Info("Disk " + std::to_string(i) + ": Label=" + disk.label +
                                         ", FileSystem=" + disk.fileSystem);
                        }
                    }
                }
                catch (const std::exception& e) {
                    Logger::Error("获取磁盘数据失败: " + std::string(e.what()));
                }
            }

            // 写入共享内存前验证数据
            if (sysInfo.cpuUsage < 0.0 || sysInfo.cpuUsage > 100.0) {
                Logger::Warn("CPU使用率数据异常: " + std::to_string(sysInfo.cpuUsage) + "%, 重置为0");
                sysInfo.cpuUsage = 0.0;
            }

            // 写入共享内存
            try {
                if (SharedMemoryManager::GetBuffer()) {
                    SharedMemoryManager::WriteToSharedMemory(sysInfo);
                    if (isDetailedLogging) {
                        Logger::Debug("成功更新共享内存");
                    }
                } else {
                    Logger::Critical("共享内存缓冲区不可用");
                    // Try to reinitialize
                    if (SharedMemoryManager::InitSharedMemory()) {
                        SharedMemoryManager::WriteToSharedMemory(sysInfo);
                        if (isDetailedLogging) {
                            Logger::Info("重新初始化并更新共享内存");
                        }
                    } else {
                        Logger::Error("重新初始化共享内存失败: " + SharedMemoryManager::GetLastError());
                    }
                }
            }
            catch (const std::exception& e) {
                Logger::Error("写入共享内存时发生异常: " + std::string(e.what()));
            }

            // 计算循环执行时间并自适应休眠
            auto loopEnd = std::chrono::high_resolution_clock::now();
            auto loopDuration = std::chrono::duration_cast<std::chrono::milliseconds>(loopEnd - loopStart);
            
            // 确保总循环时间至少为1秒
            int sleepTime = std::max(1000 - static_cast<int>(loopDuration.count()), 100); // 最少休眠100ms
            
            if (isDetailedLogging) {
                Logger::Debug("第 #" + std::to_string(loopCounter) + " 次循环耗时 " + 
                            std::to_string(loopDuration.count()) + "毫秒，休眠 " + 
                            std::to_string(sleepTime) + "毫秒");
            }
            
            // 休眠
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
            
            loopCounter++;
        }
            
        // 清理资源
        LibreHardwareMonitorBridge::Cleanup();
        SharedMemoryManager::CleanupSharedMemory(); // Use SharedMemoryManager for cleanup
        CoUninitialize();

        Logger::Info("程序正常退出");
        return 0;
    }
    catch (const std::exception& e) {
        Logger::Error("程序发生致命错误: " + std::string(e.what()));
        return 1;
    }
}
