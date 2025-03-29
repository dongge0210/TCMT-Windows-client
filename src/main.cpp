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
#include <iostream>
#include <iomanip>
#include <windows.h>

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
        PrintInfoItem("总核心数", std::to_string(cpu.GetTotalCores()));
        PrintInfoItem("性能核心", std::to_string(cpu.GetLargeCores()));
        PrintInfoItem("能效核心", std::to_string(cpu.GetSmallCores()));
        PrintInfoItem("当前频率", std::to_string(cpu.GetCurrentSpeed()) + " MHz");
        PrintInfoItem("使用率", std::to_string(cpu.GetUsage()) + "%");
        PrintInfoItem("超线程状态", cpu.IsHyperThreadingEnabled() ? "已启用" : "未启用");
        try {
            PrintInfoItem("虚拟化支持", cpu.IsVirtualizationEnabled() ? "已启用" : "未启用");
        } catch (const std::exception& e) {
            Logger::Error("虚拟化检测失败: " + std::string(e.what()));
        }

        // 内存信息
        PrintSectionHeader("内存信息");
        MemoryInfo mem;
        PrintInfoItem("物理内存总量", std::to_string(mem.GetTotalPhysical() >> 20) + " MB");
        PrintInfoItem("可用物理内存", std::to_string(mem.GetAvailablePhysical() >> 20) + " MB");
        PrintInfoItem("虚拟内存总量", std::to_string(mem.GetTotalVirtual() >> 20) + " MB");

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
                std::string brand = "未知";
                if (gpu.name.find(L"NVIDIA") != std::wstring::npos) brand = "NVIDIA";
                else if (gpu.name.find(L"AMD") != std::wstring::npos) brand = "AMD";
                else if (gpu.name.find(L"Intel") != std::wstring::npos) brand = "Intel";
                PrintInfoItem("品牌", brand);
                PrintInfoItem("显存", std::to_string(gpu.dedicatedMemory / (1024 * 1024)) + " MB");
                PrintInfoItem("核心频率", std::to_string(gpu.coreClock) + " MHz");
                if (gpu.isNvidia) {
                    PrintInfoItem("计算能力", std::to_string(gpu.computeCapabilityMajor) + "." + std::to_string(gpu.computeCapabilityMinor));
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
        } else {
            for (size_t i = 0; i < adapters.size(); ++i) {
                std::cout << "\n  适配器 #" << i + 1 << std::endl;
                PrintInfoItem("适配器名称", WinUtils::WstringToString(adapters[i].name));
                PrintInfoItem("MAC地址", WinUtils::WstringToString(adapters[i].mac));
                PrintInfoItem("IP地址", WinUtils::WstringToString(adapters[i].ip));
            }
        }

        // 时间信息
        PrintSectionHeader("时间");
        auto now = std::chrono::system_clock::now();
        auto custom_now = std::chrono::time_point_cast<std::chrono::duration<int64_t, std::ratio<1, 10000000>>>(now);
        PrintInfoItem("当前本机UTC时间", TimeUtils::FormatTimePoint(custom_now));

        // 温度信息
        PrintSectionHeader("硬件温度信息");
        LibreHardwareMonitorBridge::Initialize();
        auto temps = LibreHardwareMonitorBridge::GetTemperatures();
        for (const auto& temp : temps) {
            if (temp.first.find("CPU") != std::string::npos || temp.first.find("GPU") != std::string::npos) {
                PrintInfoItem(temp.first, std::to_string(temp.second) + "°C");
            }
        }

        Logger::Info("系统信息收集完成");
    } catch (const std::exception& e) {
        Logger::Error("程序发生致命错误: " + std::string(e.what()));
        return 1;
    }
    return 0;
}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             