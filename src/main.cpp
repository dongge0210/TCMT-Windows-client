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
#include <QMessageBox> 
#include <io.h>
#include <fcntl.h>

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
int main(int argc, char* argv[]) {
    try {
        // Set console output to UTF-8
        _setmode(_fileno(stdout), _O_U8TEXT);

        Logger::EnableConsoleOutput(true); // Enable console output for Logger
        Logger::Initialize("system_monitor.log");
        Logger::Info("程序启动");

        // 初始化COM为多线程模式
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr)) {
            if (hr == RPC_E_CHANGED_MODE) {
                Logger::Error("COM初始化模式冲突: 线程已初始化为不同的模式");
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
            QMessageBox::critical(nullptr, "错误", "WMI初始化失败，无法获取系统信息。");
            return 1;
        }

        LibreHardwareMonitorBridge::Initialize();

        while (true) {

            // 获取系统信息
            SystemInfo sysInfo;

            // 操作系统信息
            OSInfo os;
            sysInfo.osVersion = os.GetVersion();

            // CPU信息
            CpuInfo cpu;
            sysInfo.cpuName = cpu.GetName();
            sysInfo.physicalCores = cpu.GetLargeCores() + cpu.GetSmallCores();
            sysInfo.logicalCores = cpu.GetTotalCores();
            sysInfo.performanceCores = cpu.GetLargeCores();
            sysInfo.efficiencyCores = cpu.GetSmallCores();
            sysInfo.cpuUsage = cpu.GetUsage();
            sysInfo.hyperThreading = cpu.IsHyperThreadingEnabled();
            sysInfo.virtualization = cpu.IsVirtualizationEnabled();
            sysInfo.performanceCoreFreq = cpu.GetLargeCoreSpeed();
            sysInfo.efficiencyCoreFreq = cpu.GetSmallCoreSpeed() * 0.8;

            // 内存信息
            MemoryInfo mem;
            sysInfo.totalMemory = mem.GetTotalPhysical();
            sysInfo.usedMemory = mem.GetTotalPhysical() - mem.GetAvailablePhysical();
            sysInfo.availableMemory = mem.GetAvailablePhysical();

            // GPU信息
            GpuInfo gpuInfo(wmiManager);
            const auto& gpus = gpuInfo.GetGpuData();
            if (!gpus.empty()) {
                const auto& gpu = gpus[0];
                sysInfo.gpuName = WinUtils::WstringToString(gpu.name);
                sysInfo.gpuBrand = GetGpuBrand(gpu.name);
                sysInfo.gpuMemory = gpu.dedicatedMemory;
                sysInfo.gpuCoreFreq = gpu.coreClock;
            }

            // 网络适配器信息
            NetworkAdapter network(wmiManager);
            const auto& adapters = network.GetAdapters();
            if (!adapters.empty()) {
                const auto& adapter = adapters[0];
                sysInfo.networkAdapterName = WinUtils::WstringToString(adapter.name);
                sysInfo.networkAdapterMac = WinUtils::WstringToString(adapter.mac);
                sysInfo.networkAdapterSpeed = adapter.speed;
            }

            // 写入共享内存，使用 SharedMemoryManager 代替
            if (!SharedMemoryManager::GetBuffer() && !SharedMemoryManager::InitSharedMemory()) {
                // Handle shared memory initialization failure
                Logger::Error("Failed to initialize shared memory: " + SharedMemoryManager::GetLastError());
                // Continue with program execution even if shared memory fails
                // This prevents the program from crashing but logs the error
            } else {
                SharedMemoryManager::WriteToSharedMemory(sysInfo);
            }

            // 等待1秒
            std::this_thread::sleep_for(std::chrono::seconds(1));
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
