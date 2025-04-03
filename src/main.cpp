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
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <windows.h>
#include <utility>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")

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

//主要函数
int main() {
    try {
        Logger::Initialize("system_monitor.log");
        Logger::Info("系统监控程序启动");

        // 初始化WMI管理器
        WmiManager wmiManager;
        if (!wmiManager.IsInitialized()) {
            Logger::Error("WMI初始化失败");
            return 1;
        }

        // 操作系统信息
        PrintSectionHeader("操作系统");
        OSInfo os;
        PrintInfoItem("操作系统版本", os.GetVersion());

        // CPU信息
        PrintSectionHeader("处理器信息");
        CpuInfo cpu;
        PrintInfoItem("CPU名称", cpu.GetName());
        PrintInfoItem("物理核心数", std::to_string(cpu.GetLargeCores() + cpu.GetSmallCores()));
        PrintInfoItem("逻辑线程数", std::to_string(cpu.GetTotalCores()));

        // P-Core (性能核心)信息
        PrintInfoItem("性能核心", std::to_string(cpu.GetLargeCores()) + " 核" +
            (cpu.IsHyperThreadingEnabled() ? " (" + std::to_string(cpu.GetLargeCores() * 2) + " 线程)" : ""));
        PrintInfoItem("性能核心频率", FormatFrequency(cpu.GetLargeCoreSpeed()));

        // E-Core (能效核心)信息
        PrintInfoItem("能效核心", std::to_string(cpu.GetSmallCores()) + " 核");
        PrintInfoItem("能效核心频率", FormatFrequency(cpu.GetSmallCoreSpeed() * 0.8));  // E核心频率通常较低

        PrintInfoItem("CPU使用率", FormatPercentage(cpu.GetUsage()));
        PrintInfoItem("超线程状态", cpu.IsHyperThreadingEnabled() ? "已启用" : "未启用");
        PrintInfoItem("虚拟化支持", cpu.IsVirtualizationEnabled() ? "已启用" : "未启用");

        // 内存信息
        PrintSectionHeader("内存信息");
        MemoryInfo mem;
        PrintInfoItem("物理内存总量", FormatSize(mem.GetTotalPhysical()));
        PrintInfoItem("已用物理内存", FormatSize(mem.GetTotalPhysical() - mem.GetAvailablePhysical()));
        PrintInfoItem("可用物理内存", FormatSize(mem.GetAvailablePhysical()));

        // 虚拟内存显示部分
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        if (GlobalMemoryStatusEx(&memStatus)) {
            PrintInfoItem("页面文件总量", FormatSize(memStatus.ullTotalPageFile));
            PrintInfoItem("可用页面文件", FormatSize(memStatus.ullAvailPageFile));
        }

        // GPU信息
        PrintSectionHeader("显卡信息");
        GpuInfo gpuInfo(wmiManager);
        const auto& gpus = gpuInfo.GetGpuData();
        if (gpus.empty()) {
            Logger::Warning("未检测到任何显卡");
        } else {
            int gpuIndex = 1;
            for (const auto& gpu : gpus) {
                std::cout << "\n  显卡 #" << gpuIndex << std::endl;
                PrintInfoItem("名称", WinUtils::WstringToString(gpu.name));
                PrintInfoItem("品牌", GetGpuBrand(gpu.name));
                PrintInfoItem("显存", FormatSize(gpu.dedicatedMemory));
                PrintInfoItem("核心频率", FormatFrequency(gpu.coreClock));
                if (gpu.isNvidia) {
                    PrintInfoItem("计算能力",
                        std::to_string(gpu.computeCapabilityMajor) + "." +
                        std::to_string(gpu.computeCapabilityMinor));
                }
                gpuIndex++;
            }
        }

        // 网络适配器
        PrintSectionHeader("网络适配器");
        NetworkAdapter network(wmiManager);
        const auto& adapters = network.GetAdapters();
        if (adapters.empty()) {
            Logger::Warning("未检测到活动网络适配器");
        }
        else {
            for (size_t i = 0; i < adapters.size(); ++i) {
                const auto& adapter = adapters[i];
                std::cout << "\n  适配器 #" << i + 1 << std::endl;
                PrintInfoItem("适配器名称", WinUtils::WstringToString(adapter.name));
                PrintInfoItem("MAC地址", WinUtils::WstringToString(adapter.mac));
                PrintInfoItem("连接状态", adapter.isConnected ? "已连接" : "未连接");

                if (adapter.isConnected) {
                    PrintInfoItem("IP地址", WinUtils::WstringToString(adapter.ip));
                    PrintInfoItem("连接速度", WinUtils::WstringToString(adapter.speedString));
                }
            }
        }

        // 时间信息
        PrintSectionHeader("时间");
        auto now = std::chrono::system_clock::now();
        auto custom_now = std::chrono::time_point_cast<std::chrono::duration<int64_t, std::ratio<1, 10000000>>>(now);
        PrintInfoItem("当前本地时间", TimeUtils::GetCurrentLocalTime());
        PrintInfoItem("当前本机UTC时间", TimeUtils::FormatTimePoint(custom_now));
        PrintInfoItem("系统启动时间", TimeUtils::GetBootTimeUtc());
        PrintInfoItem("系统运行时间", TimeUtils::GetUptime());

        // 温度信息
        PrintSectionHeader("硬件温度信息");
        LibreHardwareMonitorBridge::Initialize();
        auto temps = LibreHardwareMonitorBridge::GetTemperatures();

        bool cpuTempFound = false;
        bool gpuTempFound = false;

        // gpus 已在前面定义，直接使用
        std::string gpuName;
        if (!gpus.empty()) {
            gpuName = WinUtils::WstringToString(gpus[0].name);
        }

        for (const auto& temp : temps) {
            if (temp.first == "CPU Package") {
                PrintInfoItem("CPU温度", FormatTemperature(temp.second));
                cpuTempFound = true;
            }
            else if (temp.first.find("GPU Core") != std::string::npos) {
                if (!gpuName.empty()) {
                    PrintInfoItem("GPU温度 (" + gpuName + ")",
                        FormatTemperature(temp.second));
                }
                else {
                    PrintInfoItem("GPU温度", FormatTemperature(temp.second));
                }
                gpuTempFound = true;
            }
        }

        // 如果没有找到温度，显示提示信息
        if (!cpuTempFound) {
            PrintInfoItem("CPU温度", "无法获取");
        }
        if (!gpuTempFound) {
            PrintInfoItem("GPU温度", "无法获取");
        }

        LibreHardwareMonitorBridge::Cleanup();

        // 硬盘信息显示部分
        PrintSectionHeader("磁盘信息");
        DiskInfo diskInfo;
        const auto& drives = diskInfo.GetDrives();

        for (const auto& drive : drives) {
            std::cout << "\n  " << drive.letter << ": 驱动器" << std::endl;

            // 显示卷标（如果有）
            if (!drive.label.empty()) {
                PrintInfoItem("卷标", WinUtils::WstringToString(drive.label));
            }

            // 显示文件系统
            PrintInfoItem("文件系统", WinUtils::WstringToString(drive.fileSystem));

            // 显示容量信息
            PrintInfoItem("总容量", FormatSize(drive.totalSize));
            PrintInfoItem("已用空间", FormatSize(drive.usedSpace));
            PrintInfoItem("可用空间", FormatSize(drive.freeSpace));

            // 计算并显示使用率
            double usagePercent = (static_cast<double>(drive.usedSpace) / drive.totalSize) * 100.0;
            PrintInfoItem("使用率", FormatPercentage(usagePercent));
        }

        Logger::Info("系统信息收集完成");
    } catch (const std::exception& e) {
        Logger::Error("程序发生致命错误: " + std::string(e.what()));
        return 1;
    }
    return 0;
}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             