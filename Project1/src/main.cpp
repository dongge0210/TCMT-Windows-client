// ...existing includes...
#include "core/utils/Logger.h"
// Removed: #include <QMessageBox>

// ...existing code...

bool InitSharedMemory() {
    // ...existing code...
    if (!pBuffer) {
        CloseHandle(hMapFile);
        Logger::Error("无法映射共享内存视图");
        return false;
    }

    // Initialize critical section
    InitializeCriticalSection(&pBuffer->lock);
    return true;
}

// ...existing code...

int main(int argc, char* argv[]) {
    try {
        // Set console output to UTF-8
        _setmode(_fileno(stdout), _O_U8TEXT);

        Logger::EnableConsoleOutput(true); // Enable console output for Logger
        Logger::Initialize("system_monitor.log");
        Logger::Info("程序启动");

        // Initialize COM in single-threaded mode
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(hr)) {
            if (hr == RPC_E_CHANGED_MODE) {
                Logger::Error("COM初始化模式冲突: 线程已初始化为不同的模式");
            } else {
                Logger::Error("COM初始化失败: 0x" + std::to_string(hr));
            }
            return -1;
        }

        struct ComCleanup {
            ~ComCleanup() { CoUninitialize(); }
        } comCleanup;

        if (!InitSharedMemory()) {
            CoUninitialize();
            return 1;
        }

        // Create WMI manager and initialize
        WmiManager wmiManager;
        if (!wmiManager.IsInitialized()) {
            Logger::Error("WMI初始化失败，无法获取系统信息。");
            return 1;
        }

        LibreHardwareMonitorBridge::Initialize();

        while (true) {
            // Retrieve system information
            SystemInfo sysInfo;

            // OS information
            OSInfo os;
            sysInfo.osVersion = os.GetVersion();

            // CPU information
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

            // Memory information
            MemoryInfo mem;
            sysInfo.totalMemory = mem.GetTotalPhysical();
            sysInfo.usedMemory = mem.GetTotalPhysical() - mem.GetAvailablePhysical();
            sysInfo.availableMemory = mem.GetAvailablePhysical();

            // GPU information
            GpuInfo gpuInfo(wmiManager);
            const auto& gpus = gpuInfo.GetGpuData();
            if (!gpus.empty()) {
                const auto& gpu = gpus[0];
                sysInfo.gpuName = WinUtils::WstringToString(gpu.name);
                sysInfo.gpuBrand = GetGpuBrand(gpu.name);
                sysInfo.gpuMemory = gpu.dedicatedMemory;
                sysInfo.gpuCoreFreq = gpu.coreClock;
            }

            // Network adapter information
            NetworkAdapter network(wmiManager);
            const auto& adapters = network.GetAdapters();
            if (!adapters.empty()) {
                const auto& adapter = adapters[0];
                sysInfo.networkAdapterName = WinUtils::WstringToString(adapter.name);
                sysInfo.networkAdapterMac = WinUtils::WstringToString(adapter.mac);
                sysInfo.networkAdapterSpeed = adapter.speed;
            }

            // Write to shared memory
            WriteToSharedMemory(sysInfo);

            // Wait for 1 second
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Cleanup resources
        LibreHardwareMonitorBridge::Cleanup();
        UnmapViewOfFile(pBuffer);
        CloseHandle(hMapFile);
        CoUninitialize();

        Logger::Info("程序正常退出");
        return 0;
    } catch (const std::exception& e) {
        Logger::Error("程序发生致命错误: " + std::string(e.what()));
        return 1;
    }
}
