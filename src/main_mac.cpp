// main_mac.cpp - macOS entry point for TCMT
// Conditionally compiled via CMake (not for Windows)

#ifndef TCMT_MACOS
#error "This file should only be compiled for macOS (TCMT_MACOS defined)"
#endif

#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <sys/types.h>
#include <mach/mach_time.h>

#include "core/cpu/CpuInfo.h"
#include "core/gpu/GpuInfo.h"
#include "core/memory/MemoryInfo.h"
#include "core/network/NetworkAdapter.h"
#include "core/os/OSInfo.h"
#include "core/disk/DiskInfo.h"
#include "core/DataStruct/DataStruct.h"
#include "core/DataStruct/SharedMemoryManager.h"
#include "core/temperature/TemperatureWrapper.h"
#include "core/Utils/Logger.h"
#include "tui/TuiApp.h"

// ======================== Signal Handling ========================
static std::atomic<bool> g_shouldExit{false};

static void SignalHandler(int sig) {
    (void)sig;
    g_shouldExit = true;
}

// ======================== Formatting Helpers ========================
static std::string FormatSize(uint64_t bytes) {
    const double gb = 1024.0 * 1024.0 * 1024.0;
    const double mb = 1024.0 * 1024.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1);
    if (bytes >= (uint64_t)gb) ss << (bytes / gb) << " GB";
    else if (bytes >= (uint64_t)mb) ss << (bytes / mb) << " MB";
    else ss << bytes << " B";
    return ss.str();
}

