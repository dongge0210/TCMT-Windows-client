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
#include <mach/mach.h>
#include <mach/vm_statistics.h>

#include "core/cpu/CpuInfo.h"
#include "core/gpu/GpuInfo.h"
#include "core/memory/MemoryInfo.h"
#include "core/network/NetworkAdapter.h"
#include "core/os/OSInfo.h"
#include "core/power/PowerInfo.h"
#include "core/disk/DiskInfo.h"
#include "core/DataStruct/DataStruct.h"
#include "core/DataStruct/SharedMemoryManager.h"
#include "core/IPC/IPCServer.h"
#include "core/temperature/TemperatureWrapper.h"
#include "core/Utils/Logger.h"
#include "tui/TuiApp.h"

// Config management (wraps CPP-parsers / nlohmann/json internally)
#include "core/Config/ConfigManager.h"
#include <fstream>
#include <cstdio>

// ======================== Signal Handling ========================
static std::atomic<bool> g_shouldExit{false};

static void SignalHandler(int sig) {
    (void)sig;
    g_shouldExit = true;
    // Force fast exit on second Ctrl+C
    static std::atomic<int> sigCount{0};
    if (++sigCount >= 2) {
        _exit(1);
    }
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

    // Load application config (config.json)
    {
        ConfigManager cfg("system_monitor.json");
        if (cfg.Load()) {
            Logger::Info("Config loaded: " + cfg.GetPath());
            // Apply config-driven settings
            std::string logLevel = cfg.GetString("logging.level", "info");
            if (logLevel == "debug")
                Logger::SetLogLevel(LOG_DEBUG);
            else if (logLevel == "warning")
                Logger::SetLogLevel(LOG_WARNING);

            int refreshRate = cfg.GetInt("display.refreshRate", 500);
            // (used later for sleep interval)
            (void)refreshRate;
        } else {
            Logger::Warn("No config file found, using defaults");
        }
    }

    // ======================== --json Mode ========================
    bool jsonMode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--json") {
            jsonMode = true;
            break;
        }
    }

    if (jsonMode) {
        // One-shot JSON output for scripting
        TemperatureWrapper::Initialize();

        // Build JSON via ConfigManager, then dump to stdout via temp file
        const std::string tmpPath = "/tmp/tcmt_export.json";
        std::remove(tmpPath.c_str());  // ensure clean start
        ConfigManager cfg(tmpPath);
        cfg.Load();  // starts with empty json::object

        // OS
        try {
            OSInfo os;
            cfg.SetString("os.version", os.GetVersion());
        } catch (...) {}

        // CPU
        try {
            auto cpu = std::make_unique<CpuInfo>();
            cfg.SetString("cpu.name", cpu->GetName());
            cfg.SetInt("cpu.cores.physical", cpu->GetLargeCores() + cpu->GetSmallCores());
            cfg.SetInt("cpu.cores.logical", cpu->GetTotalCores());
            cfg.SetDouble("cpu.usage", cpu->GetUsage());
        } catch (...) {}

        // Memory
        try {
            MemoryInfo mem;
            cfg.SetUint64("memory.total", mem.GetTotalPhysical());
            cfg.SetUint64("memory.available", mem.GetAvailablePhysical());
            cfg.SetUint64("memory.used", mem.GetTotalPhysical() - mem.GetAvailablePhysical());
        } catch (...) {}

        // GPU
        try {
            auto gpu = std::make_unique<GpuInfo>();
            const auto& gpus = gpu->GetGpuData();
            if (!gpus.empty()) {
                cfg.SetString("gpu.name",
                    std::string(gpus[0].name.begin(), gpus[0].name.end()));
                cfg.SetUint64("gpu.dedicatedMemory", gpus[0].dedicatedMemory);
                cfg.SetDouble("gpu.usage", gpus[0].usage);
            }
        } catch (...) {}

        // Network
        try {
            NetworkAdapter net;
            const auto& adapters = net.GetAdapters();
            for (const auto& a : adapters) {
                nlohmann::json na;
                na["name"] = a.name;
                na["ip"] = a.ip;
                na["mac"] = a.mac;
                na["type"] = a.adapterType;
                na["speed"] = a.speed;
                na["downloadSpeed"] = a.downloadSpeed;
                na["uploadSpeed"] = a.uploadSpeed;
                cfg.AppendToArray("network.adapters", std::move(na));
            }
        } catch (...) {}

        // Disks
        try {
            DiskInfo disk;
            auto volumes = disk.GetDisks();
            for (const auto& v : volumes) {
                nlohmann::json dj;
                dj["label"] = v.label;
                dj["fileSystem"] = v.fileSystem;
                dj["total"] = v.totalSize;
                dj["used"] = v.usedSpace;
                cfg.AppendToArray("disks", std::move(dj));
            }
        } catch (...) {}

        // Temperatures
        try {
            auto temps = TemperatureWrapper::GetTemperatures();
            nlohmann::json tempObj = nlohmann::json::object();
            for (const auto& t : temps) {
                tempObj[t.first] = t.second;
            }
            cfg.SetJson("temperatures", std::move(tempObj));
        } catch (...) {}

        // Save to temp file, read back, print to stdout
        if (cfg.Save()) {
            std::ifstream in(tmpPath);
            if (in) {
                std::cout << in.rdbuf();
            }
        }
        std::cout << std::endl;
        std::remove(tmpPath.c_str());

        TemperatureWrapper::Cleanup();
        return 0;
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

    // Initialize IPC server (schema-driven pipeline for C# Avalonia)
    tcmt::ipc::IPCServer ipcServer;
    {
        using FT = tcmt::ipc::FieldType;
        tcmt::ipc::SchemaHeader schemaHdr;
        schemaHdr.totalSize = sizeof(tcmt::ipc::IPCDataBlock);
        std::vector<tcmt::ipc::FieldDef> fields;
        auto add = [&](const char* name, uint32_t offset, uint16_t size, FT type, uint32_t count = 0) {
            tcmt::ipc::FieldDef f{};
            f.offset = offset; f.size = size; f.count = count;
            f.type = static_cast<uint8_t>(type);
            std::strncpy(f.name, name, tcmt::ipc::IPC_FIELD_NAME_LEN - 1);
            fields.push_back(f);
        };
        auto addS  = [&](const char* n, uint32_t o, uint16_t s) { add(n, o, s, FT::String); };
        auto addB  = [&](const char* n, uint32_t o) { add(n, o, 1, FT::Bool); };
        auto addI  = [&](const char* n, uint32_t o) { add(n, o, 4, FT::Int32); };
        auto addU8 = [&](const char* n, uint32_t o) { add(n, o, 1, FT::UInt8); };
        auto addU64= [&](const char* n, uint32_t o) { add(n, o, 8, FT::UInt64); };
        auto addF  = [&](const char* n, uint32_t o) { add(n, o, 4, FT::Float32); };
        auto addF64= [&](const char* n, uint32_t o) { add(n, o, 8, FT::Float64); };
        using B = tcmt::ipc::IPCDataBlock;

        // CPU
        addS("cpu/name",              offsetof(B, cpuName), 64);
        addU8("cpu/cores/physical",    offsetof(B, physicalCores));
        addU8("cpu/cores/logical",     offsetof(B, logicalCores));
        addU8("cpu/cores/performance", offsetof(B, performanceCores));
        addU8("cpu/cores/efficiency",  offsetof(B, efficiencyCores));
        addF("cpu/usage",             offsetof(B, cpuUsage));
        addF("cpu/freq/pCore",        offsetof(B, pCoreFreq));
        addF("cpu/freq/eCore",        offsetof(B, eCoreFreq));
        addF("cpu/temperature",       offsetof(B, cpuTemp));
        addB("cpu/hyperThreading",    offsetof(B, hyperThreading));
        addB("cpu/virtualization",    offsetof(B, virtualization));
        addF("cpu/sampleIntervalMs",  offsetof(B, cpuSampleIntervalMs));

        // Memory
        addU64("memory/total",        offsetof(B, totalMemory));
        addU64("memory/used",         offsetof(B, usedMemory));
        addU64("memory/available",    offsetof(B, availableMemory));
        addU64("memory/compressed",   offsetof(B, compressedMemory));

        // Battery / Power
        addI("battery/percent",       offsetof(B, batteryPercent));
        addB("battery/acOnline",      offsetof(B, acOnline));

        // OS
        addS("os/version",            offsetof(B, osVersion), 128);

        // GPU
        addS("gpu/0/name",            offsetof(B, gpuName), 48);
        addS("gpu/0/brand",           offsetof(B, gpuBrand), 32);
        addU64("gpu/0/memory",        offsetof(B, gpuMemory));
        addF("gpu/0/memoryPercent",   offsetof(B, gpuMemoryPercent));
        addF("gpu/0/usage",           offsetof(B, gpuUsage));
        addF("gpu/0/temperature",     offsetof(B, gpuTemp));
        addB("gpu/0/isVirtual",       offsetof(B, gpuIsVirtual));

        // Disks (up to 4)
        for (int i = 0; i < 4; ++i) {
            char p[32]; snprintf(p, sizeof(p), "disk/%d/", i);
            uint32_t base = offsetof(B, disks) + i * sizeof(B::DiskSlot);
            addS((std::string(p)+"label").c_str(), base + offsetof(B::DiskSlot, label), 32);
            addU64((std::string(p)+"total").c_str(), base + offsetof(B::DiskSlot, totalSize));
            addU64((std::string(p)+"used").c_str(),  base + offsetof(B::DiskSlot, usedSpace));
            addU64((std::string(p)+"free").c_str(),  base + offsetof(B::DiskSlot, freeSpace));
            addS((std::string(p)+"fs").c_str(),      base + offsetof(B::DiskSlot, fs), 16);
        }

        // Network adapters (up to 4)
        for (int i = 0; i < 4; ++i) {
            char p[32]; snprintf(p, sizeof(p), "net/%d/", i);
            uint32_t base = offsetof(B, adapters) + i * sizeof(B::NetSlot);
            addS((std::string(p)+"name").c_str(),   base + offsetof(B::NetSlot, name), 32);
            addS((std::string(p)+"ip").c_str(),     base + offsetof(B::NetSlot, ip), 16);
            addS((std::string(p)+"mac").c_str(),    base + offsetof(B::NetSlot, mac), 18);
            addS((std::string(p)+"type").c_str(),   base + offsetof(B::NetSlot, type), 16);
            addU64((std::string(p)+"speed").c_str(),  base + offsetof(B::NetSlot, speed));
            addU64((std::string(p)+"downloadSpeed").c_str(), base + offsetof(B::NetSlot, downloadSpeed));
            addU64((std::string(p)+"uploadSpeed").c_str(),   base + offsetof(B::NetSlot, uploadSpeed));
        }

        // Temperatures (up to 10)
        for (int i = 0; i < 10; ++i) {
            char p[32]; snprintf(p, sizeof(p), "sensor/%d/", i);
            uint32_t base = offsetof(B, temperatures) + i * sizeof(B::TempSlot);
            addS((std::string(p)+"name").c_str(), base + offsetof(B::TempSlot, name), 64);
            addF((std::string(p)+"value").c_str(), base + offsetof(B::TempSlot, value));
        }

        ipcServer.UpdateSchema(schemaHdr, fields);
    }
    if (ipcServer.Start()) {
        Logger::Info("IPC server started on " + std::string(tcmt::ipc::IPC_SOCK_PATH));
    } else {
        Logger::Warn("IPC server start failed: " + ipcServer.GetLastError());
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

            // === Battery / power (shared between TUI and SHM) ===
            int cachedBatteryPercent = -1;
            bool cachedAcOnline = false;
            try {
                PowerInfo power;
                power.Detect();
                if (!power.batteries.empty()) {
                    cachedBatteryPercent = static_cast<int>(power.batteries[0].chargePercent);
                }
                cachedAcOnline = power.acOnline;
            } catch (...) {}

            // === Build TuiData snapshot ===
            tcmt::TuiData data;
            data.osVersion = os.GetVersion();
            data.batteryPercent = cachedBatteryPercent;
            data.acOnline = cachedAcOnline;
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

                // Compressed memory via host_statistics64
                vm_statistics64_data_t vmStats;
                mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
                if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                                      (host_info64_t)&vmStats, &count) == KERN_SUCCESS) {
                    data.compressedMemory = (uint64_t)vmStats.compressor_page_count * vm_kernel_page_size;
                }
            } catch (const std::exception& e) {
                Logger::Error("Memory error: " + std::string(e.what()));
            }

            // GPU
            if (gpuInfo) {
                gpuInfo->RefreshUsage();
                const auto& gpus = gpuInfo->GetGpuData();
                for (const auto& gpu : gpus) {
                    if (data.gpuName.empty()) {
                        std::string gn(gpu.name.begin(), gpu.name.end());
                        data.gpuName = gn;
                        data.gpuMemory = gpu.dedicatedMemory;
                    }
                    if (data.gpuUsage == 0.0 && gpu.usage > 0.0) {
                        data.gpuUsage = gpu.usage;
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
                    ni.mac = adapter.mac;
                    ni.type = adapter.adapterType;
                    ni.speed = adapter.speed;
                    ni.downloadSpeed = adapter.downloadSpeed;
                    ni.uploadSpeed = adapter.uploadSpeed;
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
                sysInfo.osVersion = os.GetVersion();
                sysInfo.batteryPercent = cachedBatteryPercent;
                sysInfo.acOnline = cachedAcOnline;
                sysInfo.cpuName = data.cpuName;
                sysInfo.cpuUsage = data.cpuUsage;
                sysInfo.performanceCoreFreq = data.pCoreFreq;
                sysInfo.efficiencyCoreFreq = data.eCoreFreq;
                sysInfo.cpuTemperature = data.cpuTemp;
                sysInfo.totalMemory = data.totalMemory;
                sysInfo.usedMemory = data.usedMemory;
                sysInfo.availableMemory = data.availableMemory;
                sysInfo.compressedMemory = data.compressedMemory;
                sysInfo.physicalCores = data.physicalCores;
                sysInfo.performanceCores = data.performanceCores;
                sysInfo.efficiencyCores = data.efficiencyCores;
                sysInfo.gpuUsage = data.gpuUsage;
                sysInfo.gpuTemperature = data.gpuTemp;
                sysInfo.gpuName = data.gpuName;
                sysInfo.gpuMemory = data.gpuMemory;

                // Network adapters
                for (const auto& adapter : data.adapters) {
                    NetworkAdapterData nad{};
                    auto u16name = Platform::StringConverter::Utf8ToChar16(adapter.name);
                    auto u16ip = Platform::StringConverter::Utf8ToChar16(adapter.ip);
                    auto u16mac = Platform::StringConverter::Utf8ToChar16(adapter.mac);
                    auto u16type = Platform::StringConverter::Utf8ToChar16(adapter.type);
                    size_t copyLen = std::min(u16name.length(), size_t(127));
                    for (size_t i = 0; i < copyLen; ++i) nad.name[i] = u16name[i];
                    nad.name[copyLen] = u'\0';
                    copyLen = std::min(u16ip.length(), size_t(63));
                    for (size_t i = 0; i < copyLen; ++i) nad.ipAddress[i] = u16ip[i];
                    nad.ipAddress[copyLen] = u'\0';
                    copyLen = std::min(u16mac.length(), size_t(31));
                    for (size_t i = 0; i < copyLen; ++i) nad.mac[i] = u16mac[i];
                    nad.mac[copyLen] = u'\0';
                    copyLen = std::min(u16type.length(), size_t(31));
                    for (size_t i = 0; i < copyLen; ++i) nad.adapterType[i] = u16type[i];
                    nad.adapterType[copyLen] = u'\0';
                    nad.speed = adapter.speed;
                    nad.downloadSpeed = adapter.downloadSpeed;
                    nad.uploadSpeed = adapter.uploadSpeed;
                    sysInfo.adapters.push_back(nad);
                }

                // Disks
                for (const auto& disk : data.disks) {
                    DiskData dd{};
                    dd.label = disk.label;
                    dd.fileSystem = disk.fileSystem;
                    dd.totalSize = disk.totalSize;
                    dd.usedSpace = disk.usedSpace;
                    dd.freeSpace = disk.totalSize - disk.usedSpace;
                    sysInfo.disks.push_back(dd);
                }

                // Temperatures
                sysInfo.temperatures = data.temperatures;

                SharedMemoryManager::WriteToSharedMemory(sysInfo);

                // Write to IPC shared memory (schema-driven, for C# Avalonia)
                if (ipcServer.IsRunning()) {
                    auto* b = static_cast<tcmt::ipc::IPCDataBlock*>(ipcServer.GetShmPtr());
                    if (b) {
                        // CPU
                        std::strncpy(b->cpuName, data.cpuName.c_str(), 63);
                        b->cpuName[63] = '\0';
                        b->physicalCores = static_cast<uint8_t>(data.physicalCores);
                        b->logicalCores = static_cast<uint8_t>(data.physicalCores); // Apple Silicon: logical=physical
                        b->performanceCores = static_cast<uint8_t>(data.performanceCores);
                        b->efficiencyCores = static_cast<uint8_t>(data.efficiencyCores);
                        b->cpuUsage = static_cast<float>(data.cpuUsage);
                        b->pCoreFreq = static_cast<float>(data.pCoreFreq);
                        b->eCoreFreq = static_cast<float>(data.eCoreFreq);
                        b->cpuTemp = static_cast<float>(data.cpuTemp);
                        b->hyperThreading = false;
                        b->virtualization = false;
                        b->cpuSampleIntervalMs = 500.0f;
                        // Memory
                        b->totalMemory = data.totalMemory;
                        b->usedMemory = data.usedMemory;
                        b->availableMemory = data.availableMemory;
                        b->compressedMemory = data.compressedMemory;
                        // Battery / power
                        b->batteryPercent = data.batteryPercent;
                        b->acOnline = data.acOnline;
                        // OS
                        std::strncpy(b->osVersion, data.osVersion.c_str(), 127);
                        b->osVersion[127] = '\0';
                        // GPU
                        std::strncpy(b->gpuName, data.gpuName.c_str(), 47);
                        b->gpuName[47] = '\0';
                        b->gpuMemory = data.gpuMemory;
                        b->gpuMemoryPercent = static_cast<float>(data.gpuMemoryPercent);
                        b->gpuUsage = static_cast<float>(data.gpuUsage);
                        b->gpuTemp = static_cast<float>(data.gpuTemp);
                        b->gpuIsVirtual = false;
                        // Disks (up to 4)
                        b->diskCount = 0;
                        for (size_t di = 0; di < std::min(data.disks.size(), size_t(4)); ++di) {
                            auto& d = b->disks[di];
                            std::strncpy(d.label, data.disks[di].label.c_str(), 31);
                            d.label[31] = '\0';
                            d.totalSize = data.disks[di].totalSize;
                            d.usedSpace = data.disks[di].usedSpace;
                            d.freeSpace = data.disks[di].totalSize - data.disks[di].usedSpace;
                            std::strncpy(d.fs, data.disks[di].fileSystem.c_str(), 15);
                            d.fs[15] = '\0';
                            b->diskCount++;
                        }
                        // Network adapters (up to 4, only connected)
                        b->adapterCount = 0;
                        for (size_t ai = 0; ai < data.adapters.size() && b->adapterCount < 4; ++ai) {
                            if (data.adapters[ai].ip.empty()) continue;
                            auto& n = b->adapters[b->adapterCount];
                            std::strncpy(n.name, data.adapters[ai].name.c_str(), 31);
                            std::strncpy(n.ip,   data.adapters[ai].ip.c_str(), 15);
                            std::strncpy(n.mac,  data.adapters[ai].mac.c_str(), 17);
                            std::strncpy(n.type, data.adapters[ai].type.c_str(), 15);
                            n.speed = data.adapters[ai].speed;
                            n.downloadSpeed = data.adapters[ai].downloadSpeed;
                            n.uploadSpeed = data.adapters[ai].uploadSpeed;
                            b->adapterCount++;
                        }
                        // Temperatures (up to 10)
                        b->tempCount = 0;
                        for (size_t ti = 0; ti < std::min(data.temperatures.size(), size_t(10)); ++ti) {
                            std::strncpy(b->temperatures[ti].name, data.temperatures[ti].first.c_str(), 63);
                            b->temperatures[ti].name[63] = '\0';
                            b->temperatures[ti].value = static_cast<float>(data.temperatures[ti].second);
                            b->tempCount++;
                        }
                    }
                }
            }

            // Sleep
            auto loopEnd = std::chrono::high_resolution_clock::now();
            int loopMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
                loopEnd - loopStart).count();
            int sleepMs = std::max(500 - loopMs, 50);  // 2 Hz update
            // Sleep with responsive exit (check every 50ms)
            for (int s = 0; s < 10 && !g_shouldExit.load(); ++s) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
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
    ipcServer.Stop();
    Logger::Info("IPC server stopped");
    SharedMemoryManager::CleanupSharedMemory();
    Logger::Info("Done.");
    return 0;
}
