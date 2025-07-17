// 使用#pragma unmanaged确保main函数编译为本机代码
#pragma unmanaged

// ✅ 添加栈大小控制，防止栈溢出
#pragma comment(linker, "/STACK:8388608")  // 设置栈大小为8MB

/*
如果出现
警告	MSB8077	某些文件设置为 C++/CLI，但未定义"为单个文件启用 CLR 支持"属性。有关更多详细信息，请参阅"高级属性页"文档。
以上警告请忽视，这个项目的结构没法兼容这个情况
*/
// 首先包含Windows头文件以避免宏重定义警告
#include <windows.h>
#include <shellapi.h>
#include <sddl.h>
#include <Aclapi.h>
#include <conio.h>   // 添加键盘输入支持

// 然后包含标准库头文件
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <utility>
#include <thread>
#include <io.h>
#include <fcntl.h>
#include <algorithm> // Include for std::transform
#include <vector> // Include for std::vector
#include <mutex>     // 添加线程同步支持
#include <atomic>    // 添加原子操作支持
#include <locale>   // 添加locale支持以使用setlocale

// 最后包含项目头文件
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
#include "core/DataStruct/DataStruct.h"
#include "core/DataStruct/SharedMemoryManager.h"  // Include the new shared memory manager
#include "core/temperature/TemperatureWrapper.h"  // 使用TemperatureWrapper而不是直接调用LibreHardwareMonitorBridge

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")

// 全局变量
std::atomic<bool> g_shouldExit{false};
static std::atomic<bool> g_monitoringStarted{false};
static std::atomic<bool> g_comInitialized{false};

// 线程安全的控制台输出互斥锁
static std::mutex g_consoleMutex;

// 函数声明
bool CheckForKeyPress();
char GetKeyPress();
void SafeExit(int exitCode);

// 线程安全的控制台输出函数
void SafeConsoleOutput(const std::string& message);
void SafeConsoleOutput(const std::string& message, int color);

// 信号处理函数 - 简化版本
BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        Logger::Info("接收到系统关闭信号，正在安全退出...");
        g_shouldExit = true;
        SafeConsoleOutput("正在退出程序...\n", 14);
        return TRUE;
    }
    return FALSE;
}

// 线程安全的控制台输出函数实现
void SafeConsoleOutput(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_consoleMutex);
    try {
        // 统一使用UTF-8编码输出，确保中文显示正确
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole != INVALID_HANDLE_VALUE) {
            // 确保输入字符串不为空
            if (message.empty()) {
                return;
            }
            
            // 将UTF-8字符串转换为UTF-16 (Wide Character)
            int wideLength = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, nullptr, 0);
            if (wideLength > 0) {
                std::vector<wchar_t> wideMessage(wideLength);
                if (MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, wideMessage.data(), wideLength)) {
                    // 使用WriteConsoleW直接输出Unicode文本
                    DWORD written;
                    WriteConsoleW(hConsole, wideMessage.data(), static_cast<DWORD>(wideLength - 1), &written, NULL);
                    return;
                }
            }
            
            // 如果UTF-8转换失败，回退到ASCII输出
            DWORD written;
            WriteConsoleA(hConsole, message.c_str(), static_cast<DWORD>(message.length()), &written, NULL);
        }
    }
    catch (...) {
        // 忽略控制台输出错误，避免递归异常
    }
}

void SafeConsoleOutput(const std::string& message, int color) {
    std::lock_guard<std::mutex> lock(g_consoleMutex);
    try {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole != INVALID_HANDLE_VALUE) {
            // 保存原始颜色
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            GetConsoleScreenBufferInfo(hConsole, &csbi);
            WORD originalColor = csbi.wAttributes;
            
            // 设置新颜色
            SetConsoleTextAttribute(hConsole, color);
            
            // 确保输入字符串不为空
            if (!message.empty()) {
                // 将UTF-8字符串转换为UTF-16 (Wide Character)
                int wideLength = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, nullptr, 0);
                if (wideLength > 0) {
                    std::vector<wchar_t> wideMessage(wideLength);
                    if (MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, wideMessage.data(), wideLength)) {
                        // 使用WriteConsoleW直接输出Unicode文本
                        DWORD written;
                        WriteConsoleW(hConsole, wideMessage.data(), static_cast<DWORD>(wideLength - 1), &written, NULL);
                    } else {
                        // 如果转换失败，回退到ASCII输出
                        DWORD written;
                        WriteConsoleA(hConsole, message.c_str(), static_cast<DWORD>(message.length()), &written, NULL);
                    }
                }
            }
            
            // 恢复原始颜色
            SetConsoleTextAttribute(hConsole, originalColor);
        }
    }
    catch (...) {
        // 忽略控制台输出错误，避免递归异常
    }
}