// ======================== Main ========================
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    // Setup signal handlers
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
    std::signal(SIGHUP, SignalHandler);

    try {
        Logger::Initialize("system_monitor.log");
        Logger::EnableConsoleOutput(false);  // TUI takes over console
        Logger::SetLogLevel(LOG_INFO);
        Logger::Info("TCMT macOS Client starting (TUI mode)...");
    } catch (const std::exception& e) {
        std::cerr << "Logger init failed: " << e.what() << std::endl;
        return 1;
    }

    // Initialize temperature wrapper (macOS: limited support)
    TemperatureWrapper::Initialize();
    Logger::Debug("TemperatureWrapper initialized");

    // Cache static system info
    OSInfo os;
    Logger::Debug("OS: " + os.GetVersion());

    std::unique_ptr<CpuInfo> cpuInfo;
    try {
        cpuInfo = std::make_unique<CpuInfo>();
        Logger::Debug("CPU: " + cpuInfo->GetName()
                   + " (" + std::to_string(cpuInfo->GetTotalCores()) + " cores)");
    } catch (const std::exception& e) {
        Logger::Error("CpuInfo init failed: " + std::string(e.what()));
    }

    std::unique_ptr<GpuInfo> gpuInfo;
    try {
        gpuInfo = std::make_unique<GpuInfo>();
        for (const auto& gpu : gpuInfo->GetGpuData()) {
            std::string gname(gpu.name.begin(), gpu.name.end());
            Logger::Debug("GPU: " + gname + " mem=" + FormatSize(gpu.dedicatedMemory));
        }
    } catch (const std::exception& e) {
        Logger::Error("GpuInfo init failed: " + std::string(e.what()));
    }

    // Initialize shared memory (optional, for future GUI clients)
    bool shmOk = SharedMemoryManager::InitSharedMemory();
    if (shmOk) {
        Logger::Debug("SharedMemory initialized");
    } else {
        Logger::Warn("SharedMemory init failed: " + SharedMemoryManager::GetLastError());
    }

    // Start TUI
    tcmt::TuiApp tuiApp;
    tuiApp.SetLogBuffer(&Logger::GetTuiBuffer());
    tuiApp.Start();
    Logger::Info("TUI started");

    // Static info caching
    static std::string cachedCpuName;
    static int cachedTotalCores = 0;
    static int cachedPCores = 0;
    static int cachedECores = 0;
    static bool cachedHT = false;
    static bool cachedVirt = false;
    if (cpuInfo) {
        cachedCpuName = cpuInfo->GetName();
        cachedTotalCores = cpuInfo->GetTotalCores();
        cachedPCores = cpuInfo->GetLargeCores();
        cachedECores = cpuInfo->GetSmallCores();
        cachedHT = cpuInfo->IsHyperThreadingEnabled();
        cachedVirt = cpuInfo->IsVirtualizationEnabled();
    }

    int loopCounter = 1;

    while (!g_shouldExit.load() && tuiApp.IsRunning()) {
        try {
            auto loopStart = std::chrono::high_resolution_clock::now();

            // === Build TuiData snapshot ===
            tcmt::TuiData data;
            data.cpuName = cachedCpuName;
            data.physicalCores = cachedTotalCores;
            data.performanceCores = cachedPCores;
            data.efficiencyCores = cachedECores;
            data.cpuUsage = 0.0;
            data.pCoreFreq = 0.0;
            data.eCoreFreq = 0.0;
            data.cpuTemp = 0.0;
            data.totalMemory = 0;
            data.usedMemory = 0;
            data.availableMemory = 0;
            data.gpuUsage = 0.0;
            data.gpuTemp = 0.0;

            // CPU dynamic info
            if (cpuInfo) {
                data.cpuUsage = cpuInfo->GetUsage();
                data.pCoreFreq = cpuInfo->GetLargeCoreSpeed();
                data.eCoreFreq = cpuInfo->GetSmallCoreSpeed();
            }

            // Memory
            try {
                MemoryInfo mem;
                data.totalMemory = mem.GetTotalPhysical();
                data.usedMemory = mem.GetTotalPhysical() - mem.GetAvailablePhysical();
                data.availableMemory = mem.GetAvailablePhysical();
            } catch (const std::exception& e) {
                Logger::Error("Memory error: " + std::string(e.what()));
            }

            // GPU
            if (gpuInfo) {
                const auto& gpus = gpuInfo->GetGpuData();
                for (const auto& gpu : gpus) {
                    if (data.gpuName.empty()) {
                        std::string gn(gpu.name.begin(), gpu.name.end());
                        data.gpuName = gn;
                        data.gpuMemory = gpu.dedicatedMemory;
                    }
                }
            }

            // Network
            try {
                NetworkAdapter net;
                const auto& adapters = net.GetAdapters();
                for (const auto& adapter : adapters) {
                    tcmt::TuiData::NetInfo ni;
                    ni.name = adapter.name;
                    ni.ip = adapter.ip;
                    ni.type = adapter.adapterType;
                    ni.speed = adapter.speed;
                    data.adapters.push_back(ni);
                }
            } catch (const std::exception& e) {
                Logger::Error("Network error: " + std::string(e.what()));
            }

            // Disk
            try {
                DiskInfo disk;
                auto volumes = disk.GetDisks();
                for (const auto& vol : volumes) {
                    tcmt::TuiData::DiskInfo di;
                    di.label = vol.label;
                    di.totalSize = vol.totalSize;
                    di.usedSpace = vol.usedSpace;
                    di.fileSystem = vol.fileSystem;
                    data.disks.push_back(di);
                }
            } catch (const std::exception& e) {
                Logger::Error("Disk error: " + std::string(e.what()));
            }

            // Temperature
            try {
                auto temps = TemperatureWrapper::GetTemperatures();
                data.temperatures = temps;
                for (const auto& [name, temp] : temps) {
                    bool isGpu = (name.find("TG") != std::string::npos ||
                                  name.find("GPU") != std::string::npos);
                    if (isGpu && data.gpuTemp == 0) data.gpuTemp = temp;
                    else if (!isGpu && data.cpuTemp == 0) data.cpuTemp = temp;
                }
            } catch (const std::exception& e) {
                Logger::Error("Temperature error: " + std::string(e.what()));
            }

            // Timestamp
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm;
            localtime_r(&time, &tm);
            char buf[64];
            std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
            data.timestamp = buf;

            // Update TUI
            tuiApp.UpdateData(data);

            // Also write to shared memory (optional)
            if (shmOk) {
                SystemInfo sysInfo{};
                sysInfo.cpuName = data.cpuName;
                sysInfo.cpuUsage = data.cpuUsage;
                sysInfo.performanceCoreFreq = data.pCoreFreq;
                sysInfo.efficiencyCoreFreq = data.eCoreFreq;
                sysInfo.cpuTemperature = data.cpuTemp;
                sysInfo.totalMemory = data.totalMemory;
                sysInfo.usedMemory = data.usedMemory;
                sysInfo.availableMemory = data.availableMemory;
                sysInfo.physicalCores = data.physicalCores;
                sysInfo.performanceCores = data.performanceCores;
                sysInfo.efficiencyCores = data.efficiencyCores;
                sysInfo.gpuUsage = data.gpuUsage;
                sysInfo.gpuTemperature = data.gpuTemp;
                sysInfo.gpuName = data.gpuName;
                sysInfo.gpuMemory = data.gpuMemory;
                SharedMemoryManager::WriteToSharedMemory(sysInfo);
            }

            // Sleep
            auto loopEnd = std::chrono::high_resolution_clock::now();
            int loopMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
                loopEnd - loopStart).count();
            int sleepMs = std::max(500 - loopMs, 50);  // 2 Hz update
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
            loopCounter++;
        }
        catch (const std::exception& e) {
            Logger::Error("Loop error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        catch (...) {
            Logger::Error("Loop unknown error");
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    Logger::Info("Exiting, cleaning up...");
    tuiApp.Stop();
    TemperatureWrapper::Cleanup();
    SharedMemoryManager::CleanupSharedMemory();
    Logger::Info("Done.");
    return 0;
}
