// Use #pragma unmanaged to ensure main function compiles as native code
#pragma unmanaged

// ✅ Add stack size control to prevent stack overflow
#pragma comment(linker, "/STACK:8388608")  // Set stack size to 8MB

/*
If the following warning appears:
Warning	MSB8077	Some files are set to C++/CLI, but the "Enable CLR Support for single file" property is not defined. For more details, please refer to the "Advanced Property Pages" documentation.
Please ignore the above warning, as this project structure cannot accommodate this situation.
*/
// First include Windows headers to avoid macro redefinition warnings
// Fix sockaddr redefinition issue: ensure winsock2.h is included after windows.h
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#include <sddl.h>
#include <Aclapi.h>
#include <conio.h>   // Add keyboard input support
#include <eh.h>      // Add structured exception handling support

// Then include standard library headers
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <utility>
#include <thread>
#include <system_error>
#include <io.h>
#include <fcntl.h>
#include <algorithm> // Include for std::transform
#include <vector> // Include for std::vector
#include <mutex>     // Add thread synchronization support
#include <atomic>    // Add atomic operation support
#include <locale>   // Add locale support for using setlocale
#include <new>       // Add memory allocation exception support
#include <stdexcept> // Add standard exception support

// Finally include project headers
#include "core/cpu/CpuInfo.h"
#include "core/gpu/GpuInfo.h"
#include "core/memory/MemoryInfo.h"
#include "core/network/NetworkAdapter.h"
#include "core/os/OSInfo.h"
#include "core/utils/Logger.h"
#include "core/utils/TimeUtils.h"
#include "core/utils/WinUtils.h"
#include "core/Utils/WMIManager.h"
#include "core/disk/DiskInfo.h"
#include "core/DataStruct/DataStruct.h"
#include "core/DataStruct/SharedMemoryManager.h"  // Include the new shared memory manager
#include "core/temperature/TemperatureWrapper.h"  // Use TemperatureWrapper instead of directly calling LibreHardwareMonitorBridge
#include "core/tpm/TpmInfo.h"  // TPM detection (TBS primary, WMI fallback)
#include "core/tpm/TpmInfoEnhanced.h"  // Enhanced TPM detection using tpm2-tss
#include "core/usb/USBInfo.h"  // USB device monitoring

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")

// Global variables
std::atomic<bool> g_shouldExit{false};
static std::atomic<bool> g_monitoringStarted{false};
static std::atomic<bool> g_comInitialized{false};

// Thread-safe console output mutex
static std::mutex g_consoleMutex;

// Cache for static system information (fetched only once)
static std::atomic<bool> systemInfoCached{false};
static std::string cachedOsVersion;
static std::string cachedCpuName;
static uint32_t cachedPhysicalCores = 0;
static uint32_t cachedLogicalCores = 0;
static uint32_t cachedPerformanceCores = 0;
static uint32_t cachedEfficiencyCores = 0;
static bool cachedHyperThreading = false;
static bool cachedVirtualization = false;

// Function declarations
bool CheckForKeyPress();
char GetKeyPress();
void SafeExit(int exitCode);

// Structured exception handling function
void SEHTranslator(unsigned int u, EXCEPTION_POINTERS* pExp);
std::string GetSEHExceptionName(DWORD exceptionCode);

// Thread-safe console output functions
void SafeConsoleOutput(const std::string& message);
void SafeConsoleOutput(const std::string& message, int color);

// Signal handler function - simplified version
BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        Logger::Info("Received system shutdown signal, exiting safely...");
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
        // Use UTF-8 encoding for output to ensure correct display of characters
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
            
            // Fallback to ASCII output if UTF-8 conversion fails
            DWORD written;
            WriteConsoleA(hConsole, message.c_str(), static_cast<DWORD>(message.length()), &written, NULL);
        }
    }
    catch (...) {
        // Ignore console output errors to avoid recursive exceptions
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
            
            // Set new color
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
                        // Fallback to ASCII output if conversion fails
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
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "Datatype Misalignment";
        case EXCEPTION_FLT_DENORMAL_OPERAND: return "Floating-Point Denormal Operand";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "Floating-Point Divide by Zero";
        case EXCEPTION_FLT_INEXACT_RESULT: return "Floating-Point Inexact Result";
        case EXCEPTION_FLT_INVALID_OPERATION: return "Floating-Point Invalid Operation";
        case EXCEPTION_FLT_OVERFLOW: return "Floating-Point Overflow";
        case EXCEPTION_FLT_STACK_CHECK: return "Floating-Point Stack Check";
        case EXCEPTION_FLT_UNDERFLOW: return "Floating-Point Underflow";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "Illegal Instruction";
        case EXCEPTION_IN_PAGE_ERROR: return "In Page Error";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "Integer Divide by Zero";
        case EXCEPTION_INT_OVERFLOW: return "Integer Overflow";
        case EXCEPTION_INVALID_DISPOSITION: return "Invalid Disposition";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "Non-continuable Exception";
        case EXCEPTION_PRIV_INSTRUCTION: return "Privileged Instruction";
        case EXCEPTION_SINGLE_STEP: return "Single Step";
        case EXCEPTION_STACK_OVERFLOW: return "Stack Overflow";
        default: return "Unknown System Exception (0x" + std::to_string(exceptionCode) + ")";
    }
}