// 安全退出函数
void SafeExit(int exitCode) {
    try {
        Logger::Info("开始程序清理流程");
        
        // 设置退出标志
        g_shouldExit = true;
        
        // 清理硬件监控桥接
        try {
            TemperatureWrapper::Cleanup();
            Logger::Debug("硬件监控桥接清理完成");
        }
        catch (const std::exception& e) {
            Logger::Error("清理硬件监控桥接时发生错误: " + std::string(e.what()));
        }
        
        // 清理共享内存
        try {
            SharedMemoryManager::CleanupSharedMemory();
            Logger::Debug("共享内存清理完成");
        }
        catch (const std::exception& e) {
            Logger::Error("清理共享内存时发生错误: " + std::string(e.what()));
        }
        
        // 清理COM
        if (g_comInitialized.load()) {
            try {
                CoUninitialize();
                g_comInitialized = false;
                Logger::Debug("COM清理完成");
            }
            catch (...) {
                Logger::Error("清理COM时发生未知错误");
            }
        }
        
        Logger::Info("程序清理完成，退出码: " + std::to_string(exitCode));
        
        // 给日志系统一点时间完成写入
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    catch (...) {
        // 最后的异常处理，避免在清理过程中崩溃
    }
    
    exit(exitCode);
}

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
    try {
        auto time = std::chrono::system_clock::to_time_t(tp);
        struct tm timeinfo;
        if (localtime_s(&timeinfo, &time) == 0) {  // 检查返回值
            std::stringstream ss;
            ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
            return ss.str();
        }
    }
    catch (...) {
        // 时间格式化失败时返回默认值
    }
    return "时间格式化失败";
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
    SafeConsoleOutput("\n=== " + title + " ===\n", 14); // 黄色
}

static void PrintInfoItem(const std::string& label, const std::string& value, int indent = 2) {
    std::string line = std::string(indent, ' ') + label;
    // 格式化为固定宽度
    if (line.length() < 27) {
        line += std::string(27 - line.length(), ' ');
    }
    line += ": " + value + "\n";
    SafeConsoleOutput(line);
}

//主要函数
// 检查是否以管理员身份运行
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

// 线程安全的GPU信息缓存类
class ThreadSafeGpuCache {
private:
    mutable std::mutex mtx_;
    bool initialized_ = false;
    std::string cachedGpuName_ = "未检测到GPU";
    std::string cachedGpuBrand_ = "未知";
    uint64_t cachedGpuMemory_ = 0;
    uint32_t cachedGpuCoreFreq_ = 0;
    bool cachedGpuIsVirtual_ = false;

public:
    void Initialize(WmiManager& wmiManager) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (initialized_) return;
        
