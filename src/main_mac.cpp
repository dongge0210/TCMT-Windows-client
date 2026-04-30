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
#include <vector>
#include <algorithm>
#include <string>

#include "core/cpu/CpuInfo.h"
#include "core/gpu/GpuInfo.h"
#include "core/memory/MemoryInfo.h"
#include "core/network/NetworkAdapter.h"
#include "core/os/OSInfo.h"
#include "core/disk/DiskInfo.h"
#include "core/DataStruct/DataStruct.h"
#include "core/temperature/TemperatureWrapper.h"
#include "core/IPC/IPCServer.h"
#include "core/IPC/IPCData.h"
#include "core/IPC/IpcCommandHandler.h"
#include "core/Utils/Logger.h"
#include "tui/TuiApp.h"
#include <nlohmann/json.hpp>

// ======================== Signal Handling ========================
static std::atomic<bool> g_shouldExit{false};

static void SignalHandler(int sig) {
    (void)sig;
    g_shouldExit = true;
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

// ======================== JSON Output ========================

static std::string WcharToStr(const WCHAR* wstr) {
    if (!wstr) return {};
    std::string result;
    for (const WCHAR* p = wstr; *p; ++p) {
        result += static_cast<char>(*p);
    }
    return result;
}

static void StrToWchar(WCHAR* dest, size_t max, const std::string& src) {
    size_t n = std::min(max - 1, src.size());
    for (size_t i = 0; i < n; ++i) {
        dest[i] = static_cast<WCHAR>(static_cast<unsigned char>(src[i]));
    }
    dest[n] = 0;
}

static std::string GetIsoTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    gmtime_r(&time, &tm);
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

static std::string GetBrandFromName(const std::string& name) {
    if (name.find("NVIDIA") != std::string::npos) return "NVIDIA";
    if (name.find("AMD") != std::string::npos) return "AMD";
    if (name.find("Intel") != std::string::npos) return "Intel";
    if (name.find("Apple") != std::string::npos) return "Apple";
    return "Unknown";
}

static nlohmann::json SystemInfoToJson(const SystemInfo& sysInfo) {
    nlohmann::json j;

    j["cpu"]["name"] = sysInfo.cpuName.empty() ? nullptr : sysInfo.cpuName;
    j["cpu"]["usage"] = sysInfo.cpuUsage;
    j["cpu"]["temperature"] = sysInfo.cpuTemperature;
    j["cpu"]["cores"]["physical"] = sysInfo.physicalCores;
    j["cpu"]["cores"]["logical"] = sysInfo.logicalCores;
    j["cpu"]["cores"]["performance"] = sysInfo.performanceCores;
    j["cpu"]["cores"]["efficiency"] = sysInfo.efficiencyCores;
    j["cpu"]["freq"]["pCore"] = sysInfo.performanceCoreFreq;
    j["cpu"]["freq"]["eCore"] = sysInfo.efficiencyCoreFreq;

    j["memory"]["total"] = sysInfo.totalMemory;
    j["memory"]["used"] = sysInfo.usedMemory;
    j["memory"]["available"] = sysInfo.availableMemory;
    j["memory"]["compressed"] = sysInfo.compressedMemory;

    if (!sysInfo.gpus.empty()) {
        const auto& gpu = sysInfo.gpus[0];
        j["gpu"]["name"] = WcharToStr(gpu.name);
        j["gpu"]["brand"] = WcharToStr(gpu.brand);
        j["gpu"]["usage"] = gpu.usage;
        j["gpu"]["memory"] = gpu.memory;
        j["gpu"]["coreClock"] = gpu.coreClock;
    }
    j["gpu"]["temperature"] = sysInfo.gpuTemperature;

    auto& netArr = j["network"] = nlohmann::json::array();
    for (const auto& a : sysInfo.adapters) {
        nlohmann::json net;
        net["name"] = WcharToStr(a.name);
        net["mac"] = WcharToStr(a.mac);
        net["ip"] = WcharToStr(a.ipAddress);
        net["type"] = WcharToStr(a.adapterType);
        net["speed"] = a.speed;
        net["download"] = a.downloadSpeed;
        net["upload"] = a.uploadSpeed;
        netArr.push_back(std::move(net));
    }

    auto& diskArr = j["disks"] = nlohmann::json::array();
    for (const auto& d : sysInfo.disks) {
        nlohmann::json disk;
        disk["letter"] = std::string(1, d.letter);
        disk["label"] = d.label;
        disk["fileSystem"] = d.fileSystem;
        disk["total"] = d.totalSize;
        disk["used"] = d.usedSpace;
        disk["free"] = d.freeSpace;
        diskArr.push_back(std::move(disk));
    }

    j["timestamp"] = GetIsoTimestamp();

    return j;
}

static SystemInfo CollectSystemInfo() {
    SystemInfo sysInfo = {};

    try {
        CpuInfo cpuInfo;
        sysInfo.cpuName = cpuInfo.GetName();
        int totalCores = cpuInfo.GetTotalCores();
        int pCores = cpuInfo.GetLargeCores();
        int eCores = cpuInfo.GetSmallCores();
        sysInfo.physicalCores = pCores + eCores;
        sysInfo.logicalCores = totalCores;
        sysInfo.performanceCores = pCores;
        sysInfo.efficiencyCores = eCores;
        sysInfo.cpuUsage = cpuInfo.GetUsage();
        sysInfo.performanceCoreFreq = cpuInfo.GetLargeCoreSpeed();
        sysInfo.efficiencyCoreFreq = cpuInfo.GetSmallCoreSpeed();
    } catch (const std::exception& e) {
        Logger::Error("CollectSystemInfo CPU: " + std::string(e.what()));
    }

    try {
        MemoryInfo mem;
        sysInfo.totalMemory = mem.GetTotalPhysical();
        sysInfo.usedMemory = mem.GetTotalPhysical() - mem.GetAvailablePhysical();
        sysInfo.availableMemory = mem.GetAvailablePhysical();

        vm_statistics64_data_t vmStats;
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                              (host_info64_t)&vmStats, &count) == KERN_SUCCESS) {
            sysInfo.compressedMemory = (uint64_t)vmStats.compressor_page_count * vm_kernel_page_size;
        }
    } catch (const std::exception& e) {
        Logger::Error("CollectSystemInfo Memory: " + std::string(e.what()));
    }

    try {
        GpuInfo gpuInfo;
        gpuInfo.RefreshUsage();
        const auto& gpus = gpuInfo.GetGpuData();
        for (const auto& g : gpus) {
            GPUData gpuData = {};
            std::string gname(g.name.begin(), g.name.end());
            StrToWchar(gpuData.name, 128, gname);
            std::string brand = GetBrandFromName(gname);
            StrToWchar(gpuData.brand, 64, brand);
            gpuData.memory = g.dedicatedMemory;
            gpuData.coreClock = static_cast<double>(g.coreClock);
            gpuData.usage = g.usage;
            gpuData.isVirtual = false;
            sysInfo.gpus.push_back(gpuData);

            if (sysInfo.gpuName.empty()) {
                sysInfo.gpuName = gname;
                sysInfo.gpuBrand = brand;
                sysInfo.gpuMemory = g.dedicatedMemory;
                sysInfo.gpuCoreFreq = static_cast<double>(g.coreClock);
                sysInfo.gpuUsage = g.usage;
            }
        }
    } catch (const std::exception& e) {
        Logger::Error("CollectSystemInfo GPU: " + std::string(e.what()));
    }

    try {
        NetworkAdapter net;
        const auto& adapters = net.GetAdapters();
        for (const auto& a : adapters) {
            NetworkAdapterData data = {};
            StrToWchar(data.name, 128, a.name);
            StrToWchar(data.mac, 32, a.mac);
            StrToWchar(data.ipAddress, 64, a.ip);
            StrToWchar(data.adapterType, 32, a.adapterType);
            data.speed = a.speed;
            sysInfo.adapters.push_back(data);
        }
    } catch (const std::exception& e) {
        Logger::Error("CollectSystemInfo Network: " + std::string(e.what()));
    }

    try {
        auto temps = TemperatureWrapper::GetTemperatures();
        sysInfo.temperatures = temps;
        for (const auto& [name, temp] : temps) {
            bool isGpu = (name.find("TG") != std::string::npos ||
                          name.find("GPU") != std::string::npos);
            if (isGpu && sysInfo.gpuTemperature == 0) sysInfo.gpuTemperature = temp;
            else if (!isGpu && sysInfo.cpuTemperature == 0) sysInfo.cpuTemperature = temp;
        }
    } catch (const std::exception& e) {
        Logger::Error("CollectSystemInfo Temperature: " + std::string(e.what()));
    }

    try {
        DiskInfo disk;
        auto volumes = disk.GetDisks();
        if (volumes.size() <= 8) {
            sysInfo.disks = volumes;
        }
    } catch (const std::exception& e) {
        Logger::Error("CollectSystemInfo Disk: " + std::string(e.what()));
    }

    sysInfo.lastUpdate = Platform::SystemTime::Now();
    return sysInfo;
}

