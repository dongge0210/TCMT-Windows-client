#pragma unmanaged

#pragma comment(linker, "/STACK:8388608")  // Set stack size

/*
If you see warning MSB8077: Some files are set to C++/CLI but "Enable CLR Support for Single File" property is not defined.
Please ignore this warning - the project structure doesn't support this scenario
*/
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <sddl.h>
#include <Aclapi.h>
#include <conio.h>
#include <eh.h>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")

#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <utility>
#include <thread>
#include <io.h>
#include <fcntl.h>
#include <algorithm>
#include <vector>
#include <mutex>
#include <atomic>
#include <locale>
#include <new>
#include <stdexcept>

#include "core/cpu/CpuInfo.h"
#include "core/gpu/GpuInfo.h"
#include "core/memory/MemoryInfo.h"
#include "core/network/NetworkAdapter.h"
#include "core/os/OSInfo.h"
#include "core/utils/Logger.h"
#include "core/utils/TimeUtils.h"
#include "core/utils/WinUtils.h"
#include "core/utils/WmiManager.h"
#include "core/disk/DiskInfo.h"
#include "core/utils/TpmBridge.h"
#include "core/DataStruct/DataStruct.h"
#include "core/IPC/SharedMemoryManager.h"
#include "core/IPC/NamedPipeServer.h"
#include "core/IPC/IpcCommandHandler.h"
#include "core/temperature/TemperatureWrapper.h"
#include "tui/TuiApp.h"
#include <nlohmann/json.hpp>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")

std::atomic<bool> g_shouldExit{false};
static std::atomic<bool> g_monitoringStarted{false};
static std::atomic<bool> g_comInitialized{false};

static std::mutex g_consoleMutex;

bool CheckForKeyPress();
char GetKeyPress();
void SafeExit(int exitCode);

void SEHTranslator(unsigned int u, EXCEPTION_POINTERS* pExp);
std::string GetSEHExceptionName(DWORD exceptionCode);

void SafeConsoleOutput(const std::string& message);
void SafeConsoleOutput(const std::string& message, int color);
BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        Logger::Info("Received shutdown signal, exiting safely...");
        g_shouldExit = true;
        SafeConsoleOutput("Exiting program...\n", 14);
        return TRUE;
    }
    return FALSE;
}

// Thread-safe console output function implementation
void SafeConsoleOutput(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_consoleMutex);
    try {
        // Use UTF-8 encoding for output
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole != INVALID_HANDLE_VALUE) {
            // Ensure input string is not empty
            if (message.empty()) {
                return;
            }
            
            // Convert UTF-8 string to UTF-16 (Wide Character)
            int wideLength = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, nullptr, 0);
            if (wideLength > 0) {
                std::vector<wchar_t> wideMessage(wideLength);
                if (MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, wideMessage.data(), wideLength)) {
                    // Use WriteConsoleW to output Unicode text directly
                    DWORD written;
                    WriteConsoleW(hConsole, wideMessage.data(), static_cast<DWORD>(wideLength - 1), &written, NULL);
                    return;
                }
            }
            
            // If UTF-8 conversion fails, fall back to ASCII output
            DWORD written;
            WriteConsoleA(hConsole, message.c_str(), static_cast<DWORD>(message.length()), &written, NULL);
        }
    }
    catch (...) {

    }
}

void SafeConsoleOutput(const std::string& message, int color) {
    std::lock_guard<std::mutex> lock(g_consoleMutex);
    try {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole != INVALID_HANDLE_VALUE) {
            // Save original color
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            GetConsoleScreenBufferInfo(hConsole, &csbi);
            WORD originalColor = csbi.wAttributes;
            
            SetConsoleTextAttribute(hConsole, color);
            
            // Ensure input string is not empty
            if (!message.empty()) {
                // Convert UTF-8 string to UTF-16 (Wide Character)
                int wideLength = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, nullptr, 0);
                if (wideLength > 0) {
                    std::vector<wchar_t> wideMessage(wideLength);
                    if (MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, wideMessage.data(), wideLength)) {
                        // Use WriteConsoleW to output Unicode text directly
                        DWORD written;
                        WriteConsoleW(hConsole, wideMessage.data(), static_cast<DWORD>(wideLength - 1), &written, NULL);
                    } else {
                        // If conversion fails, fall back to ASCII output
                        DWORD written;
                        WriteConsoleA(hConsole, message.c_str(), static_cast<DWORD>(message.length()), &written, NULL);
                    }
                }
            }
            
            // Restore original color
            SetConsoleTextAttribute(hConsole, originalColor);
        }
    }
    catch (...) {
        // Ignore console output errors to avoid recursive exceptions
    }
}

// Structured exception handling implementation
std::string GetSEHExceptionName(DWORD exceptionCode) {
    switch (exceptionCode) {
        case EXCEPTION_ACCESS_VIOLATION: return "Access Violation";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "Array Bounds Exceeded";
        case EXCEPTION_BREAKPOINT: return "Breakpoint";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "Data Type Misalignment";
        case EXCEPTION_FLT_DENORMAL_OPERAND: return "Float Denormal Operand";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "Float Divide By Zero";
        case EXCEPTION_FLT_INEXACT_RESULT: return "Float Inexact Result";
        case EXCEPTION_FLT_INVALID_OPERATION: return "Float Invalid Operation";
        case EXCEPTION_FLT_OVERFLOW: return "Float Overflow";
        case EXCEPTION_FLT_STACK_CHECK: return "Float Stack Check";
        case EXCEPTION_FLT_UNDERFLOW: return "Float Underflow";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "Illegal Instruction";
        case EXCEPTION_IN_PAGE_ERROR: return "In Page Error";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "Integer Divide By Zero";
        case EXCEPTION_INT_OVERFLOW: return "Integer Overflow";
        case EXCEPTION_INVALID_DISPOSITION: return "Invalid Disposition";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "Noncontinuable Exception";
        case EXCEPTION_PRIV_INSTRUCTION: return "Privilege Instruction";
        case EXCEPTION_SINGLE_STEP: return "Single Step";
        case EXCEPTION_STACK_OVERFLOW: return "Stack Overflow";
        default: return "Unknown System Exception (0x" + std::to_string(exceptionCode) + ")";
    }
}

void SEHTranslator(unsigned int u, EXCEPTION_POINTERS* pExp) {
    std::string exceptionName = GetSEHExceptionName(u);
    std::stringstream ss;
    ss << "System Exception: " << exceptionName << " (0x" << std::hex << u << ")";
    if (pExp && pExp->ExceptionRecord) {
        ss << " Address: 0x" << std::hex << pExp->ExceptionRecord->ExceptionAddress;
    }
    
    // Try to safely log
    try {
        if (Logger::IsInitialized()) {
            Logger::Fatal(ss.str());
        } else {
            // If logging system not initialized, output directly to console
            SafeConsoleOutput("FATAL: " + ss.str() + "\n");
        }
    } catch (...) {
        // Last resort, direct output
        SafeConsoleOutput("FATAL: " + ss.str() + "\n");
    }
    
    throw std::runtime_error(ss.str());
}