        try {
            Logger::Info("正在初始化GPU信息");
            
            GpuInfo gpuInfo(wmiManager);
            const auto& gpus = gpuInfo.GetGpuData();
            
            // 记录所有检测到的GPU
            for (const auto& gpu : gpus) {
                std::string gpuName = WinUtils::WstringToString(gpu.name);
                Logger::Info("检测到GPU: " + gpuName + 
                           " (虚拟: " + (gpu.isVirtual ? "是" : "否") + 
                           ", NVIDIA: " + (gpuName.find("NVIDIA") != std::string::npos ? "是" : "否") + 
                           ", 集成: " + (gpuName.find("Intel") != std::string::npos ||
                                       gpuName.find("AMD") != std::string::npos ? "是" : "否") + ")");
            }
            
            // 优先选择非虚拟GPU
            const GpuInfo::GpuData* selectedGpu = nullptr;
            for (const auto& gpu : gpus) {
                if (!gpu.isVirtual) {
                    selectedGpu = &gpu;
                    break;
                }
            }
            
            // 如果没有非虚拟GPU，选择第一个GPU
            if (!selectedGpu && !gpus.empty()) {
                selectedGpu = &gpus[0];
            }
            
            if (selectedGpu) {
                cachedGpuName_ = WinUtils::WstringToString(selectedGpu->name);
                cachedGpuBrand_ = GetGpuBrand(selectedGpu->name);
                cachedGpuMemory_ = selectedGpu->dedicatedMemory;
                cachedGpuCoreFreq_ = static_cast<uint32_t>(selectedGpu->coreClock);
                cachedGpuIsVirtual_ = selectedGpu->isVirtual;
                
                Logger::Info("选择主GPU: " + cachedGpuName_ + 
                           " (虚拟: " + (cachedGpuIsVirtual_ ? "是" : "否") + ")");
            } else {
                Logger::Warn("未检测到任何GPU");
            }
            
            initialized_ = true;
            Logger::Info("GPU信息初始化完成，后续循环将使用缓存信息");
        }
        catch (const std::exception& e) {
            Logger::Error("GPU信息初始化失败: " + std::string(e.what()));
            // 保持默认值
            initialized_ = true; // 标记为已初始化以避免重复尝试
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
}; // 添加缺少的分号

// 主函数 - 控制台模式
int main(int argc, char* argv[]) {
    // 设置控制台编码为UTF-8，确保中文显示正确
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
    
    // 设置本地化支持UTF-8
    setlocale(LC_ALL, "en_US.UTF-8");
    
    // 设置控制台信号处理器
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    
    try {
        // 初始化日志系统
        try {
            Logger::EnableConsoleOutput(true); // Enable console output for Logger
            Logger::Initialize("system_monitor.log");
            Logger::SetLogLevel(LOG_DEBUG); // 设置日志等级为DEBUG，查看详细信息
            Logger::Info("程序启动");
        }
        catch (const std::exception& e) {
            printf("日志系统初始化失败: %s\n", e.what());
            return 1;
        }

        // 检查管理员权限
        if (!IsRunAsAdmin()) {
            wchar_t szPath[MAX_PATH];
            GetModuleFileNameW(NULL, szPath, MAX_PATH);

            // 以管理员权限重启自身
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.lpVerb = L"runas";
            sei.lpFile = szPath;
            sei.hwnd = NULL;
            sei.nShow = SW_NORMAL;

            if (ShellExecuteExW(&sei)) {
                // 启动成功，退出当前进程
                exit(0);
            } else {
                // 启动失败，弹窗提示
                MessageBoxW(NULL, L"自动提权失败，请右键以管理员身份运行。", L"权限不足", MB_OK | MB_ICONERROR);
                SafeExit(1);
            }
        }

        // 安全初始化COM为多线程模式
        try {
            HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (FAILED(hr)) {
                if (hr == RPC_E_CHANGED_MODE) {
                    Logger::Warn("COM初始化模式冲突: 线程已初始化为不同的模式，尝试单线程模式");
                    // 尝试单线程模式
                    hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                    if (FAILED(hr)) {
                        Logger::Error("COM初始化失败: 0x" + std::to_string(hr));
                        return -1;
                    }
                }
                else {
                    Logger::Error("COM初始化失败: 0x" + std::to_string(hr));
                    return -1;
                }
            }
            g_comInitialized = true;
            Logger::Debug("COM初始化成功");
        }
        catch (const std::exception& e) {
            Logger::Error("COM初始化过程中发生异常: " + std::string(e.what()));
            return -1;
        }

        // 初始化共享内存 - 增强错误处理
        try {
            if (!SharedMemoryManager::InitSharedMemory()) {
                std::string error = SharedMemoryManager::GetLastError();
                Logger::Error("共享内存初始化失败: " + error);
                
                // 尝试重新初始化
                Logger::Info("尝试重新初始化共享内存...");
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                
                if (!SharedMemoryManager::InitSharedMemory()) {
                    Logger::Critical("共享内存重新初始化失败，程序无法继续运行");
                    SafeExit(1);
                }
            }
            Logger::Info("共享内存初始化成功");
        }
        catch (const std::exception& e) {
            Logger::Error("共享内存初始化过程中发生异常: " + std::string(e.what()));
            SafeExit(1);
        }

        // 创建WMI管理器并初始化
        std::unique_ptr<WmiManager> wmiManager;
        try {
            wmiManager = std::make_unique<WmiManager>();
            if (!wmiManager->IsInitialized()) {
                Logger::Error("WMI初始化失败");
                MessageBoxA(NULL, "WMI初始化失败，无法获取系统信息。", "错误", MB_OK | MB_ICONERROR);
                SafeExit(1);
            }
            Logger::Debug("WMI管理器初始化成功");
        }
        catch (const std::exception& e) {
            Logger::Error("WMI管理器创建失败: " + std::string(e.what()));
            SafeExit(1);
        }

        // 初始化硬件监控桥接
        try {
            TemperatureWrapper::Initialize();
            Logger::Debug("硬件监控桥接初始化成功");
        }
        catch (const std::exception& e) {
            Logger::Error("硬件监控桥接初始化失败: " + std::string(e.what()));
            // 不退出程序，继续运行但温度数据可能不可用
        }

        Logger::Info("程序启动完成");
        
        // 初始化循环计数器，减少频繁的日志记录
        int loopCounter = 1; // 从1开始计数，更符合人类习惯
        bool isFirstRun = true; // 首次运行标志
        
        // 缓存静态系统信息（只在首次获取）
        static std::atomic<bool> systemInfoCached{false};
        static std::string cachedOsVersion;
        static std::string cachedCpuName;
        static uint32_t cachedPhysicalCores = 0;
        static uint32_t cachedLogicalCores = 0;
        static uint32_t cachedPerformanceCores = 0;
        static uint32_t cachedEfficiencyCores = 0;
        static bool cachedHyperThreading = false;
        static bool cachedVirtualization = false;
        
        // 创建CPU对象一次，重复使用（避免重复初始化性能计数器）
        std::unique_ptr<CpuInfo> cpuInfo;
        try {
            cpuInfo = std::make_unique<CpuInfo>();
            Logger::Debug("CPU信息对象创建成功");
        }
        catch (const std::exception& e) {
            Logger::Error("CPU信息对象创建失败: " + std::string(e.what()));
            SafeExit(1);
        }
        
        // 线程安全的GPU缓存
        ThreadSafeGpuCache gpuCache;
        
        while (!g_shouldExit.load()) {
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
                
                // 安全初始化所有字段以避免未定义行为
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
                    GetSystemTime(&sysInfo.lastUpdate); // 设置当前时间
                }
                catch (const std::exception& e) {
                    Logger::Error("SystemInfo初始化失败: " + std::string(e.what()));
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
                    if (!cachedGpuName.empty() && cachedGpuName != "未检测到GPU") {
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
                        
                        wcsncpy_s(gpu.name, sizeof(gpu.name)/sizeof(wchar_t), gpuNameW.c_str(), _TRUNCATE);
                        wcsncpy_s(gpu.brand, sizeof(gpu.brand)/sizeof(wchar_t), gpuBrandW.c_str(), _TRUNCATE);
                        
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

                // 初始化网络适配器信息（避免无效数据导致崩溃）
                sysInfo.networkAdapterName = "未检测到网络适配器";
                sysInfo.networkAdapterMac = "00-00-00-00-00-00";
                sysInfo.networkAdapterSpeed = 0;

                // 添加温度数据采集（每次循环都获取以确保数据实时性）
                try {
                    auto temperatures = TemperatureWrapper::GetTemperatures();
                    sysInfo.temperatures.clear();
                    for (const auto& temp : temperatures) {
                        sysInfo.temperatures.push_back({temp.first, temp.second});
                    }
                    if (isFirstRun) {
                        Logger::Debug("收集到 " + std::to_string(temperatures.size()) + " 个温度读数");
                        // 添加详细的温度传感器信息输出
                        for (const auto& temp : temperatures) {
                            Logger::Debug("温度传感器: " + temp.first + " = " + std::to_string(temp.second) + "°C");
                        }
                    }
                }
                catch (const std::exception& e) {
                    Logger::Error("获取温度数据失败: " + std::string(e.what()));
                    // 清空温度数据以避免显示过时数据
                    sysInfo.temperatures.clear();
                }

                // 添加磁盘信息采集（每次循环都获取以确保数据实时性）
                try {
                    DiskInfo diskInfo;
                    auto disks = diskInfo.GetDisks();
                    
                    // Validate disk data
                    if (disks.size() > 8) {
                        Logger::Error("磁盘数量超过最大允许值（8）。跳过磁盘数据更新。");
                        sysInfo.disks.clear(); // 清空磁盘数据
                    } else {
                        sysInfo.disks = disks;
                        if (isFirstRun) {
                            Logger::Debug("收集到 " + std::to_string(disks.size()) + " 个磁盘条目");
                        }

                        if (isFirstRun) {
                            for (size_t i = 0; i < disks.size(); ++i) {
                                const auto& disk = disks[i];
                                Logger::Debug("磁盘 " + std::to_string(i) + ": 标签=" + disk.label +
                                             ", 文件系统=" + disk.fileSystem);
                            }
                        }
                    }
                }
                catch (const std::exception& e) {
                    Logger::Error("获取磁盘数据失败: " + std::string(e.what()));
                    sysInfo.disks.clear(); // 清空磁盘数据
                }

                // 写入共享内存前验证数据
                if (sysInfo.cpuUsage < 0.0 || sysInfo.cpuUsage > 100.0) {
                    Logger::Warn("CPU使用率数据异常: " + std::to_string(sysInfo.cpuUsage) + "%, 重置为0");
                    sysInfo.cpuUsage = 0.0;
                }

                // 写入共享内存
                try {
                    if (SharedMemoryManager::GetBuffer()) {
                        SharedMemoryManager::WriteToSharedMemory(sysInfo);
                        if (isDetailedLogging) {
                            Logger::Debug("成功更新共享内存");
                        }
                    } else {
                        Logger::Critical("共享内存缓冲区不可用");
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
                catch (const std::exception& e) {
                    Logger::Error("处理系统信息时发生异常: " + std::string(e.what()));
                }

                // 计算循环执行时间并自适应休眠 - 优化刷新速度
                auto loopEnd = std::chrono::high_resolution_clock::now();
                auto loopDuration = std::chrono::duration_cast<std::chrono::milliseconds>(loopEnd - loopStart);
                
                // 1秒循环时间
                int targetCycleTime = 1000;
                int sleepTime = (std::max)(targetCycleTime - static_cast<int>(loopDuration.count()), 100); // 最少休眠100ms
                
                if (isDetailedLogging) {
                    // 将毫秒转换为秒，保留2位小数
                    double loopTimeSeconds = loopDuration.count() / 1000.0;
                    double sleepTimeSeconds = sleepTime / 1000.0;
                    
                    std::stringstream ss;
                    ss << std::fixed << std::setprecision(2);
                    ss << "主监控循环第 #" << loopCounter << " 次执行耗时 " 
                       << loopTimeSeconds << "秒，将休眠 " << sleepTimeSeconds << "秒";
                    
                    Logger::Debug(ss.str());
                }
                
                // 休眠时检查退出标志 - 使用更短的检查间隔提升响应性
                auto sleepStart = std::chrono::high_resolution_clock::now();
                while (!g_shouldExit.load()) {
                    auto now = std::chrono::high_resolution_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - sleepStart);
                    if (elapsed.count() >= sleepTime) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 50ms检查间隔
                }
                
                loopCounter++;
                
                // 首次运行后设置标志
                if (isFirstRun) {
                    isFirstRun = false;
                }
            }
            catch (const std::exception& e) {
                Logger::Error("主循环中发生异常: " + std::string(e.what()));
                // 继续循环而不是退出，增强程序稳定性
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                continue;
            }
            catch (...) {
                Logger::Error("主循环中发生未知异常");
                // 继续循环而不是退出，增强程序稳定性
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                continue;
            }
        }
        
        Logger::Info("程序收到退出信号，开始清理");
        SafeExit(0);
    }
    catch (const std::exception& e) {
        Logger::Error("程序发生致命错误: " + std::string(e.what()));
        SafeExit(1);
    }
    catch (...) {
        Logger::Error("程序发生未知致命错误");
        SafeExit(1);
    }
}

// 检查是否有按键输入（非阻塞）
bool CheckForKeyPress() {
    try {
        return _kbhit() != 0;
    }
    catch (...) {
        return false;
    }
}

// 获取按键（非阻塞）
char GetKeyPress() {
    try {
        if (_kbhit()) {
            return _getch();
        }
    }
    catch (...) {
        // 忽略键盘输入错误
    }
    return 0;
}