// ======================== Main ========================
int main(int argc, char* argv[]) {

    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
    std::signal(SIGHUP, SignalHandler);
    std::signal(SIGPIPE, SIG_IGN); // Prevent crash on broken pipe
    Logger::Info("main: starting TCMT-M (PID=" + std::to_string(getpid()) + ")");

    try {
        Logger::Initialize("system_monitor.log");
        Logger::EnableConsoleOutput(false);
        Logger::SetLogLevel(LOG_INFO);
        Logger::Info("TCMT macOS Client starting (TUI mode)...");
    } catch (const std::exception& e) {
        std::cerr << "Logger init failed: " << e.what() << std::endl;
        return 1;
    }

    // Parse CLI arguments
    bool jsonMode = false;
    bool daemonMode = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--json") jsonMode = true;
        if (arg == "--daemon") daemonMode = true;
    }

    if (daemonMode) {
        Logger::Info("--daemon mode requested: not yet implemented");
        return 0;
    }

    if (jsonMode) {
        Logger::Info("--json mode: collecting one round of data...");

        TemperatureWrapper::Initialize();

        SystemInfo sysInfo = CollectSystemInfo();

        nlohmann::json j = SystemInfoToJson(sysInfo);
        std::cout << j.dump(2) << std::endl;

        TemperatureWrapper::Cleanup();
        return 0;
    }

    TemperatureWrapper::Initialize();
    Logger::Debug("TemperatureWrapper initialized");

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

    // ====== IPC Server (Unix socket + shared memory for GUI clients) ======
    tcmt::ipc::IPCServer ipcServer;
    ipcServer.Start();
    if (ipcServer.IsRunning()) {
        Logger::Info("IPC server started");
    } else {
        Logger::Warn("IPC server failed to start");
    }

    // Build schema fields from IPCDataBlock layout
    tcmt::ipc::SchemaHeader ipcHdr;
    ipcHdr.magic = tcmt::ipc::IPC_MAGIC;
    ipcHdr.version = tcmt::ipc::IPC_VERSION;
    ipcHdr.totalSize = sizeof(tcmt::ipc::IPCDataBlock);
    ipcHdr.stringBlockSize = 0;

    std::vector<tcmt::ipc::FieldDef> ipcFields;
    auto addField = [&](const char* name, tcmt::ipc::FieldType type, size_t offset, size_t size,
                        const char* units = "", float min = 0, float max = 0) {
        if (ipcFields.size() >= tcmt::ipc::IPC_MAX_FIELDS) return;
        tcmt::ipc::FieldDef f;
        f.id = (uint32_t)ipcFields.size() + 1;
        f.type = static_cast<uint8_t>(type);
        f.offset = (uint32_t)offset;
        f.size = (uint16_t)size;
        f.count = 1;
        f.strOffset = 0;
        f.flags = 0;
        f.minVal = min;
        f.maxVal = max;
        std::strncpy(f.name, name, tcmt::ipc::IPC_FIELD_NAME_LEN - 1);
        std::strncpy(f.units, units, tcmt::ipc::IPC_FIELD_UNITS_LEN - 1);
        ipcFields.push_back(f);
    };

    // ─── CPU ───
    addField("cpu/name",              tcmt::ipc::FieldType::String,  offsetof(tcmt::ipc::IPCDataBlock, cpuName),       64);
    addField("cpu/cores/physical",    tcmt::ipc::FieldType::UInt8,   offsetof(tcmt::ipc::IPCDataBlock, physicalCores), 1);
    addField("cpu/cores/performance", tcmt::ipc::FieldType::UInt8,   offsetof(tcmt::ipc::IPCDataBlock, performanceCores), 1);
    addField("cpu/cores/efficiency",  tcmt::ipc::FieldType::UInt8,   offsetof(tcmt::ipc::IPCDataBlock, efficiencyCores), 1);
    addField("cpu/usage",             tcmt::ipc::FieldType::Float32, offsetof(tcmt::ipc::IPCDataBlock, cpuUsage),     4, "%", 0, 100);
    addField("cpu/freq/pCore",        tcmt::ipc::FieldType::Float32, offsetof(tcmt::ipc::IPCDataBlock, pCoreFreq),    4, "MHz");
    addField("cpu/freq/eCore",        tcmt::ipc::FieldType::Float32, offsetof(tcmt::ipc::IPCDataBlock, eCoreFreq),    4, "MHz");
    addField("cpu/temperature",       tcmt::ipc::FieldType::Float32, offsetof(tcmt::ipc::IPCDataBlock, cpuTemp),      4, "C", -50, 150);

    // ─── Memory ───
    addField("memory/total",         tcmt::ipc::FieldType::UInt64, offsetof(tcmt::ipc::IPCDataBlock, totalMemory),      8, "B");
    addField("memory/used",          tcmt::ipc::FieldType::UInt64, offsetof(tcmt::ipc::IPCDataBlock, usedMemory),       8, "B");
    addField("memory/available",     tcmt::ipc::FieldType::UInt64, offsetof(tcmt::ipc::IPCDataBlock, availableMemory),  8, "B");
    addField("memory/compressed",    tcmt::ipc::FieldType::UInt64, offsetof(tcmt::ipc::IPCDataBlock, compressedMemory), 8, "B");

    // ─── GPU ───
    addField("gpu/0/name",        tcmt::ipc::FieldType::String,  offsetof(tcmt::ipc::IPCDataBlock, gpuName),   48);
    addField("gpu/0/memory",      tcmt::ipc::FieldType::UInt64,  offsetof(tcmt::ipc::IPCDataBlock, gpuMemory), 8, "B");
    addField("gpu/0/usage",       tcmt::ipc::FieldType::Float32, offsetof(tcmt::ipc::IPCDataBlock, gpuUsage),  4, "%", 0, 100);
    addField("gpu/0/temperature", tcmt::ipc::FieldType::Float32, offsetof(tcmt::ipc::IPCDataBlock, gpuTemp),   4, "C", -50, 150);

    // ─── Disks ───
    for (int d = 0; d < 2; ++d) {
        auto base = offsetof(tcmt::ipc::IPCDataBlock, disks) + d * sizeof(tcmt::ipc::IPCDataBlock::DiskSlot);
        addField(("disk/" + std::to_string(d) + "/label").c_str(), tcmt::ipc::FieldType::String, base + offsetof(tcmt::ipc::IPCDataBlock::DiskSlot, label),     32);
        addField(("disk/" + std::to_string(d) + "/total").c_str(), tcmt::ipc::FieldType::UInt64, base + offsetof(tcmt::ipc::IPCDataBlock::DiskSlot, totalSize), 8, "B");
        addField(("disk/" + std::to_string(d) + "/used").c_str(),  tcmt::ipc::FieldType::UInt64, base + offsetof(tcmt::ipc::IPCDataBlock::DiskSlot, usedSpace), 8, "B");
        addField(("disk/" + std::to_string(d) + "/fs").c_str(),    tcmt::ipc::FieldType::String,  base + offsetof(tcmt::ipc::IPCDataBlock::DiskSlot, fs),        16);
    }

    // ─── Network ───
    for (int n = 0; n < 2; ++n) {
        auto base = offsetof(tcmt::ipc::IPCDataBlock, adapters) + n * sizeof(tcmt::ipc::IPCDataBlock::NetSlot);
        addField(("net/" + std::to_string(n) + "/name").c_str(),  tcmt::ipc::FieldType::String, base + offsetof(tcmt::ipc::IPCDataBlock::NetSlot, name),  32);
        addField(("net/" + std::to_string(n) + "/ip").c_str(),    tcmt::ipc::FieldType::String, base + offsetof(tcmt::ipc::IPCDataBlock::NetSlot, ip),    16);
        addField(("net/" + std::to_string(n) + "/mac").c_str(),   tcmt::ipc::FieldType::String, base + offsetof(tcmt::ipc::IPCDataBlock::NetSlot, mac),   18);
        addField(("net/" + std::to_string(n) + "/type").c_str(),  tcmt::ipc::FieldType::String, base + offsetof(tcmt::ipc::IPCDataBlock::NetSlot, type),  16);
        addField(("net/" + std::to_string(n) + "/speed").c_str(), tcmt::ipc::FieldType::UInt64, base + offsetof(tcmt::ipc::IPCDataBlock::NetSlot, speed), 8, "bps");
    }

    // Timestamp
    addField("timestamp", tcmt::ipc::FieldType::String, offsetof(tcmt::ipc::IPCDataBlock, timestamp), 16);

    ipcHdr.fieldCount = (uint16_t)ipcFields.size();
    ipcServer.UpdateSchema(ipcHdr, ipcFields);

    // Check if we have a TTY — if not, run in headless IPC-only mode
    bool hasTty = isatty(STDIN_FILENO) == 1;

    tcmt::TuiApp tuiApp;
    if (hasTty) {
        tuiApp.SetLogBuffer(&Logger::GetTuiBuffer());
        tuiApp.Start();
        Logger::Info("TUI started");
    } else {
        Logger::Info("No TTY detected — running in headless IPC-only mode");
    }
    static std::string cachedCpuName;
    static int cachedTotalCores = 0;
    static int cachedPCores = 0;
    static int cachedECores = 0;
    if (cpuInfo) {
        cachedCpuName = cpuInfo->GetName();
        cachedTotalCores = cpuInfo->GetTotalCores();
        cachedPCores = cpuInfo->GetLargeCores();
        cachedECores = cpuInfo->GetSmallCores();
    }

    // Get IPC shared memory pointer for writing
    void* shmPtr = ipcServer.GetShmPtr();
    size_t shmSize = ipcServer.GetShmSize();

    while (!g_shouldExit.load() && (hasTty ? tuiApp.IsRunning() : true)) {
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

            if (cpuInfo) {
                data.cpuUsage = cpuInfo->GetUsage();
                data.pCoreFreq = cpuInfo->GetLargeCoreSpeed();
                data.eCoreFreq = cpuInfo->GetSmallCoreSpeed();
            }

            static MemoryInfo mem;
            try {
                data.totalMemory = mem.GetTotalPhysical();
                data.usedMemory = mem.GetTotalPhysical() - mem.GetAvailablePhysical();
                data.availableMemory = mem.GetAvailablePhysical();

                vm_statistics64_data_t vmStats;
                mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
                if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                                      (host_info64_t)&vmStats, &count) == KERN_SUCCESS) {
                    data.compressedMemory = (uint64_t)vmStats.compressor_page_count * vm_kernel_page_size;
                }
            } catch (const std::exception& e) {
                Logger::Error("Memory error: " + std::string(e.what()));
            }

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
                    data.adapters.push_back(ni);
                }
            } catch (const std::exception& e) {
                Logger::Error("Network error: " + std::string(e.what()));
            }

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

            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm;
            localtime_r(&time, &tm);
            char tbuf[64];
            std::strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &tm);
            data.timestamp = tbuf;

            // === Write to IPC shared memory ===
            if (shmPtr && shmSize >= sizeof(tcmt::ipc::IPCDataBlock)) {
                tcmt::ipc::IPCDataBlock* block = static_cast<tcmt::ipc::IPCDataBlock*>(shmPtr);
                std::memset(block, 0, sizeof(tcmt::ipc::IPCDataBlock));

                if (cpuInfo) {
                    std::strncpy(block->cpuName, cpuInfo->GetName().c_str(), 63);
                    block->physicalCores    = (uint8_t)cpuInfo->GetTotalCores();
                    block->performanceCores = (uint8_t)cpuInfo->GetLargeCores();
                    block->efficiencyCores  = (uint8_t)cpuInfo->GetSmallCores();
                    block->cpuUsage  = (float)cpuInfo->GetUsage();
                    block->pCoreFreq = (float)cpuInfo->GetLargeCoreSpeed();
                    block->eCoreFreq = (float)cpuInfo->GetSmallCoreSpeed();
                }
                std::strncpy(block->timestamp, tbuf, 15);
                block->cpuTemp = (float)data.cpuTemp;
                block->totalMemory   = data.totalMemory;
                block->usedMemory    = data.usedMemory;
                block->availableMemory = data.availableMemory;
                block->compressedMemory = data.compressedMemory;

                if (!data.gpuName.empty()) {
                    std::strncpy(block->gpuName, data.gpuName.c_str(), 47);
                }
                block->gpuMemory = data.gpuMemory;
                block->gpuUsage  = (float)data.gpuUsage;
                block->gpuTemp   = (float)data.gpuTemp;

                for (size_t d = 0; d < data.disks.size() && d < 2; ++d) {
                    std::strncpy(block->disks[d].label, data.disks[d].label.c_str(), 31);
                    block->disks[d].totalSize = data.disks[d].totalSize;
                    block->disks[d].usedSpace = data.disks[d].usedSpace;
                    std::strncpy(block->disks[d].fs, data.disks[d].fileSystem.c_str(), 15);
                }
                block->diskCount = (uint8_t)std::min(data.disks.size(), size_t(2));

                for (size_t n = 0; n < data.adapters.size() && n < 2; ++n) {
                    std::strncpy(block->adapters[n].name, data.adapters[n].name.c_str(), 31);
                    std::strncpy(block->adapters[n].ip, data.adapters[n].ip.c_str(), 15);
                    std::strncpy(block->adapters[n].mac, data.adapters[n].mac.c_str(), 17);
                    std::strncpy(block->adapters[n].type, data.adapters[n].type.c_str(), 15);
                    block->adapters[n].speed = data.adapters[n].speed;
                }
                block->adapterCount = (uint8_t)std::min(data.adapters.size(), size_t(2));

                // Schema was broadcast once at startup — no need to re-broadcast
            }

            // Process frontend IPC commands from shared memory mailbox
            if (shmPtr && shmSize >= sizeof(tcmt::ipc::IPCDataBlock)) {
                auto* block = static_cast<tcmt::ipc::IPCDataBlock*>(shmPtr);
                IpcCommandHandler::Instance().ProcessCommands(
                    &block->command, &block->commandAck);
            }

            // Update TUI (if running)
            if (hasTty) {
                tuiApp.UpdateData(data);
            }

            auto loopEnd = std::chrono::high_resolution_clock::now();
            int loopMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
                loopEnd - loopStart).count();
            for (int s = 0; s < 10 && !g_shouldExit.load(); ++s) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
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
    if (hasTty) tuiApp.Stop();
    ipcServer.Stop();
    TemperatureWrapper::Cleanup();
    Logger::Info("Done.");
    return 0;
}
