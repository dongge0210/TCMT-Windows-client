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
#include "core/utils/Logger.h"

// ======================== Signal Handling ========================
static std::atomic<bool> g_shouldExit{false};

static void SignalHandler(int sig) {
    (void)sig;
    g_shouldExit = true;
}

// ======================== Formatting Helpers ========================
static std::string FormatDateTime(const std::chrono::system_clock::time_point& tp) {
    try {
        auto time = std::chrono::system_clock::to_time_t(tp);
        std::tm timeinfo;
        localtime_r(&time, &timeinfo);
        std::stringstream ss;
        ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    } catch (...) {
        return "time error";
    }
}

static std::string FormatFrequency(double value) {
    if (value <= 0) return "N/A";
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1);
    if (value >= 1000) ss << (value / 1000.0) << " GHz";
    else ss << value << " MHz";
    return ss.str();
}

static std::string FormatPercentage(double value) {
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << value << "%";
    return ss.str();
}

static std::string FormatSize(uint64_t bytes) {
    const double gb = 1024.0 * 1024.0 * 1024.0;
    const double mb = 1024.0 * 1024.0;
    const double kb = 1024.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1);
    if (bytes >= (uint64_t)gb) ss << (bytes / gb) << " GB";
    else if (bytes >= (uint64_t)mb) ss << (bytes / mb) << " MB";
    else if (bytes >= (uint64_t)kb) ss << (bytes / kb) << " KB";
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
        Logger::EnableConsoleOutput(true);
        Logger::SetLogLevel(LOG_DEBUG);
        Logger::Info("TCMT macOS Client starting...");
    } catch (const std::exception& e) {
        std::cerr << "Logger init failed: " << e.what() << std::endl;
        return 1;
    }

    // Initialize shared memory
    if (!SharedMemoryManager::InitSharedMemory()) {
        Logger::Error("SharedMemory init failed: " + SharedMemoryManager::GetLastError());
        return 1;
    }
    Logger::Info("SharedMemory initialized");

    // Initialize temperature wrapper (macOS: limited support)
    TemperatureWrapper::Initialize();
    Logger::Info("TemperatureWrapper initialized");

    // Cache static system info
    OSInfo os;
    Logger::Info("OS: " + os.GetVersion());

    std::unique_ptr<CpuInfo> cpuInfo;
    try {
        cpuInfo = std::make_unique<CpuInfo>();
        Logger::Info("CPU: " + cpuInfo->GetName()
                   + " (" + std::to_string(cpuInfo->GetTotalCores()) + " cores)");
    } catch (const std::exception& e) {
        Logger::Error("CpuInfo init failed: " + std::string(e.what()));
    }

    // Thread-safe GPU cache
    std::unique_ptr<GpuInfo> gpuInfo;
    try {
        gpuInfo = std::make_unique<GpuInfo>();
        for (const auto& gpu : gpuInfo->GetGpuData()) {
            std::string gname(gpu.name.begin(), gpu.name.end());
            Logger::Info("GPU: " + gname
                       + " mem=" + FormatSize(gpu.dedicatedMemory));
        }
    } catch (const std::exception& e) {
        Logger::Error("GpuInfo init failed: " + std::string(e.what()));
    }

    Logger::Info("System init complete, entering monitor loop...");

    int loopCounter = 1;
    bool isFirstRun = true;

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

    while (!g_shouldExit.load()) {
        try {
            auto loopStart = std::chrono::high_resolution_clock::now();
            bool isDetailed = (loopCounter % 5 == 1);

            if (isDetailed)
                Logger::Debug("Loop #" + std::to_string(loopCounter));

            // === Build SystemInfo ===
            SystemInfo sysInfo{};
            sysInfo.cpuUsage = 0.0;
            sysInfo.performanceCoreFreq = 0.0;
            sysInfo.efficiencyCoreFreq = 0.0;
            sysInfo.totalMemory = 0;
            sysInfo.usedMemory = 0;
            sysInfo.availableMemory = 0;
            sysInfo.gpuMemory = 0;
            sysInfo.gpuCoreFreq = 0.0;
            sysInfo.gpuIsVirtual = false;
            sysInfo.networkAdapterSpeed = 0;
            sysInfo.osVersion = os.GetVersion();
            sysInfo.cpuName = cachedCpuName;
            sysInfo.physicalCores = cachedTotalCores;
            sysInfo.logicalCores = cachedTotalCores;
            sysInfo.performanceCores = cachedPCores;
            sysInfo.efficiencyCores = cachedECores;
            sysInfo.hyperThreading = cachedHT;
            sysInfo.virtualization = cachedVirt;

            // CPU dynamic info
            if (cpuInfo) {
                sysInfo.cpuUsage = cpuInfo->GetUsage();
                sysInfo.performanceCoreFreq = cpuInfo->GetLargeCoreSpeed();
                sysInfo.efficiencyCoreFreq = cpuInfo->GetSmallCoreSpeed();
                sysInfo.cpuUsageSampleIntervalMs = cpuInfo->GetLastSampleIntervalMs();
            }

            // Memory
            try {
                MemoryInfo mem;
                sysInfo.totalMemory = mem.GetTotalPhysical();
                sysInfo.usedMemory = mem.GetTotalPhysical() - mem.GetAvailablePhysical();
                sysInfo.availableMemory = mem.GetAvailablePhysical();
            } catch (const std::exception& e) {
                Logger::Error("Memory error: " + std::string(e.what()));
            }

            // GPU
            if (gpuInfo) {
                const auto& gpus = gpuInfo->GetGpuData();
                for (const auto& gpu : gpus) {
                    GPUData gd;
                    memset(&gd, 0, sizeof(GPUData));
                    std::wstring gname = gpu.name;
                    if (gname.size() >= sizeof(gd.name)/sizeof(wchar_t))
                        gname = gname.substr(0, sizeof(gd.name)/sizeof(wchar_t)-1);
                    wcsncpy(gd.name, gname.c_str(), sizeof(gd.name)/sizeof(wchar_t)-1);
                    gd.memory = gpu.dedicatedMemory;
                    gd.coreClock = gpu.coreClock;
                    gd.isVirtual = gpu.isVirtual;
                    sysInfo.gpus.push_back(gd);
                    sysInfo.gpuMemory = gpu.dedicatedMemory;
                    sysInfo.gpuCoreFreq = gpu.coreClock;
                    sysInfo.gpuIsVirtual = gpu.isVirtual;
                    if (isFirstRun) {
                        std::string gn(gpu.name.begin(), gpu.name.end());
                        Logger::Debug("GPU: " + gn + " mem=" + FormatSize(gpu.dedicatedMemory));
                    }
                }
            }

            // Network
            try {
                NetworkAdapter net;
                const auto& adapters = net.GetAdapters();
                sysInfo.adapters.clear();
                for (const auto& adapter : adapters) {
                    NetworkAdapterData ad;
                    memset(&ad, 0, sizeof(NetworkAdapterData));
                    std::string name = adapter.name;
                    if (name.size() >= sizeof(ad.name)/sizeof(wchar_t))
                        name = name.substr(0, sizeof(ad.name)/sizeof(wchar_t)-1);
                    mbstowcs(ad.name, name.c_str(), sizeof(ad.name)/sizeof(wchar_t)-1);
                    std::string mac = adapter.mac;
                    if (mac.size() >= sizeof(ad.mac)/sizeof(wchar_t))
                        mac = mac.substr(0, sizeof(ad.mac)/sizeof(wchar_t)-1);
                    mbstowcs(ad.mac, mac.c_str(), sizeof(ad.mac)/sizeof(wchar_t)-1);
                    std::string ip = adapter.ip;
                    if (ip.size() >= sizeof(ad.ipAddress)/sizeof(wchar_t))
                        ip = ip.substr(0, sizeof(ad.ipAddress)/sizeof(wchar_t)-1);
                    mbstowcs(ad.ipAddress, ip.c_str(), sizeof(ad.ipAddress)/sizeof(wchar_t)-1);
                    std::string type = adapter.adapterType;
                    if (type.size() >= sizeof(ad.adapterType)/sizeof(wchar_t))
                        type = type.substr(0, sizeof(ad.adapterType)/sizeof(wchar_t)-1);
                    mbstowcs(ad.adapterType, type.c_str(), sizeof(ad.adapterType)/sizeof(wchar_t)-1);
                    ad.speed = adapter.speed;
                    sysInfo.adapters.push_back(ad);
                    if (isFirstRun) {
                        Logger::Debug("Network: " + adapter.name
                                    + " ip=" + adapter.ip
                                    + " type=" + adapter.adapterType);
                    }
                }
            } catch (const std::exception& e) {
                Logger::Error("Network error: " + std::string(e.what()));
            }

            // Disk
            try {
                DiskInfo disk;
                sysInfo.disks = disk.GetDisks();
                if (isFirstRun) {
                    Logger::Debug("Disks: " + std::to_string(sysInfo.disks.size()) + " volumes");
                }
            } catch (const std::exception& e) {
                Logger::Error("Disk error: " + std::string(e.what()));
            }

            // Temperature
            try {
                auto temps = TemperatureWrapper::GetTemperatures();
                sysInfo.temperatures = temps;

                // Extract CPU / GPU temperatures from the named vector
                for (const auto& [name, temp] : temps) {
                    // CPU sensors: TC0x / TG0x / Ts0x / Rp0T / etc.
                    // GPU sensors: TGxP / TGxD / TGDD / etc.
                    bool isGpu = (name.find("TG") != std::string::npos ||
                                  name.find("GPU") != std::string::npos ||
                                  name.find("Gg") != std::string::npos);
                    if (isGpu && sysInfo.gpuTemperature == 0)
                        sysInfo.gpuTemperature = temp;
                    else if (!isGpu && sysInfo.cpuTemperature == 0)
                        sysInfo.cpuTemperature = temp;
                }

                if (isFirstRun)
                    Logger::Debug("Temperatures: " + std::to_string(temps.size()) + " sensors");
            } catch (const std::exception& e) {
                Logger::Error("Temperature error: " + std::string(e.what()));
            }

            // Write to shared memory
            try {
                if (SharedMemoryManager::GetBuffer()) {
                    SharedMemoryManager::WriteToSharedMemory(sysInfo);
                    if (isDetailed)
                        Logger::Debug("SharedMemory updated");
                }
            } catch (const std::exception& e) {
                Logger::Error("SharedMemory write error: " + std::string(e.what()));
            }

            if (isFirstRun) isFirstRun = false;

            // Sleep with interrupt check
            auto loopEnd = std::chrono::high_resolution_clock::now();
            int loopMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
                loopEnd - loopStart).count();
            int sleepMs = std::max(1000 - loopMs, 50);
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
    TemperatureWrapper::Cleanup();
    SharedMemoryManager::CleanupSharedMemory();
    Logger::Info("Done.");
    return 0;
}