// Safe exit function
void SafeExit(int exitCode) {
    try {
        Logger::Info("Starting cleanup process");
        
        // Set exit flag
        g_shouldExit = true;
        
        // Cleanup hardware monitoring bridge
        try {
            TemperatureWrapper::Cleanup();
            Logger::Debug("Hardware monitoring bridge cleanup complete");
        }
        catch (const std::exception& e) {
            Logger::Error("Error cleaning up hardware monitoring bridge: " + std::string(e.what()));
        }
        
        // Cleanup shared memory
        try {
            SharedMemoryManager::CleanupSharedMemory();
            Logger::Debug("Shared memory cleanup complete");
        }
        catch (const std::exception& e) {
            Logger::Error("Error cleaning up shared memory: " + std::string(e.what()));
        }
        
        // Cleanup COM
        if (g_comInitialized.load()) {
            try {
                CoUninitialize();
                g_comInitialized = false;
                Logger::Debug("COM cleanup complete");
            }
            catch (...) {
                Logger::Error("Unknown error cleaning up COM");
            }
        }
        
        Logger::Info("Cleanup complete, exit code: " + std::to_string(exitCode));
        
        // Give logging system time to complete writes
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    catch (...) {
        // Last exception handling to avoid crashing during cleanup
    }
    
    exit(exitCode);
}

// Helper functions
// Hardware name translation
std::string TranslateHardwareName(const std::string& name) {
    if (name.find("CPU Package") != std::string::npos) return "CPU Temperature";
    if (name.find("GPU Core") != std::string::npos) return "GPU Temperature";
    return name;
}

// Brand detection
std::string GetGpuBrand(const std::wstring& name) {
    if (name.find(L"NVIDIA") != std::wstring::npos) return "NVIDIA";
    if (name.find(L"AMD") != std::wstring::npos) return "AMD";
    if (name.find(L"Intel") != std::wstring::npos) return "Intel";
    return "Unknown";
}

// Network speed unit
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

// Time formatting - enhanced exception handling
std::string FormatDateTime(const std::chrono::system_clock::time_point& tp) {
    try {
        auto time = std::chrono::system_clock::to_time_t(tp);
        struct tm timeinfo;
        if (localtime_s(&timeinfo, &time) == 0) {  // Check return value
            std::stringstream ss;
            ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
            std::string result = ss.str();
            
            // Verify result reasonableness
            if (result.length() >= 19 && result.length() <= 25) {  // Basic length check
                return result;
            } else {
                Logger::Warn("Time format result length abnormal: " + std::to_string(result.length()));
            }
        } else {
            Logger::Error("localtime_s call failed");
        }
    }
    catch (const std::exception& e) {
        Logger::Error("Exception during time formatting: " + std::string(e.what()));
    }
    catch (...) {
        Logger::Error("Exception during time formatting - unknown exception");
    }
    return "Time Formatting Failed";
}

std::string FormatFrequency(double value) {
    try {
        // Parameter validation
        if (std::isnan(value) || std::isinf(value)) {
            Logger::Warn("Invalid frequency value: " + std::to_string(value));
            return "N/A";
        }
        
        if (value < 0) {
            Logger::Warn("Frequency value is negative: " + std::to_string(value));
            return "N/A";
        }
        
        // Reasonableness check - frequency typically not exceeding 10GHz
        if (value > 10000) {
            Logger::Warn("Frequency value abnormal: " + std::to_string(value) + "MHz");
            return "Abnormal Value";
        }
        
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
    catch (const std::exception& e) {
        Logger::Error("Exception during frequency formatting: " + std::string(e.what()));
        return "Formatting Failed";
    }
    catch (...) {
        Logger::Error("Exception during frequency formatting - unknown exception");
        return "Formatting Failed";
    }
}

std::string FormatPercentage(double value) {
    try {
        // Parameter validation
        if (std::isnan(value) || std::isinf(value)) {
            Logger::Warn("Percentage value invalid: " + std::to_string(value));
            return "N/A";
        }
        
        // Reasonableness check - percentage typically between 0-100
        if (value < -1.0 || value > 105.0) {
            Logger::Warn("Percentage value abnormal: " + std::to_string(value));
        }
        
        // Limit to reasonable range
        if (value < 0) value = 0;
        if (value > 100) value = 100;
        
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << value << "%";
        return ss.str();
    }
    catch (const std::exception& e) {
        Logger::Error("Exception during percentage formatting: " + std::string(e.what()));
        return "Formatting Failed";
    }
    catch (...) {
        Logger::Error("Exception during percentage formatting - unknown exception");
        return "Formatting Failed";
    }
}

std::string FormatTemperature(double value) {
    try {
        // Parameter validation
        if (std::isnan(value) || std::isinf(value)) {
            Logger::Warn("Temperature value invalid: " + std::to_string(value));
            return "N/A";
        }
        
        // Reasonableness check - temperature typically between -50C to 150C
        if (value < -50.0 || value > 150.0) {
            Logger::Warn("Temperature value abnormal: " + std::to_string(value) + "°C");
            if (value < -50.0) return "Too Low";
            if (value > 150.0) return "Too High";
        }
        
        std::stringstream ss;
        ss << static_cast<int>(value) << "°C";  // Display integer temperature
        return ss.str();
    }
    catch (const std::exception& e) {
        Logger::Error("Exception during temperature formatting: " + std::string(e.what()));
        return "Formatting Failed";
    }
    catch (...) {
        Logger::Error("Exception during temperature formatting - unknown exception");
        return "Formatting Failed";
    }
}

std::string FormatSize(uint64_t bytes, bool useBinary = true) {
    try {
        const double kb = useBinary ? 1024.0 : 1000.0;
        const double mb = kb * kb;
        const double gb = mb * kb;
        const double tb = gb * kb;

        // Parameter validation - check if at maximum value (usually indicates error)
        if (bytes == UINT64_MAX) {
            Logger::Warn("Bytes at maximum value, may indicate error");
            return "N/A";
        }

        std::stringstream ss;
        ss << std::fixed << std::setprecision(1);

        if (bytes >= tb) ss << (bytes / tb) << " TB";
        else if (bytes >= gb) ss << (bytes / gb) << " GB";
        else if (bytes >= mb) ss << (bytes / mb) << " MB";
        else if (bytes >= kb) ss << (bytes / kb) << " KB";
        else ss << bytes << " B";

        return ss.str();
    }
    catch (const std::exception& e) {
        Logger::Error("Exception during size formatting: " + std::string(e.what()));
        return "Formatting Failed";
    }
    catch (...) {
        Logger::Error("Exception during size formatting - unknown exception");
        return "Formatting Failed";
    }
}

std::string FormatDiskUsage(uint64_t used, uint64_t total) {
    if (total == 0) return "0%";
    double percentage = (static_cast<double>(used) / total) * 100.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << percentage << "%";
    return ss.str();
}

static void PrintSectionHeader(const std::string& title) {
    SafeConsoleOutput("\n=== " + title + " ===\n", 14); // Yellow
}

static void PrintInfoItem(const std::string& label, const std::string& value, int indent = 2) {
    std::string line = std::string(indent, ' ') + label;
    // Format to fixed width
    if (line.length() < 27) {
        line += std::string(27 - line.length(), ' ');
    }
    line += ": " + value + "\n";
    SafeConsoleOutput(line);
}

// Main function
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

// Thread-safe GPU info cache class
class ThreadSafeGpuCache {
private:
    mutable std::mutex mtx_;
    bool initialized_ = false;
    std::string cachedGpuName_ = "No GPU detected";
    std::string cachedGpuBrand_ = "Unknown";
    uint64_t cachedGpuMemory_ = 0;
    uint32_t cachedGpuCoreFreq_ = 0;
    bool cachedGpuIsVirtual_ = false;
    double cachedGpuUsage_ = 0.0;
    std::unique_ptr<GpuInfo> gpuInfo_;
    size_t selectedGpuIndex_ = 0;

public:
    void Initialize(WmiManager& wmiManager) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (initialized_) return;
        
        try {
            Logger::Info("Initializing GPU info");

            gpuInfo_ = std::make_unique<GpuInfo>(wmiManager);
            const auto& gpus = gpuInfo_->GetGpuData();

            // Record all detected GPUs
            for (const auto& gpu : gpus) {
                std::string gpuName = WinUtils::WstringToString(gpu.name);
                Logger::Info("Detected GPU: " + gpuName +
                           " (Virtual: " + (gpu.isVirtual ? "Yes" : "No") +
                           ", NVIDIA: " + (gpuName.find("NVIDIA") != std::string::npos ? "Yes" : "No") +
                           ", Integrated: " + (gpuName.find("Intel") != std::string::npos ||
                                       gpuName.find("AMD") != std::string::npos ? "Yes" : "No") + ")");
            }

            const GpuInfo::GpuData* selectedGpu = nullptr;
            selectedGpuIndex_ = 0;
            for (size_t i = 0; i < gpus.size(); ++i) {
                if (!gpus[i].isVirtual) {
                    selectedGpu = &gpus[i];
                    selectedGpuIndex_ = i;
                    break;
                }
            }

            if (!selectedGpu && !gpus.empty()) {
                selectedGpu = &gpus[0];
                selectedGpuIndex_ = 0;
            }

            if (selectedGpu) {
                cachedGpuName_ = WinUtils::WstringToString(selectedGpu->name);
                cachedGpuBrand_ = GetGpuBrand(selectedGpu->name);
                cachedGpuMemory_ = selectedGpu->dedicatedMemory;
                cachedGpuCoreFreq_ = static_cast<uint32_t>(selectedGpu->coreClock);
                cachedGpuIsVirtual_ = selectedGpu->isVirtual;
                cachedGpuUsage_ = selectedGpu->usage;  // Save GPU usage

                Logger::Info("Selected primary GPU: " + cachedGpuName_ +
                           " (Virtual: " + (cachedGpuIsVirtual_ ? "Yes" : "No") + ")");
            } else {
                Logger::Warn("No GPU detected");
            }

            initialized_ = true;
        }
        catch (const std::exception& e) {
            Logger::Error("GPU info initialization failed: " + std::string(e.what()));
            initialized_ = true;
        }
    }
    
    void GetCachedInfo(std::string& name, std::string& brand, uint64_t& memory, 
                       uint32_t& coreFreq, bool& isVirtual, double& usage) const {
        std::lock_guard<std::mutex> lock(mtx_);
        name = cachedGpuName_;
        brand = cachedGpuBrand_;
        memory = cachedGpuMemory_;
        coreFreq = cachedGpuCoreFreq_;
        isVirtual = cachedGpuIsVirtual_;
        usage = cachedGpuUsage_;
    }

    void RefreshUsage() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_ || !gpuInfo_) return;
        gpuInfo_->RefreshUsage();
        const auto& gpus = gpuInfo_->GetGpuData();
        if (selectedGpuIndex_ < gpus.size()) {
            cachedGpuUsage_ = gpus[selectedGpuIndex_].usage;
        }
    }

    bool IsInitialized() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return initialized_;
    }
};