void SEHTranslator(unsigned int u, EXCEPTION_POINTERS* pExp) {
    std::string exceptionName = GetSEHExceptionName(u);
    std::stringstream ss;
    ss << "System-level exception: " << exceptionName << " (0x" << std::hex << u << ")";
    if (pExp && pExp->ExceptionRecord) {
        ss << " Address: 0x" << std::hex << pExp->ExceptionRecord->ExceptionAddress;
    }
    
    // Try to log safely
    try {
        if (Logger::IsInitialized()) {
            Logger::Fatal(ss.str());
        } else {
            // If logger is not initialized, output directly to console
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
        Logger::Info("Starting program cleanup process");
        
        // Set exit flag
        g_shouldExit = true;
        
        // Cleanup hardware monitor bridge
        try {
            TemperatureWrapper::Cleanup();
            // removed noisy TemperatureWrapper debug log
        }
        catch (const std::exception& e) {
            Logger::Error("Error during hardware monitor bridge cleanup: " + std::string(e.what()));
        }
        
        // Cleanup shared memory
        try {
            SharedMemoryManager::CleanupSharedMemory();
            Logger::Debug("Shared memory cleanup complete");
        }
        catch (const std::exception& e) {
            Logger::Error("Error during shared memory cleanup: " + std::string(e.what()));
        }
        
        // Cleanup COM
        if (g_comInitialized.load()) {
            try {
                CoUninitialize();
                g_comInitialized = false;
                Logger::Debug("COM cleanup complete");
            }
            catch (...) {
                Logger::Error("Unknown error during COM cleanup");
            }
        }
        
        Logger::Info("Program cleanup complete, exit code: " + std::to_string(exitCode));
        
        // Give the logger some time to finish writing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    catch (...) {
        // Final exception handler to avoid crashing during cleanup
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
    if (name.find(L"NVIDIA") != std::string::npos) return "NVIDIA";
    if (name.find(L"AMD") != std::string::npos) return "AMD";
    if (name.find(L"Intel") != std::string::npos) return "Intel";
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

// Time formatting - with enhanced exception handling
std::string FormatDateTime(const std::chrono::system_clock::time_point& tp) {
    try {
        auto time = std::chrono::system_clock::to_time_t(tp);
        struct tm timeinfo;
        if (localtime_s(&timeinfo, &time) == 0) {  // Check return value
            std::stringstream ss;
            ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
            std::string result = ss.str();
            
            // Validate result reasonableness
            if (result.length() >= 19 && result.length() <= 25) {  // Basic length check
                return result;
            } else {
                Logger::Warn("Abnormal length of formatted time result: " + std::to_string(result.length()));
            }
        } else {
            Logger::Error("localtime_s call failed");
        }
    }
    catch (const std::exception& e) {
        Logger::Error("Exception occurred during time formatting: " + std::string(e.what()));
    }
    catch (...) {
        Logger::Error("Unknown exception occurred during time formatting");
    }
    return "Time formatting failed";
}

std::string FormatFrequency(double value) {
    try {
        // Parameter validation
        if (std::isnan(value) || std::isinf(value)) {
            Logger::Warn("Invalid frequency value: " + std::to_string(value));
            return "N/A";
        }
        
        if (value < 0) {
            Logger::Warn("Negative frequency value: " + std::to_string(value));
            return "N/A";
        }
        
        // Reasonableness check - frequency usually doesn't exceed 10GHz
        if (value > 10000) {
            Logger::Warn("Abnormal frequency value: " + std::to_string(value) + "MHz");
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
        Logger::Error("Exception occurred while formatting frequency: " + std::string(e.what()));
        return "Formatting failed";
    }
    catch (...) {
        Logger::Error("Unknown exception occurred while formatting frequency");
        return "Formatting failed";
    }
}

std::string FormatPercentage(double value) {
    try {
        // Parameter validation
        if (std::isnan(value) || std::isinf(value)) {
            Logger::Warn("Invalid percentage value: " + std::to_string(value));
            return "N/A";
        }
        
        // Reasonableness check - percentage is usually between 0-100, with some tolerance
        if (value < -1.0 || value > 105.0) {
            Logger::Warn("Abnormal percentage value: " + std::to_string(value));
        }
        
        // Clamp to a reasonable range
        if (value < 0) value = 0;
        if (value > 100) value = 100;
        
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << value << "%";
        return ss.str();
    }
    catch (const std::exception& e) {
        Logger::Error("Exception occurred while formatting percentage: " + std::string(e.what()));
        return "Formatting failed";
    }
    catch (...) {
        Logger::Error("Unknown exception occurred while formatting percentage");
        return "Formatting failed";
    }
}

std::string FormatTemperature(double value) {
    try {
        // Parameter validation
        if (std::isnan(value) || std::isinf(value)) {
            Logger::Warn("Invalid temperature value: " + std::to_string(value));
            return "N/A";
        }
        
        // Reasonableness check - temperature is usually between -50°C and 150°C
        if (value < -50.0 || value > 150.0) {
            Logger::Warn("Abnormal temperature value: " + std::to_string(value) + "°C");
            if (value < -50.0) return "Too Low";
            if (value > 150.0) return "Too High";
        }
        
        std::stringstream ss;
        ss << static_cast<int>(value) << "°C";  // Display integer temperature
        return ss.str();
    }
    catch (const std::exception& e) {
        Logger::Error("Exception occurred while formatting temperature: " + std::string(e.what()));
        return "Formatting failed";
    }
    catch (...) {
        Logger::Error("Unknown exception occurred while formatting temperature");
        return "Formatting failed";
    }
}

std::string FormatSize(uint64_t bytes, bool useBinary = true) {
    try {
        const double kb = useBinary ? 1024.0 : 1000.0;
        const double mb = kb * kb;
        const double gb = mb * kb;
        const double tb = gb * kb;

        // Parameter validation - check for max value (usually indicates an error)
        if (bytes == UINT64_MAX) {
            Logger::Warn("Byte count is at max value, may indicate an error state");
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
        Logger::Error("Exception occurred while formatting size: " + std::string(e.what()));
        return "Formatting failed";
    }
    catch (...) {
        Logger::Error("Unknown exception occurred while formatting size");
        return "Formatting failed";
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
    // Format to a fixed width
    if (line.length() < 27) {
        line += std::string(27 - line.length(), ' ');
    }
    line += ": " + value + "\n";
    SafeConsoleOutput(line);
}

// Main functions
// Check if running as administrator
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

// Thread-safe GPU information cache class
class ThreadSafeGpuCache {
private:
    mutable std::mutex mtx_;
    bool initialized_ = false;
    std::string cachedGpuName_ = "GPU not detected";
    std::string cachedGpuBrand_ = "Unknown";
    uint64_t cachedGpuMemory_ = 0;
    uint32_t cachedGpuCoreFreq_ = 0;
    bool cachedGpuIsVirtual_ = false;

public:
    void Initialize(WmiManager& wmiManager) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (initialized_) return;
        
        try {
            Logger::Info("Initializing GPU information");
            
            GpuInfo gpuInfo(wmiManager);
            const auto& gpus = gpuInfo.GetGpuData();
            
            // Log all detected GPUs
            for (const auto& gpu : gpus) {
                std::string gpuName = WinUtils::WstringToString(gpu.name);
                Logger::Info("Detected GPU: " + gpuName + 
                           " (Virtual: " + (gpu.isVirtual ? "Yes" : "No") + 
                           ", NVIDIA: " + (gpuName.find("NVIDIA") != std::string::npos ? "Yes" : "No") + 
                           ", Integrated: " + (gpuName.find("Intel") != std::string::npos ||
                                       gpuName.find("AMD") != std::string::npos ? "Yes" : "No") + ")");
            }
            
            // Prioritize non-virtual GPUs
            const GpuInfo::GpuData* selectedGpu = nullptr;
            for (const auto& gpu : gpus) {
                if (!gpu.isVirtual) {
                    selectedGpu = &gpu;
                    break;
                }
            }
            
            // If no non-virtual GPU, select the first one
            if (!selectedGpu && !gpus.empty()) {
                selectedGpu = &gpus[0];
            }
            
            if (selectedGpu) {
                cachedGpuName_ = WinUtils::WstringToString(selectedGpu->name);
                cachedGpuBrand_ = GetGpuBrand(selectedGpu->name);
                cachedGpuMemory_ = selectedGpu->dedicatedMemory;
                cachedGpuCoreFreq_ = static_cast<uint32_t>(selectedGpu->coreClock);
                cachedGpuIsVirtual_ = selectedGpu->isVirtual;
                
                Logger::Info("Selected main GPU: " + cachedGpuName_ + 
                           " (Virtual: " + (cachedGpuIsVirtual_ ? "Yes" : "No") + ")");
            } else {
                Logger::Warn("No GPU detected");
            }
            
            initialized_ = true;
            Logger::Info("GPU information initialization complete, subsequent loops will use cached info");
        }
        catch (const std::exception& e) {
            Logger::Error("GPU information initialization failed: " + std::string(e.what()));
            // Keep default values
            initialized_ = true; // Mark as initialized to avoid retrying
        }
    }
    
    void GetCachedInfo(std::string& name, std::string& brand, uint64_t& memory, 
                       uint32_t& coreFreq, bool& isVirtual) const {
        std::lock_guard<std::mutex> lock(mtx_);
        name = cachedGpuName_;
        brand = cachedGpuBrand_;
        memory = cachedGpuMemory_;
        coreFreq = cachedGpuCoreFreq_;
        isVirtual = cachedGpuIsVirtual_;
    }
    
    bool IsInitialized() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return initialized_;
    }
}; // Added missing semicolon

// Main function - console mode
int main(int argc, char* argv[]) {
    // Set structured exception handling
    _set_se_translator(SEHTranslator);
    
    // Set memory allocation failure handling
    std::set_new_handler([]() {
        Logger::Fatal("Memory allocation failed - insufficient system memory");
        throw std::bad_alloc();
    });
    
    // Set console encoding to UTF-8 to ensure correct character display
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    
    // Set locale support for UTF-8
    setlocale(LC_ALL, "C.UTF-8");
    
    // Set console signal handler
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    
    try {
        // Initialize logging system
        try {
            // Logger::EnableConsoleOutput(true); // Comment out this line to prevent identifier not found error

            Logger::Initialize("system_monitor.log", false);
            Logger::SetLogLevel(LL_DEBUG);
            Logger::Info("Program startup");
        }
        catch (const std::exception& e) {
            printf("Logging system initialization failed: %s\n", e.what());
            return 1;
        }

        // Check administrator privileges
        if (!IsRunAsAdmin()) {
            wchar_t szPath[MAX_PATH];
            GetModuleFileNameW(NULL, szPath, MAX_PATH);

            // Restart itself with administrator privileges
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.lpVerb = L"runas";
            sei.lpFile = szPath;
            sei.hwnd = NULL;
            sei.nShow = SW_NORMAL;

            if (ShellExecuteExW(&sei)) {
                // Startup successful, exit current process
                exit(0);
            } else {
                // Startup failed, show popup prompt
                MessageBoxW(NULL, L"Automatic privilege elevation failed, please right-click and run as administrator.", L"Insufficient Privileges", MB_OK | MB_ICONERROR);
                SafeExit(1);
            }
        }

        // Safely initialize COM for multi-threaded mode
        try {
            HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (FAILED(hr)) {
                if (hr == RPC_E_CHANGED_MODE) {
                    Logger::Warn("COM initialization mode conflict: thread already initialized with different mode, trying single-threaded mode");
                    // Try single-threaded mode
                    hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                    if (FAILED(hr)) {
                        Logger::Error("COM initialization failed: 0x" + std::to_string(hr));
                        return -1;
                    }
                }
                else {
                    Logger::Error("COM初始化失败: 0x" + std::to_string(hr));
                    return -1;
                }
            }
            g_comInitialized = true;
            Logger::Debug("COM initialization successful");
        }
        catch (const std::exception& e) {
            Logger::Error("Exception occurred during COM initialization: " + std::string(e.what()));
            return -1;
        }

        // Initialize shared memory - enhanced error handling
        try {
            if (!SharedMemoryManager::InitSharedMemory()) {
                std::string error = SharedMemoryManager::GetLastError();
                Logger::Error("Shared memory initialization failed: " + error);
                
                // Try to reinitialize
                Logger::Info("Attempting to reinitialize shared memory...");
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                
                if (!SharedMemoryManager::InitSharedMemory()) {
                    Logger::Critical("Shared memory reinitialization failed, program cannot continue running");
                    SafeExit(1);
                }
            }
            Logger::Info("共享内存初始化成功");
        }
        catch (const std::exception& e) {
            Logger::Error("共享内存初始化过程中发生异常: " + std::string(e.what()));
            SafeExit(1);
        }

        // 创建WMI管理器并初始化 - 增强内存分配异常处理
        std::unique_ptr<WmiManager> wmiManager;
        try {
            wmiManager = std::make_unique<WmiManager>();
            if (!wmiManager) {
                Logger::Fatal("WMI管理器对象创建失败 - 内存分配返回null");
                SafeExit(1);
            }
            if (!wmiManager->IsInitialized()) {
                Logger::Error("WMI初始化失败");
                MessageBoxA(NULL, "WMI初始化失败，无法获取系统信息。", "错误", MB_OK | MB_ICONERROR);
                SafeExit(1);
            }
            Logger::Debug("WMI管理器初始化成功");
        }
        catch (const std::bad_alloc& e) {
            Logger::Fatal("WMI管理器创建失败 - 内存分配失败: " + std::string(e.what()));
            SafeExit(1);
        }
        catch (const std::exception& e) {
            Logger::Error("WMI管理器创建失败: " + std::string(e.what()));
            SafeExit(1);
        }
        catch (...) {
            Logger::Fatal("WMI管理器创建失败 - 未知异常");
            SafeExit(1);
        }

        // 初始化硬件监控桥接
        try {
            TemperatureWrapper::Initialize();
            // Logger::Debug("硬件监控桥接初始化成功"); // remove noisy debug
        }
        catch (const std::exception& e) {
            Logger::Error("硬件监控桥接初始化失败: " + std::string(e.what()));
            // 不退出程序，继续运行但温度数据可能不可用
        }

        Logger::Info("程序启动完成");
        
        // 初始化循环计数器，减少频繁的日志记录
        int loopCounter = 1; // 从1开始计数，更符合人类习惯
        bool isFirstRun = true; // 首次运行标志
        
        // 创建CPU对象一次，重复使用（避免重复初始化性能计数器）- 增强异常处理
        std::unique_ptr<CpuInfo> cpuInfo;
        try {
            cpuInfo = std::make_unique<CpuInfo>();
            if (!cpuInfo) {
                Logger::Fatal("CPU信息对象创建失败 - 内存分配返回null");
                SafeExit(1);
            }
            Logger::Debug("CPU信息对象创建成功");
        }
        catch (const std::bad_alloc& e) {
            Logger::Fatal("CPU信息对象创建失败 - 内存分配失败: " + std::string(e.what()));
            SafeExit(1);
        }
        catch (const std::exception& e) {
            Logger::Error("CPU信息对象创建失败: " + std::string(e.what()));
            SafeExit(1);
        }
        catch (...) {
            Logger::Fatal("CPU信息对象创建失败 - 未知异常");
            SafeExit(1);
        }
        
        // 初始化WMI管理器（用于主板信息采集和TPM检测）
        try {
            wmiManager = std::make_unique<WmiManager>();
            if (!wmiManager->IsInitialized()) {
                Logger::Warn("WMI管理器初始化失败，某些功能可能不可用");
            } else {
                Logger::Info("WMI管理器初始化成功");
            }
        }
        catch (const std::exception& e) {
            Logger::Error("WMI管理器创建异常: " + std::string(e.what()));
        }
        catch (...) {
            Logger::Error("WMI管理器创建失败 - 未知异常");
        }
        
        // 线程安全的GPU缓存
        ThreadSafeGpuCache gpuCache;
        
        while (!g_shouldExit.load()) {
            // TPM缓存静态局部变量（首次进入循环时初始化一次）
            try {
                auto loopStart = std::chrono::high_resolution_clock::now();
                
                // 只在每5次循环记录一次详细信息（约5秒）
                bool isDetailedLogging = (loopCounter % 5 == 1); // 第1, 6, 11, 16, 21... 次循环
                
                if (isDetailedLogging) {
                    Logger::Debug("开始执行主监控循环第 #" + std::to_string(loopCounter) + " 次迭代");
                }
                
                // 在第5次循环后，监控已稳定运行
                if (loopCounter == 5) {
                    g_monitoringStarted = true;
                    Logger::Info("程序已稳定运行");
                }
                
                // 获取系统信息
                SystemInfo sysInfo;
                
                // 安全初始化所有字段以避免未定义行为 - 增强异常处理
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
                    // 安全地初始化 SYSTEMTIME 结构
                    ZeroMemory(&sysInfo.lastUpdate, sizeof(sysInfo.lastUpdate));
#ifdef PLATFORM_WINDOWS
                    GetSystemTime(&sysInfo.lastUpdate.windowsTime); // 设置当前时间
                    
                    // 验证系统时间是否合理
                    if (sysInfo.lastUpdate.windowsTime.wYear < 2020 || sysInfo.lastUpdate.windowsTime.wYear > 2050) {
                        Logger::Warn("系统时间异常: " + std::to_string(sysInfo.lastUpdate.windowsTime.wYear));
                    }
#else
                    auto now = std::chrono::system_clock::now();
                    auto time_t = std::chrono::system_clock::to_time_t(now);
                    sysInfo.lastUpdate.unixTime = time_t;
                    sysInfo.lastUpdate.milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
#endif
                    
                    // TPM检测 - 使用增强的TPM信息采集
                    try {
                        if (wmiManager && wmiManager->IsInitialized()) {
                            TpmInfoEnhanced tpmEnhanced(*wmiManager);
                            const TpmDataEnhanced& tpmData = tpmEnhanced.GetEnhancedTpmData();
                            
                            // 更新SystemInfo中的TPM字段
                            sysInfo.hasTpm = (tpmData.tpmPresent != 0);
                            // Convert wstring to string
                            sysInfo.tpmManufacturer = std::string(tpmData.manufacturerName.begin(), tpmData.manufacturerName.end());
                            sysInfo.tpmManufacturerId = std::string(tpmData.manufacturerId.begin(), tpmData.manufacturerId.end());
                            sysInfo.tpmVersion = std::string(tpmData.version.begin(), tpmData.version.end());
                            sysInfo.tpmFirmwareVersion = std::string(tpmData.firmwareVersion.begin(), tpmData.firmwareVersion.end());
                            sysInfo.tpmEnabled = tpmData.isEnabled;
                            sysInfo.tpmIsActivated = tpmData.isActivated;
                            sysInfo.tpmIsOwned = tpmData.isOwned;
                            sysInfo.tpmReady = tpmData.isReady;
                            sysInfo.tpmTbsAvailable = tpmData.tbsAvailable;
                            sysInfo.tpmSpecVersion = tpmData.specVersion;
                            sysInfo.tpmTbsVersion = tpmData.tbsVersion;
                            sysInfo.tpmErrorMessage = std::string(tpmData.errorMessage.begin(), tpmData.errorMessage.end());
                            sysInfo.tmpDetectionMethod = std::string(tpmData.detectionMethod.begin(), tpmData.detectionMethod.end());
                            sysInfo.tmpWmiDetectionWorked = tpmData.wmiDetectionWorked;
                            sysInfo.tmpTbsDetectionWorked = tpmData.tbsDetectionWorked;
                            
                            if (sysInfo.hasTpm) {
                                Logger::Info("TPM检测成功: " + std::string(tpmData.manufacturerName.begin(), tpmData.manufacturerName.end()));
                            }
                        } else {
                            Logger::Warn("WMI管理器未初始化，跳过TPM检测");
                            sysInfo.hasTpm = false;
                        }
                    }
                    catch (const std::exception& e) {
                        Logger::Error("TPM检测异常: " + std::string(e.what()));
                        sysInfo.hasTpm = false;
                    }
                }
                catch (const std::exception& e) {
                    Logger::Error("SystemInfo初始化失败: " + std::string(e.what()));
                    continue; // 跳过当前循环
                }
                catch (...) {
                    Logger::Error("SystemInfo初始化失败 - 未知异常");
                    continue; // 跳过当前循环
                }

                // 静态系统信息（只在首次获取）
                if (!systemInfoCached.load()) {
                    try {
                        Logger::Info("正在初始化系统信息");
                        
                        // 操作系统信息
                        OSInfo os;
                        cachedOsVersion = os.GetVersion();

                        // CPU基本信息（使用cpuInfo对象）
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
                        Logger::Info("系统信息初始化完成");
                    }
                    catch (const std::exception& e) {
                        Logger::Error("系统信息初始化失败: " + std::string(e.what()));
                        // 设置默认值
                        cachedOsVersion = "未知";
                        cachedCpuName = "未知";
                        systemInfoCached = true; // 防止重复尝试
                    }
                }
                
                // 使用缓存的静态信息
                sysInfo.osVersion = cachedOsVersion;
                sysInfo.cpuName = cachedCpuName;
                sysInfo.physicalCores = cachedPhysicalCores;
                sysInfo.logicalCores = cachedLogicalCores;
                sysInfo.performanceCores = cachedPerformanceCores;
                sysInfo.efficiencyCores = cachedEfficiencyCores;
                sysInfo.hyperThreading = cachedHyperThreading;
                sysInfo.virtualization = cachedVirtualization;

                // 动态CPU信息（每次循环都需要获取）
                try {
                    if (cpuInfo) {
                        sysInfo.cpuUsage = cpuInfo->GetUsage();
                        sysInfo.performanceCoreFreq = cpuInfo->GetLargeCoreSpeed();
                        sysInfo.efficiencyCoreFreq = cpuInfo->GetSmallCoreSpeed() * 0.8;
                        // 新增：CPU 基准/即时频率（MHz）
                        sysInfo.cpuBaseFrequencyMHz = cpuInfo->GetBaseFrequencyMHz();
                        sysInfo.cpuCurrentFrequencyMHz = cpuInfo->GetCurrentFrequencyMHz();
                        sysInfo.cpuUsageSampleIntervalMs = cpuInfo->GetLastSampleIntervalMs();
                    }
                }
                catch (const std::exception& e) {
                    Logger::Error("获取CPU动态信息失败: " + std::string(e.what()));
                    // 保持默认值
                }

                // 内存信息（每次循环都获取以确保数据实时性）
                try {
                    MemoryInfo mem;
                    sysInfo.totalMemory = mem.GetTotalPhysical();
                    sysInfo.usedMemory = mem.GetTotalPhysical() - mem.GetAvailablePhysical();
                    sysInfo.availableMemory = mem.GetAvailablePhysical();
                }
                catch (const std::exception& e) {
                    Logger::Error("获取内存信息失败: " + std::string(e.what()));
                    // 保持默认值
                }

                // GPU信息 - 使用线程安全的缓存机制
                if (!gpuCache.IsInitialized()) {
                    try {
                        gpuCache.Initialize(*wmiManager);
                    }
                    catch (const std::exception& e) {
                        Logger::Error("GPU缓存初始化失败: " + std::string(e.what()));
                    }
                }
                
                // 获取缓存的GPU信息
                try {
                    std::string cachedGpuName, cachedGpuBrand;
                    uint64_t cachedGpuMemory;
                    uint32_t cachedGpuCoreFreq;
                    bool cachedGpuIsVirtual;
                    
                    gpuCache.GetCachedInfo(cachedGpuName, cachedGpuBrand, cachedGpuMemory, 
                                          cachedGpuCoreFreq, cachedGpuIsVirtual);
                    
                    sysInfo.gpuName = cachedGpuName;
                    sysInfo.gpuBrand = cachedGpuBrand;
                    sysInfo.gpuMemory = cachedGpuMemory;
                    sysInfo.gpuCoreFreq = cachedGpuCoreFreq;
                    sysInfo.gpuIsVirtual = cachedGpuIsVirtual;

                    // 修复GPU数组填充 - 添加数据验证和清理
                    sysInfo.gpus.clear();
                    if (!cachedGpuName.empty() && cachedGpuName != "GPU not detected") {
                        GPUData gpu;
                        
                        // 初始化GPU结构体以避免垃圾数据
                        memset(&gpu, 0, sizeof(GPUData));
                        
                        // 安全地复制GPU名称和品牌到wchar_t数组
                        std::wstring gpuNameW = WinUtils::StringToWstring(cachedGpuName);
                        std::wstring gpuBrandW = WinUtils::StringToWstring(cachedGpuBrand);
                        
                        // 限制字符串长度以防止缓冲区溢出
                        if (gpuNameW.length() >= sizeof(gpu.name)/sizeof(wchar_t)) {
                            gpuNameW = gpuNameW.substr(0, sizeof(gpu.name)/sizeof(wchar_t) - 1);
                        }
                        if (gpuBrandW.length() >= sizeof(gpu.brand)/sizeof(wchar_t)) {
                            gpuBrandW = gpuBrandW.substr(0, sizeof(gpu.brand)/sizeof(wchar_t) - 1);
                        }
                        
                        wcsncpy_s(gpu.name, _countof(gpu.name), gpuNameW.c_str(), _TRUNCATE);
                        wcsncpy_s(gpu.brand, _countof(gpu.brand), gpuBrandW.c_str(), _TRUNCATE);
                        
                        // 验证和清理GPU数据 - 避免异常值
                        gpu.memory = (cachedGpuMemory > 0 && cachedGpuMemory < UINT64_MAX) ? cachedGpuMemory : 0;
                        
                        // 修复GPU核心频率 - 确保在合理范围内
                        if (cachedGpuCoreFreq > 0 && cachedGpuCoreFreq < 10000) {
                            gpu.coreClock = cachedGpuCoreFreq;
                        } else {
                            gpu.coreClock = 0; // 设置为0而不是异常值
                            if (isFirstRun && cachedGpuCoreFreq > 10000) {
                                Logger::Warn("GPU核心频率异常: " + std::to_string(cachedGpuCoreFreq) + "MHz，已重置为0");
                            }
                        }
                        
                        gpu.isVirtual = cachedGpuIsVirtual;
                        
                        sysInfo.gpus.push_back(gpu);
                        
                        if (isFirstRun) {
                            Logger::Debug("已添加GPU到数组: " + cachedGpuName + 
                                         " (内存: " + FormatSize(cachedGpuMemory) + 
                                         ", 频率: " + std::to_string(gpu.coreClock) + "MHz" +
                                         ", 虚拟: " + (cachedGpuIsVirtual ? "是" : "否") + ")");
                        }
                    } else {
                        if (isFirstRun) {
                            Logger::Debug("未检测到有效GPU，跳过GPU数据填充");
                        }
                    }
                }
                catch (const std::bad_alloc& e) {
                    Logger::Error("GPU缓存信息处理失败 - 内存不足: " + std::string(e.what()));
                    // 清空GPU数据以避免显示错误信息
                    sysInfo.gpus.clear();
                    sysInfo.gpuName = "内存不足";
                    sysInfo.gpuBrand = "未知";
                    sysInfo.gpuMemory = 0;
                    sysInfo.gpuCoreFreq = 0;
                    sysInfo.gpuIsVirtual = false;
                }
                catch (const std::exception& e) {
                    Logger::Error("获取GPU缓存信息失败: " + std::string(e.what()));
                    // 清空GPU数据以避免显示错误信息
                    sysInfo.gpus.clear();
                    sysInfo.gpuName = "GPU信息获取失败";
                    sysInfo.gpuBrand = "未知";
                    sysInfo.gpuMemory = 0;
                    sysInfo.gpuCoreFreq = 0;
                    sysInfo.gpuIsVirtual = false;
                }
                catch (...) {
                    Logger::Error("获取GPU缓存信息失败 - 未知异常");
                    sysInfo.gpus.clear();
                    sysInfo.gpuName = "未知异常";
                    sysInfo.gpuBrand = "未知";
                    sysInfo.gpuMemory = 0;
                    sysInfo.gpuCoreFreq = 0;
                    sysInfo.gpuIsVirtual = false;
                }

                // 初始化网络适配器信息（避免无效数据导致崩溃）
                sysInfo.networkAdapterName = "未检测到网络适配器";
                sysInfo.networkAdapterMac = "00-00-00-00-00-00";
                sysInfo.networkAdapterSpeed = 0;
                sysInfo.networkAdapterIp = "N/A"; // 添加默认IP地址
                sysInfo.networkAdapterType = "未知"; // 添加默认网卡类型

                // 填充所有网络适配器信息
                try {
                    sysInfo.adapters.clear();
                    NetworkAdapter netAdapter(*wmiManager);
                    const auto& adapters = netAdapter.GetAdapters();
                    if (!adapters.empty()) {
                        for (const auto& adapter : adapters) {
                            NetworkAdapterData data;
                            // 将std::string转换为std::wstring
                            std::wstring nameW(adapter.name.begin(), adapter.name.end());
                            std::wstring macW(adapter.mac.begin(), adapter.mac.end());
                            std::wstring ipW(adapter.ip.begin(), adapter.ip.end());
                            std::wstring typeW(adapter.adapterType.begin(), adapter.adapterType.end());
                            
                            // 复制到wchar_t数组
                            wcsncpy_s(data.name, _countof(data.name), nameW.c_str(), _TRUNCATE);
                            wcsncpy_s(data.mac, _countof(data.mac), macW.c_str(), _TRUNCATE);
                            wcsncpy_s(data.ipAddress, _countof(data.ipAddress), ipW.c_str(), _TRUNCATE); // 添加IP地址
                            wcsncpy_s(data.adapterType, _countof(data.adapterType), typeW.c_str(), _TRUNCATE); // 添加网卡类型
                            data.speed = adapter.speed;
                            sysInfo.adapters.push_back(data);
                        }
                        // 兼容旧字段，取第一个适配器
                        sysInfo.networkAdapterName = adapters[0].name;
                        sysInfo.networkAdapterMac = adapters[0].mac;
                        sysInfo.networkAdapterIp = adapters[0].ip; // 添加IP地址
                        sysInfo.networkAdapterType = adapters[0].adapterType; // 添加网卡类型
                        sysInfo.networkAdapterSpeed = adapters[0].speed;
                    } else {
                        sysInfo.networkAdapterName = "未检测到网络适配器";
                        sysInfo.networkAdapterMac = "00-00-00-00-00-00";
                        sysInfo.networkAdapterIp = "N/A"; // 添加默认IP地址
                        sysInfo.networkAdapterType = "未知"; // 添加默认网卡类型
                        sysInfo.networkAdapterSpeed = 0;
                    }
                } catch (const std::bad_alloc& e) {
                    Logger::Error("获取网络适配器信息失败 - 内存不足: " + std::string(e.what()));
                    sysInfo.adapters.clear();
                    sysInfo.networkAdapterName = "内存不足";
                    sysInfo.networkAdapterMac = "00-00-00-00-00-00";
                    sysInfo.networkAdapterIp = "N/A"; 
                    sysInfo.networkAdapterType = "未知";
                    sysInfo.networkAdapterSpeed = 0;
                } catch (const std::exception& e) {
                    Logger::Error("获取网络适配器信息失败: " + std::string(e.what()));
                    sysInfo.adapters.clear();
                    sysInfo.networkAdapterName = "未检测到网络适配器";
                    sysInfo.networkAdapterMac = "00-00-00-00-00-00";
                    sysInfo.networkAdapterIp = "N/A"; // 添加默认IP地址
                    sysInfo.networkAdapterType = "未知"; // 添加默认网卡类型
                    sysInfo.networkAdapterSpeed = 0;
                } catch (...) {
                    Logger::Error("获取网络适配器信息失败 - 未知异常");
                    sysInfo.adapters.clear();
                    sysInfo.networkAdapterName = "未知异常";
                    sysInfo.networkAdapterMac = "00-00-00-00-00-00";
                    sysInfo.networkAdapterIp = "N/A";
                    sysInfo.networkAdapterType = "未知";
                    sysInfo.networkAdapterSpeed = 0;
                }

                // 添加温度数据采集（每次循环都获取以确保数据实时性）
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
                }
                catch (const std::bad_alloc& e) {
                    Logger::Error("获取温度数据失败 - 内存不足: " + std::string(e.what()));
                    // 清空温度数据以避免显示过时数据
                    sysInfo.temperatures.clear();
                    sysInfo.cpuTemperature = 0;
                    sysInfo.gpuTemperature = 0;
                }
                catch (const std::exception& e) {
                    Logger::Error("获取温度数据失败: " + std::string(e.what()));
                    // 清空温度数据以避免显示过时数据
                    sysInfo.temperatures.clear();
                    sysInfo.cpuTemperature = 0;
                    sysInfo.gpuTemperature = 0;
                }
                catch (...) {
                    Logger::Error("获取温度数据失败 - 未知异常");
                    sysInfo.temperatures.clear();
                    sysInfo.cpuTemperature = 0;
                    sysInfo.gpuTemperature = 0;
                }

                // 添加USB设备数据采集
                try {
                    sysInfo.usbDevices.clear();
                    
                    // 通过SharedMemoryManager获取USB设备信息
                    if (SharedMemoryManager::GetBuffer()) {
                        // USB数据会在SharedMemoryManager中处理
                        // 这里只是为了保持数据结构的一致性
                    }
                }
                catch (const std::bad_alloc& e) {
                    Logger::Error("获取USB设备数据失败 - 内存不足: " + std::string(e.what()));
                    sysInfo.usbDevices.clear();
                }
                catch (const std::exception& e) {
                    Logger::Error("获取USB设备数据失败: " + std::string(e.what()));
                    sysInfo.usbDevices.clear();
                }
                catch (...) {
                    Logger::Error("获取USB设备数据失败 - 未知异常");
                    sysInfo.usbDevices.clear();
                }

                // 采集TPM信息（TBS优先，WMI补充），并添加Debug日志
                try {
                    TpmInfo tpm(*wmiManager);
                    const auto& td = tpm.GetTpmData();

                    sysInfo.hasTpm = tpm.HasTpm();
                    sysInfo.tpmManufacturer = td.manufacturerName;
                    sysInfo.tpmManufacturerId = td.manufacturerId;
                    sysInfo.tpmVersion = td.version;
                    sysInfo.tpmFirmwareVersion = td.firmwareVersion;
                    sysInfo.tpmStatus = td.status;
                    sysInfo.tpmEnabled = td.isEnabled;
                    sysInfo.tpmIsActivated = td.isActivated;
                    sysInfo.tpmIsOwned = td.isOwned;
                    sysInfo.tpmReady = td.isReady;
                    sysInfo.tpmTbsAvailable = td.tbsAvailable;
                    sysInfo.tpmPhysicalPresenceRequired = td.physicalPresenceRequired;
                    sysInfo.tpmSpecVersion = td.specVersion;
                    sysInfo.tpmTbsVersion = td.tbsVersion;
                    sysInfo.tpmErrorMessage = td.errorMessage;
                    sysInfo.tmpDetectionMethod = td.detectionMethod;
                    sysInfo.tmpWmiDetectionWorked = td.wmiDetectionWorked;
                    sysInfo.tmpTbsDetectionWorked = td.tbsDetectionWorked;

                    if (isFirstRun) {
                        std::stringstream ss;
                        ss << "TPM: hasTpm=" << (sysInfo.hasTpm ? "true" : "false")
                           << ", method=" << sysInfo.tmpDetectionMethod
                           << ", manufacturer=" << sysInfo.tpmManufacturer
                           << ", mfrId=" << sysInfo.tpmManufacturerId
                           << ", version=" << sysInfo.tpmVersion
                           << ", fw=" << sysInfo.tpmFirmwareVersion
                           << ", status=" << sysInfo.tpmStatus
                           << ", enabled=" << (sysInfo.tpmEnabled ? "true" : "false")
                           << ", activated=" << (sysInfo.tpmIsActivated ? "true" : "false")
                           << ", owned=" << (sysInfo.tpmIsOwned ? "true" : "false")
                           << ", ready=" << (sysInfo.tpmReady ? "true" : "false")
                           << ", tbsAvail=" << (sysInfo.tpmTbsAvailable ? "true" : "false")
                           << ", tbsVer=" << sysInfo.tpmTbsVersion
                           << ", spec=" << sysInfo.tpmSpecVersion
                           << ", physPresence=" << (sysInfo.tpmPhysicalPresenceRequired ? "true" : "false")
                           << ", error=" << sysInfo.tpmErrorMessage;
                        Logger::Debug(ss.str());
                    }
                }
                catch (const std::exception& e) {
                    Logger::Error("TPM 信息采集失败: " + std::string(e.what()));
                }
                catch (...) {
                    Logger::Error("TPM 信息采集失败 - 未知异常");
                }


                // 写入共享内存前验证数据 - 增强数据验证
                try {
                    // CPU使用率验证
                    if (sysInfo.cpuUsage < 0.0 || sysInfo.cpuUsage > 100.0) {
                        Logger::Warn("CPU使用率数据异常: " + std::to_string(sysInfo.cpuUsage) + "%, 重置为0");
                        sysInfo.cpuUsage = 0.0;
                    }
                    
                    // 内存数据验证
                    if (sysInfo.totalMemory > 0) {
                        if (sysInfo.usedMemory > sysInfo.totalMemory) {
                            Logger::Warn("已用内存超过总内存，数据异常");
                            sysInfo.usedMemory = sysInfo.totalMemory;
                        }
                        if (sysInfo.availableMemory > sysInfo.totalMemory) {
                            Logger::Warn("可用内存超过总内存，数据异常");
                            sysInfo.availableMemory = sysInfo.totalMemory;
                        }
                    }
                    
                    // 频率数据验证
                    if (std::isnan(sysInfo.performanceCoreFreq) || std::isinf(sysInfo.performanceCoreFreq)) {
                        sysInfo.performanceCoreFreq = 0.0;
                    }
                    if (std::isnan(sysInfo.efficiencyCoreFreq) || std::isinf(sysInfo.efficiencyCoreFreq)) {
                        sysInfo.efficiencyCoreFreq = 0.0;
                    }
                    if (std::isnan(sysInfo.gpuCoreFreq) || std::isinf(sysInfo.gpuCoreFreq)) {
                        sysInfo.gpuCoreFreq = 0.0;
                    }
                    
                    // 温度数据验证
                    if (std::isnan(sysInfo.cpuTemperature) || std::isinf(sysInfo.cpuTemperature)) {
                        sysInfo.cpuTemperature = 0.0;
                    }
                    if (std::isnan(sysInfo.gpuTemperature) || std::isinf(sysInfo.gpuTemperature)) {
                        sysInfo.gpuTemperature = 0.0;
                    }
                    
                    // 网络速度验证
                    if (sysInfo.networkAdapterSpeed > 1000000000000ULL) // 大于1TB/s可能异常
                        Logger::Warn("网络适配器速度异常: " + std::to_string(sysInfo.networkAdapterSpeed));
                        sysInfo.networkAdapterSpeed = 0;
                }
                catch (const std::exception& e) {
                    Logger::Error("数据验证过程中发生异常: " + std::string(e.what()));
                }
                catch (...) {
                    Logger::Error("数据验证过程中发生未知异常");
                }

                // 写入共享内存 - 增强异常处理
                try {
                    if (SharedMemoryManager::GetBuffer()) {
                        SharedMemoryManager::WriteToSharedMemory(sysInfo);
                        if (isDetailedLogging) {
                            Logger::Debug("成功更新共享内存");
                        }
                    } else {
                        Logger::Error("共享内存缓冲区不可用");
                        // 尝试重新初始化
                        if (SharedMemoryManager::InitSharedMemory()) {
                            SharedMemoryManager::WriteToSharedMemory(sysInfo);
                            if (isDetailedLogging) {
                                Logger::Info("重新初始化并更新共享内存");
                            }
                        } else {
                            Logger::Error("重新初始化共享内存失败: " + SharedMemoryManager::GetLastError());
                        }
                    }
                    
                    if (isDetailedLogging) {
                        Logger::Debug("系统信息已更新到共享内存");
                    }
                }
                catch (const std::bad_alloc& e) {
                    Logger::Error("处理系统信息时内存不足: " + std::string(e.what()));
                }
                catch (const std::exception& e) {
                    Logger::Error("处理系统信息时发生异常: " + std::string(e.what()));
                }
                catch (...) {
                    Logger::Error("处理系统信息时发生未知异常");
                }

                // 计算循环执行时间并自适应休眠 - 优化刷新速度，增强异常处理
                try {
                    auto loopEnd = std::chrono::high_resolution_clock::now();
                    auto loopDuration = std::chrono::duration_cast<std::chrono::milliseconds>(loopEnd - loopStart);
                    
                    // 1秒循环时间
                    int targetCycleTime = 1000;
                    int sleepTime = (std::max)(targetCycleTime - static_cast<int>(loopDuration.count()), 100); // 最少休眠100ms
                    
                    if (isDetailedLogging) {
                        // 将毫秒转换为秒，保留2位小数
                        double loopTimeSeconds = loopDuration.count() / 1000.0;
                        double sleepTimeSeconds = sleepTime / 1000.0;
                        
                        // 验证计算结果的合理性
                        if (loopTimeSeconds < 0 || loopTimeSeconds > 60) {
                            Logger::Warn("循环时间计算异常: " + std::to_string(loopTimeSeconds) + "秒");
                        }
                        
                        std::stringstream ss;
                        ss << std::fixed << std::setprecision(2);
                        ss << "主监控循环第 #" << loopCounter << " 次执行耗时 " 
                           << loopTimeSeconds << "秒，将休眠 " << sleepTimeSeconds << "秒";
                        
                        Logger::Debug(ss.str());
                    }
                    
                    // 休眠时检查退出标志 - 使用更短的检查间隔提升响应性
                    auto sleepStart = std::chrono::high_resolution_clock::now();
                    while (!g_shouldExit.load()) {
                        try {
                            auto now = std::chrono::high_resolution_clock::now();
                            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - sleepStart);
                            if (elapsed.count() >= sleepTime) {
                                break;
                            }
                            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 50ms检查间隔
                        }
                        catch (const std::exception& e) {
                            Logger::Warn("休眠过程中发生异常: " + std::string(e.what()));
                            break; // 退出休眠循环
                        }
                        catch (...) {
                            Logger::Warn("休眠过程中发生未知异常");
                            break;
                        }
                    }
                }
                catch (const std::exception& e) {
                    Logger::Error("计算循环时间时发生异常: " + std::string(e.what()));
                    // 使用默认休眠时间
                    try {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    } catch (...) {
                        // 如果连休眠都失败，记录错误但继续执行
                        Logger::Fatal("系统休眠功能异常");
                    }
                }
                catch (...) {
                    Logger::Error("计算循环时间时发生未知异常");
                    try {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    } catch (...) {
                        Logger::Fatal("系统休眠功能异常");
                    }
                }
                
                // 安全增加循环计数器
                try {
                    loopCounter++;
                    
                    // 防止循环计数器溢出（虽然几乎不可能发生）
                    if (loopCounter < 0 || loopCounter > 2000000000) {
                        Logger::Warn("循环计数器异常，重置为1");
                        loopCounter = 1;
                    }
                }
                catch (...) {
                    Logger::Error("循环计数器更新失败");
                    loopCounter = 1; // 重置为安全值
                }
                
                // 首次运行后设置标志
                if (isFirstRun) {
                    isFirstRun = false;
                }
            }
            catch (const std::bad_alloc& e) {
                Logger::Critical("主循环中发生内存分配异常: " + std::string(e.what()));
                // 继续循环而不是退出，增强程序稳定性
                try {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000)); // 更长的休眠以缓解内存压力
                } catch (...) {
                    Logger::Fatal("无法执行休眠，系统严重异常");
                }
                continue;
            }
            catch (const std::exception& e) {
                Logger::Critical("主循环中发生异常: " + std::string(e.what()));
                // 继续循环而不是退出，增强程序稳定性
                try {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                } catch (...) {
                    Logger::Fatal("无法执行休眠，系统严重异常");
                }
                continue;
            }
            catch (...) {
                Logger::Fatal("主循环中发生未知异常");
                // 继续循环而不是退出，增强程序稳定性
                try {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                } catch (...) {
                    // 如果连休眠都失败，程序可能处于严重异常状态
                    SafeExit(1);
                }
                continue;
            }
        }
        
        Logger::Info("程序收到退出信号，开始清理");
        SafeExit(0);
    }
    catch (const std::exception& e) {
        Logger::Fatal("程序发生致命错误: " + std::string(e.what()));
        SafeExit(1);
    }
    catch (...) {
        Logger::Fatal("程序发生未知致命错误");
        SafeExit(1);
    }
}

// 检查是否有按键输入（非阻塞）- 增强异常处理
bool CheckForKeyPress() {
    try {
        return _kbhit() != 0;
    }
    catch (const std::exception& e) {
        Logger::Warn("检查按键输入时发生异常: " + std::string(e.what()));
        return false;
    }
    catch (...) {
        Logger::Warn("检查按键输入时发生未知异常");
        return false;
    }
}

// 获取按键（非阻塞）- 增强异常处理
char GetKeyPress() {
    try {
        if (_kbhit()) {
            char key = _getch();
            // 验证按键值的合理性
            if (key >= 0 && key <= 127) // ASCII范围
                return key;
            else
                Logger::Warn("检测到异常按键值: " + std::to_string(static_cast<int>(key)));
        }
    }
    catch (const std::exception& e) {
        Logger::Warn("获取按键时发生异常: " + std::string(e.what()));
    }
    catch (...) {
        Logger::Warn("获取按键时发生未知异常");
    }
    return 0;
}