// ======================== JSON Output Helpers ========================

static std::string GetIsoTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    gmtime_s(&tm, &time);
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

static nlohmann::json SystemInfoToJson(const SystemInfo& sysInfo) {
    nlohmann::json j;

    j["cpu"]["name"] = sysInfo.cpuName;
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
        j["gpu"]["name"] = WinUtils::WstringToString(std::wstring(gpu.name));
        j["gpu"]["brand"] = WinUtils::WstringToString(std::wstring(gpu.brand));
        j["gpu"]["usage"] = gpu.usage;
        j["gpu"]["memory"] = gpu.memory;
        j["gpu"]["coreClock"] = gpu.coreClock;
    }
    j["gpu"]["temperature"] = sysInfo.gpuTemperature;

    auto& netArr = j["network"] = nlohmann::json::array();
    for (const auto& a : sysInfo.adapters) {
        nlohmann::json net;
        net["name"] = WinUtils::WstringToString(std::wstring(a.name));
        net["mac"] = WinUtils::WstringToString(std::wstring(a.mac));
        net["ip"] = WinUtils::WstringToString(std::wstring(a.ipAddress));
        net["type"] = WinUtils::WstringToString(std::wstring(a.adapterType));
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

static SystemInfo CollectSystemInfo(WmiManager& wmiManager) {
    SystemInfo sysInfo = {};

    try {
        CpuInfo cpuInfo;
        sysInfo.cpuName = cpuInfo.GetName();
        sysInfo.physicalCores = cpuInfo.GetLargeCores() + cpuInfo.GetSmallCores();
        sysInfo.logicalCores = cpuInfo.GetTotalCores();
        sysInfo.performanceCores = cpuInfo.GetLargeCores();
        sysInfo.efficiencyCores = cpuInfo.GetSmallCores();
        sysInfo.hyperThreading = cpuInfo.IsHyperThreadingEnabled();
        sysInfo.virtualization = cpuInfo.IsVirtualizationEnabled();
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
    } catch (const std::exception& e) {
        Logger::Error("CollectSystemInfo Memory: " + std::string(e.what()));
    }

    try {
        GpuInfo gpuInfo(wmiManager);
        const auto& gpus = gpuInfo.GetGpuData();
        for (size_t i = 0; i < gpus.size(); ++i) {
            GPUData gpuData = {};
            std::wstring nameW(gpus[i].name);
            wcsncpy_s(gpuData.name, nameW.c_str(), 128);
            std::wstring brandW = WinUtils::StringToWstring(GetGpuBrand(gpus[i].name));
            wcsncpy_s(gpuData.brand, brandW.c_str(), 64);
            gpuData.memory = gpus[i].dedicatedMemory;
            gpuData.coreClock = static_cast<double>(gpus[i].coreClock);
            gpuData.isVirtual = gpus[i].isVirtual;
            gpuData.usage = gpus[i].usage;
            sysInfo.gpus.push_back(gpuData);
        }
    } catch (const std::exception& e) {
        Logger::Error("CollectSystemInfo GPU: " + std::string(e.what()));
    }

    try {
        NetworkAdapter netAdapter(wmiManager);
        const auto& adapters = netAdapter.GetAdapters();
        for (const auto& a : adapters) {
            NetworkAdapterData data = {};
            std::wstring nameW = WinUtils::StringToWstring(a.name);
            std::wstring macW = WinUtils::StringToWstring(a.mac);
            std::wstring ipW = WinUtils::StringToWstring(a.ip);
            std::wstring typeW = WinUtils::StringToWstring(a.adapterType);
            wcsncpy_s(data.name, nameW.c_str(), 128);
            wcsncpy_s(data.mac, macW.c_str(), 32);
            wcsncpy_s(data.ipAddress, ipW.c_str(), 64);
            wcsncpy_s(data.adapterType, typeW.c_str(), 32);
            data.speed = a.speed;
            sysInfo.adapters.push_back(data);
        }
    } catch (const std::exception& e) {
        Logger::Error("CollectSystemInfo Network: " + std::string(e.what()));
    }

    try {
        auto temperatures = TemperatureWrapper::GetTemperatures();
        sysInfo.temperatures = temperatures;
        for (const auto& temp : temperatures) {
            std::string nameLower = temp.first;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            if (nameLower.find("gpu") != std::string::npos || nameLower.find("graphics") != std::string::npos) {
                sysInfo.gpuTemperature = temp.second;
            } else if (nameLower.find("cpu") != std::string::npos || nameLower.find("package") != std::string::npos) {
                sysInfo.cpuTemperature = temp.second;
            }
        }
    } catch (const std::exception& e) {
        Logger::Error("CollectSystemInfo Temperature: " + std::string(e.what()));
    }

    try {
        DiskInfo diskInfo;
        auto disks = diskInfo.GetDisks();
        if (disks.size() <= 8) {
            sysInfo.disks = disks;
        }
    } catch (const std::exception& e) {
        Logger::Error("CollectSystemInfo Disk: " + std::string(e.what()));
    }

    sysInfo.lastUpdate = Platform::SystemTime::Now();
    return sysInfo;
}

int main(int argc, char* argv[]) {
    _set_se_translator(SEHTranslator);
    
    std::set_new_handler([]() {
        Logger::Fatal("Memory allocation failed - system out of memory");
        throw std::bad_alloc();
    });
    
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
    
    setlocale(LC_ALL, "en_US.UTF-8");
    
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    
    try {
        try {
            Logger::EnableConsoleOutput(true);
            Logger::Initialize("system_monitor.log");
            Logger::SetLogLevel(LOG_INFO);
            Logger::Info("Program started");
#ifdef TCMT_BUILD_TIMESTAMP
            Logger::Info("[DIAG] Logger OK; version=" + std::string(TCMT_VERSION) + " built=" + std::string(TCMT_BUILD_TIMESTAMP));
#else
            Logger::Info("[DIAG] Logger OK; version=" + std::string(TCMT_VERSION) + " built=" __DATE__ " " __TIME__);
#endif
        }
        catch (const std::exception& e) {
            printf("Logging system initialization failed: %s\n", e.what());
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
            SafeExit(0);
        }

        if (jsonMode) {
            Logger::Info("--json mode: collecting one round of data...");

            // Skip admin elevation for --json mode.
            // Init COM (needed for WMI).
            HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (FAILED(hr)) {
                if (hr == RPC_E_CHANGED_MODE) {
                    Logger::Warn("COM init mode conflict, trying single-threaded");
                    hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                }
                if (FAILED(hr)) {
                    Logger::Error("COM init failed for --json mode");
                    return -1;
                }
            }
            g_comInitialized = true;

            // Create WMI manager
            std::unique_ptr<WmiManager> wmiManager;
            try {
                wmiManager = std::make_unique<WmiManager>();
                if (!wmiManager->IsInitialized()) {
                    Logger::Error("WMI init failed for --json mode");
                    SafeExit(1);
                }
            } catch (const std::exception& e) {
                Logger::Error("WMI creation failed: " + std::string(e.what()));
                SafeExit(1);
            }

            // Init temperature bridge
            try {
                TemperatureWrapper::Initialize();
            } catch (...) {
                Logger::Warn("TemperatureWrapper init failed for --json mode");
            }

            // Collect one round of data
            SystemInfo sysInfo = CollectSystemInfo(*wmiManager);

            // Serialize and print JSON to stdout
            nlohmann::json j = SystemInfoToJson(sysInfo);
            std::cout << j.dump(2) << std::endl;

            Logger::Info("--json mode complete");
            exit(0);
        }

        if (!IsRunAsAdmin()) {
            wchar_t szPath[MAX_PATH];
            GetModuleFileNameW(NULL, szPath, MAX_PATH);

            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.lpVerb = L"runas";
            sei.lpFile = szPath;
            sei.hwnd = NULL;
            sei.nShow = SW_NORMAL;

            if (ShellExecuteExW(&sei)) {
                exit(0);
            } else {
                MessageBoxW(NULL, L"Auto elevation failed, please right-click and run as administrator.", L"Insufficient Privileges", MB_OK | MB_ICONERROR);
                SafeExit(1);
            }
        }

        try {
            HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (FAILED(hr)) {
                if (hr == RPC_E_CHANGED_MODE) {
                    Logger::Warn("COM initialization mode conflict: thread already initialized in different mode, trying single-threaded mode");
                    hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                    if (FAILED(hr)) {
                        Logger::Error("COM initialization failed: 0x" + std::to_string(hr));
                        return -1;
                    }
                }
                else {
                    Logger::Error("COM initialization failed: 0x" + std::to_string(hr));
                    return -1;
                }
            }
            g_comInitialized = true;
            Logger::Info("[DIAG] COM init OK");
        }
        catch (const std::exception& e) {
            Logger::Fatal("[DIAG] COM init exception: " + std::string(e.what()));
            return -1;
        }

        try {
            if (!SharedMemoryManager::InitSharedMemory()) {
                std::string error = SharedMemoryManager::GetLastError();
                Logger::Error("Shared memory initialization failed: " + error);
                
                Logger::Info("Attempting to reinitialize shared memory...");
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                
                if (!SharedMemoryManager::InitSharedMemory()) {
                    Logger::Critical("Shared memory reinitialization failed, program cannot continue");
                    SafeExit(1);
                }
            }
            Logger::Info("Shared memory initialized successfully");
            Logger::Info("[DIAG] SHM init OK, sizeof(SharedMemoryBlock)=" + std::to_string(sizeof(SharedMemoryBlock)));
        }
        catch (const std::exception& e) {
            Logger::Fatal("[DIAG] SHM init exception: " + std::string(e.what()));
            SafeExit(1);
        }

        // Create and initialize WMI manager - enhanced memory allocation exception handling
        std::unique_ptr<WmiManager> wmiManager;
        try {
            wmiManager = std::make_unique<WmiManager>();
            if (!wmiManager) {
                Logger::Fatal("WMI manager object creation failed - memory allocation returned null");
                SafeExit(1);
            }
            if (!wmiManager->IsInitialized()) {
                Logger::Error("WMI initialization failed");
                MessageBoxA(NULL, "WMI initialization failed, cannot retrieve system information.", "Error", MB_OK | MB_ICONERROR);
                SafeExit(1);
            }
            Logger::Info("[DIAG] WMI init OK");
        }
        catch (const std::bad_alloc& e) {
            Logger::Fatal("[DIAG] WMI bad_alloc: " + std::string(e.what()));
            SafeExit(1);
        }
        catch (const std::exception& e) {
            Logger::Fatal("[DIAG] WMI exception: " + std::string(e.what()));
            SafeExit(1);
        }
        catch (...) {
            Logger::Fatal("[DIAG] WMI unknown exception");
            SafeExit(1);
        }

        // Initialize hardware monitoring bridge
        try {
            TemperatureWrapper::Initialize();
            Logger::Debug("Hardware monitoring bridge initialized successfully");
        }
        catch (const std::exception& e) {
            Logger::Error("Hardware monitoring bridge initialization failed: " + std::string(e.what()));
            // Do not exit program, continue running but temperature data may not be available
        }

        Logger::Info("Program startup complete");
        Logger::Info("[DIAG] Entering main monitoring loop...");

        // Start NamedPipe server for IPC schema broadcast
        tcmt::ipc::NamedPipeServer pipeServer;
        if (pipeServer.Start()) {
            Logger::Info("NamedPipe server started");
        } else {
            Logger::Warn("NamedPipe server failed: " + pipeServer.LastError());
        }

        // Start TUI (Windows version)
        tcmt::TuiApp tuiApp;
        tuiApp.SetLogBuffer(&Logger::GetTuiBuffer());
        tuiApp.Start();
        Logger::Info("TUI started");

        // Register schema fields for NamedPipe IPC
        using namespace tcmt::ipc;
        SchemaHeader ipcHdr;
        ipcHdr.magic = IPC_MAGIC;
        ipcHdr.version = IPC_VERSION;
        ipcHdr.totalSize = sizeof(SharedMemoryBlock);
        ipcHdr.stringBlockSize = 0;

        std::vector<FieldDef> ipcFields;
        auto addField = [&](const char* name, FieldType type, size_t offset, size_t size,
                            const char* units = "", float min = 0, float max = 0) {
            if (ipcFields.size() >= IPC_MAX_FIELDS) return;
            FieldDef f;
            f.id = (uint32_t)ipcFields.size() + 1;
            f.type = static_cast<uint8_t>(type);
            f.offset = (uint32_t)offset;
            f.size = (uint16_t)size;
            f.count = 1;
            f.strOffset = 0;
            f.flags = 0;
            f.minVal = min;
            f.maxVal = max;
            strncpy_s(f.name, sizeof(f.name), name, _TRUNCATE);
            strncpy_s(f.units, sizeof(f.units), units, _TRUNCATE);
            ipcFields.push_back(f);
        };

        // ─── CPU ───
        addField("cpu/name",              FieldType::WString, offsetof(SharedMemoryBlock, cpuName),          128);
        addField("cpu/cores/physical",    FieldType::UInt32,  offsetof(SharedMemoryBlock, physicalCores),    4);
        addField("cpu/cores/logical",     FieldType::UInt32,  offsetof(SharedMemoryBlock, logicalCores),     4);
        addField("cpu/cores/performance", FieldType::UInt32,  offsetof(SharedMemoryBlock, performanceCores), 4);
        addField("cpu/cores/efficiency",  FieldType::UInt32,  offsetof(SharedMemoryBlock, efficiencyCores),  4);
        addField("cpu/usage",             FieldType::Float64, offsetof(SharedMemoryBlock, cpuUsage),         8, "%", 0, 100);
        addField("cpu/freq/pCore",        FieldType::Float64, offsetof(SharedMemoryBlock, pCoreFreq),        8, "MHz");
        addField("cpu/freq/eCore",        FieldType::Float64, offsetof(SharedMemoryBlock, eCoreFreq),        8, "MHz");
        addField("cpu/temperature",       FieldType::Float64, offsetof(SharedMemoryBlock, cpuTemperature),   8, "C", -50, 150);
        addField("cpu/hyperThreading",    FieldType::Bool,    offsetof(SharedMemoryBlock, hyperThreading),   1);
        addField("cpu/virtualization",    FieldType::Bool,    offsetof(SharedMemoryBlock, virtualization),   1);

        // ─── Memory ───
        addField("memory/total",         FieldType::UInt64, offsetof(SharedMemoryBlock, totalMemory),      8, "B");
        addField("memory/used",          FieldType::UInt64, offsetof(SharedMemoryBlock, usedMemory),       8, "B");
        addField("memory/available",     FieldType::UInt64, offsetof(SharedMemoryBlock, availableMemory),  8, "B");
        addField("memory/compressed",    FieldType::UInt64, offsetof(SharedMemoryBlock, compressedMemory), 8, "B");

        // --- GPU array ---
        {
            constexpr int maxGpus = sizeof(SharedMemoryBlock::gpus) / sizeof(GPUData);
            char name[64];
            for (int i = 0; i < maxGpus; i++) {
                size_t base = offsetof(SharedMemoryBlock, gpus) + i * sizeof(GPUData);
                snprintf(name, sizeof(name), "gpu/%d/name", i);
                addField(name, FieldType::WString, base + offsetof(GPUData, name),     128);
                snprintf(name, sizeof(name), "gpu/%d/brand", i);
                addField(name, FieldType::WString, base + offsetof(GPUData, brand),    64);
                snprintf(name, sizeof(name), "gpu/%d/memory", i);
                addField(name, FieldType::UInt64,  base + offsetof(GPUData, memory),   8, "B");
                snprintf(name, sizeof(name), "gpu/%d/usage", i);
                addField(name, FieldType::Float64, base + offsetof(GPUData, usage),    8, "%", 0, 100);
                snprintf(name, sizeof(name), "gpu/%d/coreFreq", i);
                addField(name, FieldType::Float64, base + offsetof(GPUData, coreClock),8, "MHz");
                snprintf(name, sizeof(name), "gpu/%d/isVirtual", i);
                addField(name, FieldType::Bool,    base + offsetof(GPUData, isVirtual),1);
            }
        }
        addField("gpu/0/temperature", FieldType::Float64, offsetof(SharedMemoryBlock, gpuTemperature), 8, "C");

        // --- Network array (capped at 2 to stay within IPC_MAX_FIELDS) ---
        {
            constexpr int maxAdapters = (std::min)(
                (int)(sizeof(SharedMemoryBlock::adapters) / sizeof(NetworkAdapterData)), 2);
            char name[64];
            for (int i = 0; i < maxAdapters; i++) {
                size_t base = offsetof(SharedMemoryBlock, adapters) + i * sizeof(NetworkAdapterData);
                snprintf(name, sizeof(name), "net/%d/name", i);
                addField(name, FieldType::WString, base + offsetof(NetworkAdapterData, name),        128);
                snprintf(name, sizeof(name), "net/%d/mac", i);
                addField(name, FieldType::WString, base + offsetof(NetworkAdapterData, mac),         32);
                snprintf(name, sizeof(name), "net/%d/ip", i);
                addField(name, FieldType::WString, base + offsetof(NetworkAdapterData, ipAddress),   64);
                snprintf(name, sizeof(name), "net/%d/type", i);
                addField(name, FieldType::WString, base + offsetof(NetworkAdapterData, adapterType), 32);
                snprintf(name, sizeof(name), "net/%d/speed", i);
                addField(name, FieldType::UInt64,  base + offsetof(NetworkAdapterData, speed),       8, "bps");
            }
        }

        // --- Disk array (capped at 4 to stay within IPC_MAX_FIELDS) ---
        {
            constexpr int maxDisks = (std::min)(
                (int)(sizeof(SharedMemoryBlock::disks) / sizeof(SharedMemoryBlock::SharedDiskData)), 4);
            char name[64];
            for (int i = 0; i < maxDisks; i++) {
                size_t base = offsetof(SharedMemoryBlock, disks) + i * sizeof(SharedMemoryBlock::SharedDiskData);
                snprintf(name, sizeof(name), "disk/%d/letter", i);
                addField(name, FieldType::String,  base + offsetof(SharedMemoryBlock::SharedDiskData, letter),    1);
                snprintf(name, sizeof(name), "disk/%d/label", i);
                addField(name, FieldType::WString, base + offsetof(SharedMemoryBlock::SharedDiskData, label),     128);
                snprintf(name, sizeof(name), "disk/%d/total", i);
                addField(name, FieldType::UInt64,  base + offsetof(SharedMemoryBlock::SharedDiskData, totalSize), 8, "B");
                snprintf(name, sizeof(name), "disk/%d/used", i);
                addField(name, FieldType::UInt64,  base + offsetof(SharedMemoryBlock::SharedDiskData, usedSpace), 8, "B");
                snprintf(name, sizeof(name), "disk/%d/free", i);
                addField(name, FieldType::UInt64,  base + offsetof(SharedMemoryBlock::SharedDiskData, freeSpace), 8, "B");
            }
        }

        ipcHdr.fieldCount = (uint16_t)ipcFields.size();
        pipeServer.UpdateSchema(ipcHdr, ipcFields);
        Logger::Debug("IPC schema broadcast: " + std::to_string(ipcFields.size()) + " fields");

        int loopCounter = 1;
        bool isFirstRun = true;
        
        // Cache static system info (only on first fetch)
        static std::atomic<bool> systemInfoCached{false};
        static std::string cachedOsVersion;
        static std::string cachedCpuName;
        static uint32_t cachedPhysicalCores = 0;
        static uint32_t cachedLogicalCores = 0;
        static uint32_t cachedPerformanceCores = 0;
        static uint32_t cachedEfficiencyCores = 0;
        static bool cachedHyperThreading = false;
        static bool cachedVirtualization = false;
        
        std::unique_ptr<CpuInfo> cpuInfo;
        try {
            cpuInfo = std::make_unique<CpuInfo>();
            if (!cpuInfo) {
                Logger::Fatal("CPU info object creation failed - memory allocation returned null");
                SafeExit(1);
            }
            Logger::Debug("CPU info object created successfully");
        }
        catch (const std::bad_alloc& e) {
            Logger::Fatal("CPU info object creation failed - memory allocation failed: " + std::string(e.what()));
            SafeExit(1);
        }
        catch (const std::exception& e) {
            Logger::Error("CPU info object creation failed: " + std::string(e.what()));
            SafeExit(1);
        }
        catch (...) {
            Logger::Fatal("CPU info object creation failed - unknown exception");
            SafeExit(1);
        }
        
        ThreadSafeGpuCache gpuCache;
        
        while (!g_shouldExit.load()) {
            try {
                auto loopStart = std::chrono::high_resolution_clock::now();
                
                bool isDetailedLogging = (loopCounter % 5 == 1);
                
                if (isDetailedLogging) {
                    Logger::Debug("Starting main monitoring loop iteration #" + std::to_string(loopCounter));
                }
                if (loopCounter == 1) {
                    Logger::Info("[DIAG] First loop iteration started");
                }
                
                if (loopCounter == 5) {
                    g_monitoringStarted = true;
                    Logger::Info("Program is running stable");
                }
                
                SystemInfo sysInfo;

                try {
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
                    sysInfo.lastUpdate = Platform::SystemTime::Now();
                    
                    if (sysInfo.lastUpdate.year < 2020 || sysInfo.lastUpdate.year > 2050) {
                        Logger::Warn("Abnormal system time: " + std::to_string(sysInfo.lastUpdate.year));
                    }
                }
                catch (const std::exception& e) {
                    Logger::Error("SystemInfo initialization failed: " + std::string(e.what()));
                    continue;
                }
                catch (...) {
                    Logger::Error("SystemInfo initialization failed - unknown exception");
                    continue;
                }

                if (!systemInfoCached.load()) {
                    try {
                        Logger::Info("[DIAG] Initializing system information...");
                        
                        OSInfo os;
                        cachedOsVersion = os.GetVersion();

                        if (cpuInfo) {
                            cachedCpuName = cpuInfo->GetName();
                            cachedPhysicalCores = cpuInfo->GetLargeCores() + cpuInfo->GetSmallCores();
                            cachedLogicalCores = cpuInfo->GetTotalCores();
                            cachedPerformanceCores = cpuInfo->GetLargeCores();
                            cachedEfficiencyCores = cpuInfo->GetSmallCores();
                            cachedHyperThreading = cpuInfo->IsHyperThreadingEnabled();
                            cachedVirtualization = cpuInfo->IsVirtualizationEnabled();
                        }
                        
                        systemInfoCached = true;
                        Logger::Info("[DIAG] System info: cpu=" + cachedCpuName +
                            " cores=" + std::to_string(cachedPhysicalCores) +
                            " mem_total=" + std::to_string(sysInfo.totalMemory));
                    }
                    catch (const std::exception& e) {
                        Logger::Error("[DIAG] System info init failed: " + std::string(e.what()));
                        cachedOsVersion = "Unknown";
                        cachedCpuName = "Unknown";
                        systemInfoCached = true;
                    }
                }
                
                sysInfo.osVersion = cachedOsVersion;
                sysInfo.cpuName = cachedCpuName;
                sysInfo.physicalCores = cachedPhysicalCores;
                sysInfo.logicalCores = cachedLogicalCores;
                sysInfo.performanceCores = cachedPerformanceCores;
                sysInfo.efficiencyCores = cachedEfficiencyCores;
                sysInfo.hyperThreading = cachedHyperThreading;
                sysInfo.virtualization = cachedVirtualization;

                try {
                    if (cpuInfo) {
                        sysInfo.cpuUsage = cpuInfo->GetUsage();
                        sysInfo.performanceCoreFreq = cpuInfo->GetLargeCoreSpeed();
                        sysInfo.efficiencyCoreFreq = cpuInfo->GetSmallCoreSpeed();
                        sysInfo.cpuUsageSampleIntervalMs = cpuInfo->GetLastSampleIntervalMs();
                    }
                }
                catch (const std::exception& e) {
                    Logger::Error("Failed to get CPU dynamic info: " + std::string(e.what()));
                }

                try {
                    MemoryInfo mem;
                    sysInfo.totalMemory = mem.GetTotalPhysical();
                    sysInfo.usedMemory = mem.GetTotalPhysical() - mem.GetAvailablePhysical();
                    sysInfo.availableMemory = mem.GetAvailablePhysical();

                    // Compressed memory via WMI (Windows 10+)
                    if (wmiManager) {
                        IWbemServices* wmiSvc = wmiManager->GetWmiService();
                        if (wmiSvc) {
                            IEnumWbemClassObject* pEnumMem = nullptr;
                            if (SUCCEEDED(wmiSvc->ExecQuery(
                                bstr_t(L"WQL"),
                                bstr_t(L"SELECT CompressedMemory FROM Win32_PerfFormattedData_PerfOS_Memory"),
                                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                nullptr, &pEnumMem)) && pEnumMem)
                            {
                                IWbemClassObject* objMem = nullptr;
                                ULONG retMem = 0;
                                if (pEnumMem->Next(WBEM_INFINITE, 1, &objMem, &retMem) == S_OK) {
                                    VARIANT vtMem;
                                    VariantInit(&vtMem);
                                    if (SUCCEEDED(objMem->Get(L"CompressedMemory", 0, &vtMem, 0, 0))) {
                                        if (vtMem.vt == VT_UI8)
                                            sysInfo.compressedMemory = vtMem.ullVal * 1024;
                                        else if (vtMem.vt == VT_I4 || vtMem.vt == VT_UI4)
                                            sysInfo.compressedMemory = static_cast<uint64_t>((vtMem.vt == VT_I4) ? vtMem.lVal : vtMem.ulVal) * 1024;
                                    }
                                    VariantClear(&vtMem);
                                    objMem->Release();
                                }
                                pEnumMem->Release();
                            }
                        }
                    }
                }
                catch (const std::exception& e) {
                    Logger::Error("Failed to get memory info: " + std::string(e.what()));
                }

                if (!gpuCache.IsInitialized()) {
                    try {
                        gpuCache.Initialize(*wmiManager);
                    }
                    catch (const std::exception& e) {
                        Logger::Error("GPU cache initialization failed: " + std::string(e.what()));
                    }
                }

                gpuCache.RefreshUsage();

                try {
                    std::string cachedGpuName, cachedGpuBrand;
                    uint64_t cachedGpuMemory;
                    uint32_t cachedGpuCoreFreq;
                    bool cachedGpuIsVirtual;
                    double cachedGpuUsage;
                    
                    gpuCache.GetCachedInfo(cachedGpuName, cachedGpuBrand, cachedGpuMemory, 
                                          cachedGpuCoreFreq, cachedGpuIsVirtual, cachedGpuUsage);
                    
                    sysInfo.gpuName = cachedGpuName;
                    sysInfo.gpuBrand = cachedGpuBrand;
                    sysInfo.gpuMemory = cachedGpuMemory;
                    sysInfo.gpuCoreFreq = cachedGpuCoreFreq;
                    sysInfo.gpuIsVirtual = cachedGpuIsVirtual;
                    sysInfo.gpuUsage = cachedGpuUsage;

                    // Fix GPU array population - add data validation and cleanup
                    sysInfo.gpus.clear();
                    if (!cachedGpuName.empty() && cachedGpuName != "No GPU detected") {
                        GPUData gpu;
                        
                        // Initialize GPU struct to avoid garbage data
                        memset(&gpu, 0, sizeof(GPUData));
                        
                        // Safely copy GPU name and brand to wchar_t arrays
                        std::wstring gpuNameW = WinUtils::StringToWstring(cachedGpuName);
                        std::wstring gpuBrandW = WinUtils::StringToWstring(cachedGpuBrand);
                        
                        // Limit string length to prevent buffer overflow
                        if (gpuNameW.length() >= sizeof(gpu.name)/sizeof(wchar_t)) {
                            gpuNameW = gpuNameW.substr(0, sizeof(gpu.name)/sizeof(wchar_t) - 1);
                        }
                        if (gpuBrandW.length() >= sizeof(gpu.brand)/sizeof(wchar_t)) {
                            gpuBrandW = gpuBrandW.substr(0, sizeof(gpu.brand)/sizeof(wchar_t) - 1);
                        }
                        
                        wcsncpy_s(gpu.name, sizeof(gpu.name)/sizeof(wchar_t), gpuNameW.c_str(), _TRUNCATE);
                        wcsncpy_s(gpu.brand, sizeof(gpu.brand)/sizeof(wchar_t), gpuBrandW.c_str(), _TRUNCATE);
                        
                        // Validate and clean GPU data - avoid abnormal values
                        gpu.memory = (cachedGpuMemory > 0 && cachedGpuMemory < UINT64_MAX) ? cachedGpuMemory : 0;
                        
                        // Fix GPU core clock - ensure it's in reasonable range
                        if (cachedGpuCoreFreq > 0 && cachedGpuCoreFreq < 10000) {
                            gpu.coreClock = cachedGpuCoreFreq;
                        } else {
                            gpu.coreClock = 0; // Set to 0 instead of abnormal value
                            if (isFirstRun && cachedGpuCoreFreq > 10000) {
                                Logger::Warn("GPU core clock abnormal: " + std::to_string(cachedGpuCoreFreq) + "MHz, reset to 0");
                            }
                        }
                        
                        gpu.isVirtual = cachedGpuIsVirtual;
                        gpu.usage = cachedGpuUsage;  // GPU usage
                        
                        sysInfo.gpus.push_back(gpu);
                        
                        if (isFirstRun) {
                            Logger::Debug("Added GPU to array: " + cachedGpuName + 
                                         " (Memory: " + FormatSize(cachedGpuMemory) + 
                                         ", Clock: " + std::to_string(gpu.coreClock) + "MHz" +
                                         ", Virtual: " + (cachedGpuIsVirtual ? "Yes" : "No") + ")");
                        }
                    } else {
                        if (isFirstRun) {
                            Logger::Debug("No valid GPU detected, skipping GPU data population");
                        }
                    }
                }
                catch (const std::bad_alloc& e) {
                    Logger::Error("GPU cache info processing failed - Out of memory: " + std::string(e.what()));
                    sysInfo.gpus.clear();
                    sysInfo.gpuName = "Out of memory";
                    sysInfo.gpuBrand = "Unknown";
                    sysInfo.gpuMemory = 0;
                    sysInfo.gpuCoreFreq = 0;
                    sysInfo.gpuIsVirtual = false;
                }
                catch (const std::exception& e) {
                    Logger::Error("Failed to get GPU cache info: " + std::string(e.what()));
                    sysInfo.gpus.clear();
                    sysInfo.gpuName = "Failed to get GPU info";
                    sysInfo.gpuBrand = "Unknown";
                    sysInfo.gpuMemory = 0;
                    sysInfo.gpuCoreFreq = 0;
                    sysInfo.gpuIsVirtual = false;
                }
                catch (...) {
                    Logger::Error("Failed to get GPU cache info - Unknown exception");
                    sysInfo.gpus.clear();
                    sysInfo.gpuName = "Unknown exception";
                    sysInfo.gpuBrand = "Unknown";
                    sysInfo.gpuMemory = 0;
                    sysInfo.gpuCoreFreq = 0;
                    sysInfo.gpuIsVirtual = false;
                }

                // Initialize network adapter info (avoid crash due to invalid data)
                sysInfo.networkAdapterName = "No network adapter detected";
                sysInfo.networkAdapterMac = "00-00-00-00-00-00";
                sysInfo.networkAdapterSpeed = 0;
                sysInfo.networkAdapterIp = "N/A";
                sysInfo.networkAdapterType = "Unknown";

                // Populate all network adapter info
                try {
                    sysInfo.adapters.clear();
                    NetworkAdapter netAdapter(*wmiManager);
                    const auto& adapters = netAdapter.GetAdapters();
                    if (!adapters.empty()) {
                        for (const auto& adapter : adapters) {
                            NetworkAdapterData data;
                            // Zero initialize to avoid garbage data
                            memset(&data, 0, sizeof(NetworkAdapterData));
                            
                            // Convert std::string to std::wstring before copying to wchar_t arrays
                            std::wstring nameW = WinUtils::StringToWstring(adapter.name);
                            std::wstring macW = WinUtils::StringToWstring(adapter.mac);
                            std::wstring ipW = WinUtils::StringToWstring(adapter.ip);
                            std::wstring typeW = WinUtils::StringToWstring(adapter.adapterType);
                            
                            // Use 4-argument version: dest, source, size, count
                            wcsncpy_s(data.name, nameW.c_str(), 128);
                            wcsncpy_s(data.mac, macW.c_str(), 32);
                            wcsncpy_s(data.ipAddress, ipW.c_str(), 64);
                            wcsncpy_s(data.adapterType, typeW.c_str(), 32);
                            data.speed = adapter.speed;
                            sysInfo.adapters.push_back(data);
                        }
                        sysInfo.networkAdapterName = adapters[0].name;
                        sysInfo.networkAdapterMac = adapters[0].mac;
                        sysInfo.networkAdapterIp = adapters[0].ip;
                        sysInfo.networkAdapterType = adapters[0].adapterType;
                        sysInfo.networkAdapterSpeed = adapters[0].speed;
                    } else {
                        sysInfo.networkAdapterName = "No network adapter detected";
                        sysInfo.networkAdapterMac = "00-00-00-00-00-00";
                        sysInfo.networkAdapterIp = "N/A";
                        sysInfo.networkAdapterType = "Unknown";
                        sysInfo.networkAdapterSpeed = 0;
                    }
                } catch (const std::bad_alloc& e) {
                    Logger::Error("Failed to get network adapter info - Out of memory: " + std::string(e.what()));
                    sysInfo.adapters.clear();
                    sysInfo.networkAdapterName = "Out of memory";
                    sysInfo.networkAdapterMac = "00-00-00-00-00-00";
                    sysInfo.networkAdapterIp = "N/A"; 
                    sysInfo.networkAdapterType = "Unknown";
                    sysInfo.networkAdapterSpeed = 0;
                } catch (const std::exception& e) {
                    Logger::Error("Failed to get network adapter info: " + std::string(e.what()));
                    sysInfo.adapters.clear();
                    sysInfo.networkAdapterName = "No network adapter detected";
                    sysInfo.networkAdapterMac = "00-00-00-00-00-00";
                    sysInfo.networkAdapterIp = "N/A";
                    sysInfo.networkAdapterType = "Unknown";
                    sysInfo.networkAdapterSpeed = 0;
                } catch (...) {
                    Logger::Error("Failed to get network adapter info - Unknown exception");
                    sysInfo.adapters.clear();
                    sysInfo.networkAdapterName = "Unknown exception";
                    sysInfo.networkAdapterMac = "00-00-00-00-00-00";
                    sysInfo.networkAdapterIp = "N/A";
                    sysInfo.networkAdapterType = "Unknown";
                    sysInfo.networkAdapterSpeed = 0;
                }

                // Add temperature data collection (get each loop to ensure real-time data)
                try {
                    auto temperatures = TemperatureWrapper::GetTemperatures();
                    sysInfo.temperatures.clear();
                    sysInfo.cpuTemperature = 0;
                    sysInfo.gpuTemperature = 0;
                    for (const auto& temp : temperatures) {
                        std::string nameLower = temp.first;
                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                        if (nameLower.find("gpu") != std::string::npos || nameLower.find("graphics") != std::string::npos) {
                            sysInfo.gpuTemperature = temp.second;
                            sysInfo.temperatures.push_back({"GPU", temp.second});
                        } else if (nameLower.find("cpu") != std::string::npos || nameLower.find("package") != std::string::npos) {
                            sysInfo.cpuTemperature = temp.second;
                            sysInfo.temperatures.push_back({"CPU", temp.second});
                        } else {
                            sysInfo.temperatures.push_back(temp);
                        }
                    }
                    if (isFirstRun) {
                        Logger::Debug("Collected " + std::to_string(temperatures.size()) + " temperature readings");
                        for (const auto& temp : sysInfo.temperatures) {
                            Logger::Debug("Temperature sensor: " + temp.first + " = " + std::to_string(temp.second) + "°C");
                        }
                        Logger::Debug("CPU temperature: " + std::to_string(sysInfo.cpuTemperature) + ", GPU temperature: " + std::to_string(sysInfo.gpuTemperature));
                    }
                }
                catch (const std::bad_alloc& e) {
                    Logger::Error("Failed to get temperature data - Out of memory: " + std::string(e.what()));
                    sysInfo.temperatures.clear();
                    sysInfo.cpuTemperature = 0;
                    sysInfo.gpuTemperature = 0;
                }
                catch (const std::exception& e) {
                    Logger::Error("Failed to get temperature data: " + std::string(e.what()));
                    sysInfo.temperatures.clear();
                    sysInfo.cpuTemperature = 0;
                    sysInfo.gpuTemperature = 0;
                }
                catch (...) {
                    Logger::Error("Failed to get temperature data - Unknown exception");
                    sysInfo.temperatures.clear();
                    sysInfo.cpuTemperature = 0;
                    sysInfo.gpuTemperature = 0;
                }

                // Add disk info collection (get each loop to ensure real-time data)
                try {
                    DiskInfo diskInfo;
                    auto disks = diskInfo.GetDisks();
                    if (disks.size() > 8) {
                        Logger::Error("Disk count exceeds maximum allowed (8). Skipping disk data update.");
                        sysInfo.disks.clear();
                    } else {
                        sysInfo.disks = disks;
                        if (isFirstRun) {
                            Logger::Debug("Collected " + std::to_string(disks.size()) + " disk entries");
                            for (size_t i = 0; i < disks.size(); ++i) {
                                const auto& disk = disks[i];
                                Logger::Debug("Disk " + std::to_string(i) + ": Label=" + disk.label + ", FileSystem=" + disk.fileSystem);
                            }
                        }
                    }
                    // Collect physical disks and build logical drive mapping
                    if (wmiManager) {
                        DiskInfo::CollectPhysicalDisks(*wmiManager, sysInfo.disks, sysInfo);
                    }
                    
                    // Collect disk SMART data
                    DiskInfo::CollectSmartData(sysInfo);
                    
                    // Collect TPM data
                    {
                        TpmInfo tpmInfo = {};
                        if (TpmBridge::GetTpmInfo(tpmInfo)) {
                            sysInfo.tpms.clear();
                            sysInfo.tpms.push_back(tpmInfo);
                            Logger::Debug("TPM data collected, isPresent=" + std::to_string(tpmInfo.isPresent));
                        }
                    }
                }
                catch (const std::bad_alloc& e) {
                    Logger::Error("Failed to get disk/physical disk data - Out of memory: " + std::string(e.what()));
                    sysInfo.disks.clear();
                    sysInfo.physicalDisks.clear();
                }
                catch (const std::exception& e) {
                    Logger::Error("Failed to get disk/physical disk data: " + std::string(e.what()));
                    sysInfo.disks.clear();
                    sysInfo.physicalDisks.clear();
                }
                catch (...) {
                    Logger::Error("Failed to get disk/physical disk data - Unknown exception");
                    sysInfo.disks.clear();
                    sysInfo.physicalDisks.clear();
                }

                // Validate data before writing to shared memory - enhanced data validation
                try {
                    // CPU usage validation
                    if (sysInfo.cpuUsage < 0.0 || sysInfo.cpuUsage > 100.0) {
                        Logger::Warn("CPU usage data abnormal: " + std::to_string(sysInfo.cpuUsage) + "%, resetting to 0");
                        sysInfo.cpuUsage = 0.0;
                    }
                    
                    if (sysInfo.totalMemory > 0) {
                        if (sysInfo.usedMemory > sysInfo.totalMemory) {
                            Logger::Warn("Used memory exceeds total memory, data abnormal");
                            sysInfo.usedMemory = sysInfo.totalMemory;
                        }
                        if (sysInfo.availableMemory > sysInfo.totalMemory) {
                            Logger::Warn("Available memory exceeds total memory, data abnormal");
                            sysInfo.availableMemory = sysInfo.totalMemory;
                        }
                    }
                    
                    // Frequency data validation
                    if (std::isnan(sysInfo.performanceCoreFreq) || std::isinf(sysInfo.performanceCoreFreq)) {
                        sysInfo.performanceCoreFreq = 0.0;
                    }
                    if (std::isnan(sysInfo.efficiencyCoreFreq) || std::isinf(sysInfo.efficiencyCoreFreq)) {
                        sysInfo.efficiencyCoreFreq = 0.0;
                    }
                    if (std::isnan(sysInfo.gpuCoreFreq) || std::isinf(sysInfo.gpuCoreFreq)) {
                        sysInfo.gpuCoreFreq = 0.0;
                    }
                    
                    // Temperature data validation
                    if (std::isnan(sysInfo.cpuTemperature) || std::isinf(sysInfo.cpuTemperature)) {
                        sysInfo.cpuTemperature = 0.0;
                    }
                    if (std::isnan(sysInfo.gpuTemperature) || std::isinf(sysInfo.gpuTemperature)) {
                        sysInfo.gpuTemperature = 0.0;
                    }
                    
                    // Network speed validation
                    if (sysInfo.networkAdapterSpeed > 1000000000000ULL) {
                        Logger::Warn("Network adapter speed abnormal: " + std::to_string(sysInfo.networkAdapterSpeed));
                        sysInfo.networkAdapterSpeed = 0;
                    }
                }
                catch (const std::exception& e) {
                    Logger::Error("Exception during data validation: " + std::string(e.what()));
                }
                catch (...) {
                    Logger::Error("Unknown exception during data validation");
                }

                // Write to shared memory - enhanced exception handling
                try {
                    if (SharedMemoryManager::GetBuffer()) {
                        SharedMemoryManager::WriteToSharedMemory(sysInfo);
                        if (isFirstRun) {
                            auto* buf = SharedMemoryManager::GetBuffer();
                            Logger::Info("[DIAG] First SHM write: sysInfo.cpu=" + sysInfo.cpuName +
                                " buf->cpuName[0]=" + std::to_string((int)static_cast<unsigned char>(static_cast<char>(buf->cpuName[0]))) +
                                " usage=" + std::to_string(sysInfo.cpuUsage) + "%");
                        }
                        if (isDetailedLogging) {
                            Logger::Debug("Successfully updated shared memory");
                        }
                    } else {
                        Logger::Error("Shared memory buffer unavailable");
                        if (SharedMemoryManager::InitSharedMemory()) {
                            SharedMemoryManager::WriteToSharedMemory(sysInfo);
                            if (isDetailedLogging) {
                                Logger::Info("Reinitialized and updated shared memory");
                            }
                        } else {
                            Logger::Error("Failed to reinitialize shared memory: " + SharedMemoryManager::GetLastError());
                        }
                    }

                    // Broadcast IPC schema (re-register each loop for new clients)
                    pipeServer.UpdateSchema(ipcHdr, ipcFields);

                    // Process frontend IPC commands from shared memory mailbox
                    IpcCommandHandler::Instance().ProcessCommands(
                        SharedMemoryManager::GetBuffer());

                    if (isDetailedLogging) {
                        Logger::Debug("System info updated to shared memory");
                    }
                }
                catch (const std::bad_alloc& e) {
                    Logger::Error("Out of memory while processing system info: " + std::string(e.what()));
                }
                catch (const std::exception& e) {
                    Logger::Error("Exception while processing system info: " + std::string(e.what()));
                }
                catch (...) {
                    Logger::Error("Unknown exception while processing system info");
                }
                
                // Update TUI with current data
                try {
                    tcmt::TuiData tuiData;
                    tuiData.cpuName = sysInfo.cpuName;
                    tuiData.cpuUsage = sysInfo.cpuUsage;
                    tuiData.physicalCores = sysInfo.physicalCores;
                    tuiData.performanceCores = sysInfo.performanceCores;
                    tuiData.efficiencyCores = sysInfo.efficiencyCores;
                    tuiData.pCoreFreq = sysInfo.performanceCoreFreq;
                    tuiData.eCoreFreq = sysInfo.efficiencyCoreFreq;
                    tuiData.cpuTemp = sysInfo.cpuTemperature;
                    tuiData.totalMemory = sysInfo.totalMemory;
                    tuiData.usedMemory = sysInfo.usedMemory;
                    tuiData.availableMemory = sysInfo.availableMemory;
                    
                    if (!sysInfo.gpus.empty()) {
                        tuiData.gpuName = sysInfo.gpuName;
                        tuiData.gpuMemory = sysInfo.gpuMemory;
                        tuiData.gpuUsage = sysInfo.gpuUsage;
                    }
                    tuiData.gpuTemp = sysInfo.gpuTemperature;
                    
                    // Disks
                    for (const auto& disk : sysInfo.disks) {
                        tcmt::TuiData::DiskInfo di;
                        di.label = disk.label;
                        di.totalSize = disk.totalSize;
                        di.usedSpace = disk.usedSpace;
                        di.fileSystem = disk.fileSystem;
                        tuiData.disks.push_back(di);
                    }
                    
                    // Network adapters
                    for (const auto& adapter : sysInfo.adapters) {
                        tcmt::TuiData::NetInfo ni;
                        // Convert wchar_t[] arrays to std::string
                        ni.name = WinUtils::WstringToString(adapter.name);
                        ni.ip = WinUtils::WstringToString(adapter.ipAddress);
                        ni.mac = WinUtils::WstringToString(adapter.mac);
                        ni.type = WinUtils::WstringToString(adapter.adapterType);
                        ni.speed = adapter.speed;
                        tuiData.adapters.push_back(ni);
                    }
                    
                    tuiData.osVersion = sysInfo.osVersion;
                    tuiData.temperatures = sysInfo.temperatures;
                    if (!sysInfo.tpms.empty() && sysInfo.tpms[0].isPresent) {
                        auto& tpm = sysInfo.tpms[0];
                        tuiData.tpmInfo = WinUtils::WstringToString(tpm.manufacturer)
                                        + " v" + WinUtils::WstringToString(tpm.firmwareVersion);
                        if (!tpm.isEnabled) tuiData.tpmInfo += " (Disabled)";
                        else if (!tpm.isActive) tuiData.tpmInfo += " (Inactive)";
                    } else {
                        tuiData.tpmInfo = "No TPM";
                    }
                    tuiData.timestamp = FormatDateTime(std::chrono::system_clock::now());
                    
                    if (isFirstRun) {
                        Logger::Info("[DIAG] TUI update: cpu=" + tuiData.cpuName +
                            " usage=" + std::to_string(tuiData.cpuUsage) +
                            " mem=" + std::to_string(tuiData.totalMemory));
                    }
                    tuiApp.UpdateData(tuiData);
                }
                catch (const std::exception& e) {
                    Logger::Fatal("[DIAG] TUI update exception: " + std::string(e.what()));
                }

                // Calculate loop execution time and adaptive sleep - optimize refresh speed, enhanced exception handling
                try {
                    auto loopEnd = std::chrono::high_resolution_clock::now();
                    auto loopDuration = std::chrono::duration_cast<std::chrono::milliseconds>(loopEnd - loopStart);
                    
                    // 1 second cycle time
                    int targetCycleTime = 1000;
                    int sleepTime = (std::max)(targetCycleTime - static_cast<int>(loopDuration.count()), 100); // Min sleep 100ms
                    
                    if (isDetailedLogging) {
                        double loopTimeSeconds = loopDuration.count() / 1000.0;
                        double sleepTimeSeconds = sleepTime / 1000.0;
                        
                        if (loopTimeSeconds < 0 || loopTimeSeconds > 60) {
                            Logger::Warn("Loop time calculation abnormal: " + std::to_string(loopTimeSeconds) + " seconds");
                        }
                        
                        std::stringstream ss;
                        ss << std::fixed << std::setprecision(2);
                        ss << "Main monitoring loop #" << loopCounter << " executed in " 
                           << loopTimeSeconds << "s, will sleep for " << sleepTimeSeconds << "s";
                        
                        Logger::Debug(ss.str());
                    }
                    
                    // Check exit flag during sleep - use shorter check interval for better responsiveness
                    auto sleepStart = std::chrono::high_resolution_clock::now();
                    while (!g_shouldExit.load()) {
                        try {
                            auto now = std::chrono::high_resolution_clock::now();
                            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - sleepStart);
                            if (elapsed.count() >= sleepTime) {
                                break;
                            }
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        }
                        catch (const std::exception& e) {
                            Logger::Warn("Exception during sleep: " + std::string(e.what()));
                            break;
                        }
                        catch (...) {
                            Logger::Warn("Unknown exception during sleep");
                            break;
                        }
                    }
                }
                catch (const std::exception& e) {
                    Logger::Error("Exception while calculating loop time: " + std::string(e.what()));
                    try {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    } catch (...) {
                        Logger::Fatal("System sleep function abnormal");
                    }
                }
                catch (...) {
                    Logger::Error("Unknown exception while calculating loop time");
                    try {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    } catch (...) {
                        Logger::Fatal("System sleep function abnormal");
                    }
                }
                
                // Safely increment loop counter
                try {
                    loopCounter++;
                    
                    if (loopCounter < 0 || loopCounter > 2000000000) {
                        Logger::Warn("Loop counter abnormal, resetting to 1");
                        loopCounter = 1;
                    }
                }
                catch (...) {
                    Logger::Error("Failed to update loop counter");
                    loopCounter = 1;
                }
                
                // Set flag after first run
                if (isFirstRun) {
                    isFirstRun = false;
                }
            }
            catch (const std::bad_alloc& e) {
                Logger::Critical("Memory allocation exception in main loop: " + std::string(e.what()));
                try {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                } catch (...) {
                    Logger::Fatal("Cannot execute sleep, system severe exception");
                }
                continue;
            }
            catch (const std::exception& e) {
                Logger::Critical("Exception in main loop: " + std::string(e.what()));
                try {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                } catch (...) {
                    Logger::Fatal("Cannot execute sleep, system severe exception");
                }
                continue;
            }
            catch (...) {
                Logger::Fatal("Unknown exception in main loop");
                try {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                } catch (...) {
                    SafeExit(1);
                }
                continue;
            }
        }
        
        Logger::Info("Program received exit signal, starting cleanup");
        
        // Stop TUI before cleanup
        try {
            tuiApp.Stop();
            Logger::Debug("TUI stopped");
        }
        catch (const std::exception& e) {
            Logger::Error("Error stopping TUI: " + std::string(e.what()));
        }

        pipeServer.Stop();
        Logger::Debug("NamedPipe server stopped");

        SafeExit(0);
    }
    catch (const std::exception& e) {
        Logger::Fatal("Program fatal error: " + std::string(e.what()));
        SafeExit(1);
    }
    catch (...) {
        Logger::Fatal("Program unknown fatal error");
        SafeExit(1);
    }
}

// Check for key press (non-blocking) - enhanced exception handling
bool CheckForKeyPress() {
    try {
        return _kbhit() != 0;
    }
    catch (const std::exception& e) {
        Logger::Warn("Exception while checking key press: " + std::string(e.what()));
        return false;
    }
    catch (...) {
        Logger::Warn("Unknown exception while checking key press");
        return false;
    }
}

char GetKeyPress() {
    try {
        if (_kbhit()) {
            char key = _getch();
            if (key >= 0 && key <= 127) {
                return key;
            } else {
                Logger::Warn("Detected abnormal key value: " + std::to_string(static_cast<int>(key)));
                return 0;
            }
        }
    }
    catch (const std::exception& e) {
        Logger::Warn("Exception while getting key press: " + std::string(e.what()));
    }
    catch (...) {
        Logger::Warn("Unknown exception while getting key press");
    }
    return 0;
}


