#include "CpuInfo.h"
#include "../Utils/Logger.h"
#include "../Utils/CrossPlatformSystemInfo.h"

#ifdef PLATFORM_WINDOWS
    #include <windows.h>
    #include <Pdh.h>
    #include <comdef.h>
    #include <algorithm>
    #include <winternl.h>
    #pragma comment(lib, "Pdh.lib")
    #pragma comment(lib, "ntdll.lib")
    
    // Ensure NTSTATUS is defined
#ifndef NTSTATUS
    typedef LONG NTSTATUS;
#endif
#elif defined(PLATFORM_MACOS)
    #include <mach/mach.h>
    #include <mach/processor_info.h>
    #include <sys/sysctl.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <fstream>
    #include <sstream>
    #include <algorithm>
    #include <CoreFoundation/CoreFoundation.h>
    #include <IOKit/IOKitLib.h>
#elif defined(__linux__)
    #include <fstream>
    #include <sstream>
    #include <algorithm>
    #include <unistd.h>
    #include <sys/types.h>
    #include <dirent.h>
#endif

// Define PDH constants if not available
#ifndef PDH_CSTATUS_VALID_DATA
#define PDH_CSTATUS_VALID_DATA 0x00000000L
#endif
#ifndef PDH_CSTATUS_NEW_DATA
#define PDH_CSTATUS_NEW_DATA 0x00000001L
#endif

#ifdef PLATFORM_WINDOWS
// Ensure NTSTATUS is defined
#ifndef NTSTATUS
    typedef LONG NTSTATUS;
#endif

// Ensure POWER_INFORMATION_LEVEL is defined only if not already defined
#ifndef POWER_INFORMATION_LEVEL
    typedef enum _POWER_INFORMATION_LEVEL {
        ProcessorInformation = 11
    } POWER_INFORMATION_LEVEL;
#endif

// Missing constants
#ifndef STATUS_PROCEDURE_NOT_FOUND
#define STATUS_PROCEDURE_NOT_FOUND ((NTSTATUS)0xC000007AL)
#endif

// 声明ntdll.dll中的函数
typedef NTSTATUS (NTAPI *PFN_CALL_NT_POWER_INFORMATION)(
    POWER_INFORMATION_LEVEL InformationLevel,
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength
);

static NTSTATUS CallNtPowerInformation(
    POWER_INFORMATION_LEVEL InformationLevel,
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength)
{
    static HMODULE hNtDll = LoadLibraryA("ntdll.dll");
    if (!hNtDll) return STATUS_DLL_NOT_FOUND;
    
    static PFN_CALL_NT_POWER_INFORMATION pfnCallNtPowerInformation = 
        (PFN_CALL_NT_POWER_INFORMATION)GetProcAddress(hNtDll, "NtPowerInformation");
    
    if (!pfnCallNtPowerInformation) return STATUS_PROCEDURE_NOT_FOUND;
    
    return pfnCallNtPowerInformation(InformationLevel, InputBuffer, InputBufferLength, OutputBuffer, OutputBufferLength);
}

static bool QueryBaseAndCurrentFrequencyACPI(double& baseMHz, double& currentMHz)
{
    baseMHz = 0; currentMHz = 0;
    typedef struct _PROCESSOR_POWER_INFORMATION { ULONG Number; ULONG MaxMhz; ULONG CurrentMhz; ULONG MhzLimit; ULONG MaxIdleState; ULONG CurrentIdleState; } PROCESSOR_POWER_INFORMATION;
    SYSTEM_INFO si{}; GetSystemInfo(&si);
    ULONG count = si.dwNumberOfProcessors; if (count == 0 || count > 256) return false;
    std::vector<PROCESSOR_POWER_INFORMATION> buf(count);
    auto st = CallNtPowerInformation(ProcessorInformation, nullptr, 0, buf.data(), (ULONG)(buf.size()*sizeof(PROCESSOR_POWER_INFORMATION)));
    if (st == 0) { double sumBase=0,sumCur=0; int n=0; for (ULONG i=0;i<count;++i){ if (buf[i].MaxMhz>0){ sumBase+=buf[i].MaxMhz; sumCur+=buf[i].CurrentMhz; ++n; } } if (n>0){ baseMHz=sumBase/n; currentMHz=sumCur/n; return true; } }
    return false;
}
#else
static bool QueryBaseAndCurrentFrequencyACPI(double& baseMHz, double& currentMHz)
{
    // Non-Windows platforms - not implemented
    baseMHz = 0; currentMHz = 0;
    return false;
}
#endif



#ifdef PLATFORM_WINDOWS
static void QueryBaseAndCurrentFrequencyFallback(double& baseMHz, double& currentMHz)
{
    baseMHz = currentMHz = 0;
    HKEY hKey; DWORD speed=0; DWORD size=sizeof(DWORD);
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"~MHz", NULL, NULL, (LPBYTE)&speed, &size) == ERROR_SUCCESS) {
            baseMHz = currentMHz = (double)speed;
        }
        RegCloseKey(hKey);
    }
}
#else
static void QueryBaseAndCurrentFrequencyFallback(double& baseMHz, double& currentMHz)
{
    // Non-Windows platforms - not implemented
    baseMHz = currentMHz = 0;
}
#endif

void CpuInfo::InitializeFrequencyCounter()
{
    if (freqCounterInitialized) return;
    
#ifdef PLATFORM_WINDOWS
    PDH_STATUS s = PdhOpenQuery(NULL, 0, &freqQueryHandle);
    if (s != ERROR_SUCCESS) { Logger::Warn("无法打开PDH频率查询"); return; }
    // English name to avoid localization: Processor Information(_Total)\\Processor Frequency
    s = PdhAddEnglishCounter(freqQueryHandle, L"\\Processor Information(_Total)\\Processor Frequency", 0, &freqCounterHandle);
    if (s != ERROR_SUCCESS) { Logger::Warn("无法添加频率计数器"); PdhCloseQuery(freqQueryHandle); return; }
    s = PdhCollectQueryData(freqQueryHandle); // prime
    if (s != ERROR_SUCCESS) { Logger::Warn("无法读取频率计数器初值"); PdhCloseQuery(freqQueryHandle); return; }
    freqCounterInitialized = true;
#elif defined(PLATFORM_MACOS)
    // macOS 使用 sysctl 获取频率信息
    freqCounterInitialized = true;
#elif defined(__linux__)
    // Linux 使用 /proc/cpuinfo 获取频率信息
    freqCounterInitialized = true;
#endif
}

void CpuInfo::CleanupFrequencyCounter()
{
    if (freqCounterInitialized) {
        PdhCloseQuery(freqQueryHandle);
        freqCounterInitialized = false;
        freqQueryHandle = nullptr; freqCounterHandle = nullptr;
    }
}

double CpuInfo::updateInstantFrequencyMHz()
{
    // 优先 ACPI
    double base=0, cur=0;
    if (QueryBaseAndCurrentFrequencyACPI(base, cur)) {
        cachedInstantMHz = cur; return cachedInstantMHz;
    }
    // 次选 PDH 频率计数器
    if (!freqCounterInitialized) InitializeFrequencyCounter();
    if (freqCounterInitialized) {
        DWORD now = GetTickCount();
        if (now - lastFreqTick >= 300) { // 300ms 刷新
            lastFreqTick = now;
            PDH_STATUS s = PdhCollectQueryData(freqQueryHandle);
            if (s == ERROR_SUCCESS) {
                PDH_FMT_COUNTERVALUE v{}; s = PdhGetFormattedCounterValue(freqCounterHandle, PDH_FMT_DOUBLE, NULL, &v);
                if (s == ERROR_SUCCESS && (v.CStatus == PDH_CSTATUS_VALID_DATA || v.CStatus == PDH_CSTATUS_NEW_DATA)) {
                    cachedInstantMHz = v.doubleValue; return cachedInstantMHz;
                }
            }
        }
        if (cachedInstantMHz > 0) return cachedInstantMHz;
    }
    // 最后回退注册表
    QueryBaseAndCurrentFrequencyFallback(base, cur);
    cachedInstantMHz = cur; return cachedInstantMHz;
}

#ifdef PLATFORM_WINDOWS
CpuInfo::CpuInfo() :
    totalCores(0),
    largeCores(0),
    smallCores(0),
    cpuUsage(0.0),
    counterInitialized(false),
    lastUpdateTime(0),
    lastSampleTick(0),
    prevSampleTick(0),
    lastSampleIntervalMs(0.0),
    queryHandle(nullptr),
    counterHandle(nullptr),
    freqQueryHandle(nullptr),
    freqCounterHandle(nullptr),
    freqCounterInitialized(false),
    lastFreqTick(0),
    cachedInstantMHz(0.0)
#else
CpuInfo::CpuInfo() :
    totalCores(0),
    largeCores(0),
    smallCores(0),
    cpuUsage(0.0),
    counterInitialized(false),
    lastUpdateTime(0),
    lastSampleTick(0),
    prevSampleTick(0),
    lastSampleIntervalMs(0.0),
    queryHandle(nullptr),
    counterHandle(nullptr),
    freqQueryHandle(nullptr),
    freqCounterHandle(nullptr),
    freqCounterInitialized(false),
    lastFreqTick(0),
    cachedInstantMHz(0.0)
#endif
{

    try {
#ifdef PLATFORM_WINDOWS
        DetectCores();
        cpuName = GetNameFromRegistry();
        InitializeCounter();
        UpdateCoreSpeeds();  // 初始化频率信息
        InitializeFrequencyCounter();     // 初始化即时频率计数器
#elif defined(PLATFORM_MACOS)
        DetectCores();
        cpuName = GetNameFromSysctl();
        InitializeCounter();
        UpdateCoreSpeeds();  // 初始化频率信息
        InitializeFrequencyCounter();     // 初始化即时频率计数器
#elif defined(PLATFORM_LINUX)
        DetectCores();
        cpuName = GetNameFromProcCpuinfo();
        InitializeCounter();
        UpdateCoreSpeeds();  // 初始化频率信息
        InitializeFrequencyCounter();     // 初始化即时频率计数器
#endif
    }
    catch (const std::exception& e) {
        Logger::Error("CPU信息初始化失败: " + std::string(e.what()));
    }
}

CpuInfo::~CpuInfo() {
    CleanupFrequencyCounter();
    CleanupCounter();
}

void CpuInfo::InitializeCounter() {
#ifdef PLATFORM_WINDOWS
    PDH_STATUS status = PdhOpenQuery(NULL, 0, &queryHandle);
    if (status != ERROR_SUCCESS) {
        Logger::Error("无法打开性能计数器查询");
        return;
    }

    // 使用英文计数器名称以避免本地化问题
    status = PdhAddEnglishCounter(queryHandle,
        L"\\Processor(_Total)\\% Processor Time",
        0,
        &counterHandle);

    if (status != ERROR_SUCCESS) {
        Logger::Error("无法添加CPU使用率计数器");
        PdhCloseQuery(queryHandle);
        return;
    }

    // 首次查询以初始化计数器
    status = PdhCollectQueryData(queryHandle);
    if (status != ERROR_SUCCESS) {
        Logger::Error("无法收集性能计数器数据");
        PdhCloseQuery(queryHandle);
        return;
    }

    counterInitialized = true;
    Logger::Debug("CPU性能计数器初始化完成");
#elif defined(PLATFORM_MACOS)
    // macOS 使用 host_statistics 获取 CPU 信息
    counterInitialized = true;
#elif defined(__linux__)
    // Linux 使用 /proc/stat 获取 CPU 信息
    counterInitialized = true;
#endif
}

double CpuInfo::GetLargeCoreSpeed() const {
    const_cast<CpuInfo*>(this)->UpdateCoreSpeeds();
    if (largeCoresSpeeds.empty()) {
        return GetCurrentSpeed();
    }
    // 计算平均频率
    double total = 0;
    for (DWORD speed : largeCoresSpeeds) {
        total += speed;
    }
    return total / largeCoresSpeeds.size();
}

double CpuInfo::GetSmallCoreSpeed() const {
    const_cast<CpuInfo*>(this)->UpdateCoreSpeeds();
    if (smallCoresSpeeds.empty()) {
        return GetCurrentSpeed();
    }
    // 计算平均频率
    double total = 0;
    for (DWORD speed : smallCoresSpeeds) {
        total += speed;
    }
    return total / smallCoresSpeeds.size();
}

void CpuInfo::CleanupCounter() {
    if (counterInitialized) {
        PdhCloseQuery(queryHandle);
        counterInitialized = false;
    }
}

void CpuInfo::DetectCores() {
#ifdef PLATFORM_WINDOWS
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    totalCores = sysInfo.dwNumberOfProcessors;

    DWORD bufferSize = 0;
    GetLogicalProcessorInformation(nullptr, &bufferSize);
    std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer(bufferSize / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));

    if (GetLogicalProcessorInformation(buffer.data(), &bufferSize)) {
        for (const auto& info : buffer) {
            if (info.Relationship == RelationProcessorCore) {
                (info.ProcessorCore.Flags == 1) ? largeCores++ : smallCores++;
            }
        }
    }
    
#elif defined(PLATFORM_MACOS)
    // macOS 使用 sysctl 获取核心数量
    int physicalCores = 0;
    int logicalCores = 0;
    size_t size = sizeof(int);
    
    // 获取物理核心数
    if (sysctlbyname("hw.physicalcpu", &physicalCores, &size, nullptr, 0) == 0) {
        totalCores = physicalCores;
    }
    
    // 获取逻辑核心数
    if (sysctlbyname("hw.logicalcpu", &logicalCores, &size, nullptr, 0) == 0) {
        if (totalCores == 0) {
            totalCores = logicalCores;
        }
    }
    
    // 检测性能核心和能效核心 (Apple Silicon)
    natural_t numCPUs = 0;
    processor_cpu_load_info_t *cpuLoadInfo = nullptr;
    mach_msg_type_number_t numCpuInfo;
    
    if (host_processor_info(mach_host_self(), &numCPUs, &cpuLoadInfo, &numCpuInfo) == KERN_SUCCESS) {
        // Apple Silicon 通常有性能核(P-cores)和能效核(E-cores)
        // 这里简化处理，假设物理核心数为性能核心数
        largeCores = physicalCores;
        smallCores = logicalCores - physicalCores;
        
        vm_deallocate(mach_task_self(), (vm_size_t)(numCPUs * sizeof(processor_cpu_load_info_t)), (vm_address_t)cpuLoadInfo, 0);
    }
    
#elif defined(__linux__)
    // Linux 使用 /proc/cpuinfo 获取核心信息
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (!cpuinfo.is_open()) {
        totalCores = 1;
        largeCores = 1;
        smallCores = 0;
        return;
    }
    
    std::string line;
    int processorCount = 0;
    std::set<int> physicalCoreIds;
    
    while (std::getline(cpuinfo, line)) {
        if (line.find("processor") == 0) {
            processorCount++;
        } else if (line.find("physical id") == 0) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                int physicalId = std::stoi(line.substr(pos + 1));
                physicalCoreIds.insert(physicalId);
            }
        }
    }
    
    totalCores = processorCount;
    
    // 简化处理：假设物理核心数为性能核心，逻辑核心数减去物理核心数为能效核心
    largeCores = physicalCoreIds.size();
    smallCores = processorCount - largeCores;
    
    cpuinfo.close();
    
#else
    totalCores = 1;
    largeCores = 1;
    smallCores = 0;
#endif
}

void CpuInfo::UpdateCoreSpeeds() {
    // 检查更新间隔
    DWORD currentTime = GetTickCount();
    if (currentTime - lastUpdateTime < 1000) { // 1秒更新一次
        return;
    }
    lastUpdateTime = currentTime;

    HKEY hKey;
    DWORD speed;
    DWORD size = sizeof(DWORD);

    // 清空旧数据
    largeCoresSpeeds.clear();
    smallCoresSpeeds.clear();

    // 遍历所有核心
    for (int i = 0; i < totalCores; ++i) {
        std::wstring keyPath = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\" + std::to_wstring(i);
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            if (RegQueryValueExW(hKey, L"~MHz", NULL, NULL, (LPBYTE)&speed, &size) == ERROR_SUCCESS) {
                // 根据核心类型分类存储频率
                if (i < largeCores * 2) { // 考虑超线程，每个物理核心有两个逻辑核心
                    largeCoresSpeeds.push_back(speed);
                }
                else {
                    smallCoresSpeeds.push_back(speed);
                }
            }
            RegCloseKey(hKey);
        }
    }
}

double CpuInfo::updateUsage() {
    if (!counterInitialized) {
        Logger::Warn("CPU性能计数器未初始化");
        return cpuUsage;
    }

#ifdef PLATFORM_WINDOWS
    // 检查时间间隔，确保两次采样之间有足够间隔
    static DWORD lastCollectTime = 0;
    DWORD currentTime = GetTickCount();
    DWORD delta = currentTime - lastCollectTime;
    if (delta < 500) { // 放宽至 500ms
        return cpuUsage; // 返回上次的值
    }

    lastCollectTime = currentTime;

    PDH_STATUS status = PdhCollectQueryData(queryHandle);
    if (status != ERROR_SUCCESS) {
        Logger::Error("无法收集CPU使用率数据，错误代码: " + std::to_string(status));
        return cpuUsage;
    }

    PDH_FMT_COUNTERVALUE counterValue;
    status = PdhGetFormattedCounterValue(counterHandle,
        PDH_FMT_DOUBLE,
        NULL,
        &counterValue);

    if (status != ERROR_SUCCESS) {
        Logger::Error("无法格式化CPU使用率数据，错误代码: " + std::to_string(status));
        return cpuUsage;
    }

    // 记录采样间隔
    prevSampleTick = lastSampleTick;
    lastSampleTick = currentTime;
    if (prevSampleTick != 0) {
        lastSampleIntervalMs = static_cast<double>(lastSampleTick - prevSampleTick);
    }

    // 验证数据有效性
    if (counterValue.CStatus == PDH_CSTATUS_VALID_DATA || counterValue.CStatus == PDH_CSTATUS_NEW_DATA) {
        double newUsage = counterValue.doubleValue;
        if (newUsage < 0.0) newUsage = 0.0;
        if (newUsage > 100.0) newUsage = 100.0;
        if (cpuUsage > 0.0) cpuUsage = (cpuUsage * 0.8) + (newUsage * 0.2); else cpuUsage = newUsage;
        static int updateCounter = 0;
        if (++updateCounter % 60 == 0) {
            Logger::Debug("CPU使用率更新: " + std::to_string(cpuUsage) + "% (采样间隔=" + std::to_string(lastSampleIntervalMs) + "ms)");
        }
    } else {
        Logger::Warn("CPU使用率数据无效，状态: " + std::to_string(counterValue.CStatus));
    }
    return cpuUsage;
    
#elif defined(PLATFORM_MACOS)
    // macOS 使用 host_statistics 获取 CPU 使用率
    static mach_port_t host_port = 0;
    if (host_port == 0) {
        host_port = mach_host_self();
    }
    
    host_cpu_load_info_data_t cpuinfo;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    
    if (host_statistics(host_port, HOST_CPU_LOAD_INFO, (host_info_t)&cpuinfo, &count) == KERN_SUCCESS) {
        unsigned long totalTicks = 0;
        unsigned long idleTicks = 0;
        
        for (int i = 0; i < CPU_STATE_MAX; i++) {
            totalTicks += cpuinfo.cpu_ticks[i];
        }
        
        idleTicks = cpuinfo.cpu_ticks[CPU_STATE_IDLE];
        
        if (totalTicks > 0) {
            static unsigned long prevTotalTicks = 0;
            static unsigned long prevIdleTicks = 0;
            
            if (prevTotalTicks > 0) {
                unsigned long totalDiff = totalTicks - prevTotalTicks;
                unsigned long idleDiff = idleTicks - prevIdleTicks;
                
                if (totalDiff > 0) {
                    double newUsage = 100.0 * (1.0 - (double)idleDiff / totalDiff);
                    if (newUsage < 0.0) newUsage = 0.0;
                    if (newUsage > 100.0) newUsage = 100.0;
                    if (cpuUsage > 0.0) cpuUsage = (cpuUsage * 0.8) + (newUsage * 0.2); else cpuUsage = newUsage;
                }
            }
            
            prevTotalTicks = totalTicks;
            prevIdleTicks = idleTicks;
        }
    }
    
    return cpuUsage;
    
#elif defined(__linux__)
    // Linux 使用 /proc/stat 获取 CPU 使用率
    static std::ifstream procStat("/proc/stat");
    if (!procStat.is_open()) {
        return cpuUsage;
    }
    
    std::string line;
    if (std::getline(procStat, line)) {
        if (line.find("cpu ") == 0) {
            std::istringstream iss(line);
            std::string cpuLabel;
            unsigned long user, nice, system, idle, iowait, irq, softirq;
            
            iss >> cpuLabel >> user >> nice >> system >> idle >> iowait >> irq >> softirq;
            
            unsigned long totalTicks = user + nice + system + idle + iowait + irq + softirq;
            unsigned long idleTicks = idle + iowait;
            
            static unsigned long prevTotalTicks = 0;
            static unsigned long prevIdleTicks = 0;
            
            if (prevTotalTicks > 0) {
                unsigned long totalDiff = totalTicks - prevTotalTicks;
                unsigned long idleDiff = idleTicks - prevIdleTicks;
                
                if (totalDiff > 0) {
                    double newUsage = 100.0 * (1.0 - (double)idleDiff / totalDiff);
                    if (newUsage < 0.0) newUsage = 0.0;
                    if (newUsage > 100.0) newUsage = 100.0;
                    if (cpuUsage > 0.0) cpuUsage = (cpuUsage * 0.8) + (newUsage * 0.2); else cpuUsage = newUsage;
                }
            }
            
            prevTotalTicks = totalTicks;
            prevIdleTicks = idleTicks;
        }
    }
    
    procStat.close();
    return cpuUsage;
    
#else
    return 0.0; // 不支持的平台
#endif
}

double CpuInfo::GetUsage() {
    double currentUsage = updateUsage();
    
    // 减少调试信息的频率
    static int debugCounter = 0;
    if (++debugCounter % 30 == 0) { // 每30次调用记录一次（约30秒）
        Logger::Info("CPU使用率: " + std::to_string(currentUsage) + "%");
    }
    
    return currentUsage;
}

std::string CpuInfo::GetNameFromRegistry() {
    HKEY hKey;
    char buffer[128];
    DWORD size = sizeof(buffer);

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExA(hKey, "ProcessorNameString", nullptr, nullptr, (LPBYTE)buffer, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return std::string(buffer, size - 1);
        }
        RegCloseKey(hKey);
    }
    return "Unknown CPU";
}



std::string CpuInfo::GetNameFromProcCpuinfo() {
    std::ifstream file("/proc/cpuinfo");
    if (!file.is_open()) {
        return "Unknown CPU";
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("model name") != std::string::npos) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                std::string cpuModel = line.substr(pos + 1);
                // 移除前后空格
                cpuModel.erase(0, cpuModel.find_first_not_of(' '));
                cpuModel.erase(cpuModel.find_last_not_of(' ') + 1);
                return cpuModel;
            }
        }
    }
    return "Unknown CPU";
}

#ifdef PLATFORM_MACOS
std::string CpuInfo::GetNameFromSysctl() {
    // macOS 使用 sysctl 获取 CPU 信息
    size_t size = 0;
    sysctlbyname("machdep.cpu.brand_string", nullptr, &size, nullptr, 0);
    
    if (size > 0) {
        std::vector<char> buffer(size);
        if (sysctlbyname("machdep.cpu.brand_string", buffer.data(), &size, nullptr, 0) == 0) {
            return std::string(buffer.data());
        }
    }
    
    // 如果品牌字符串获取失败，尝试其他方法
    size_t modelSize = 0;
    sysctlbyname("hw.model", nullptr, &modelSize, nullptr, 0);
    
    if (modelSize > 0) {
        std::vector<char> modelBuffer(modelSize);
        if (sysctlbyname("hw.model", modelBuffer.data(), &modelSize, nullptr, 0) == 0) {
            return std::string(modelBuffer.data()) + " CPU";
        }
    }
    
    return "Unknown CPU";
}
#else
std::string CpuInfo::GetNameFromSysctl() {
    // 非macOS平台不支持sysctlbyname
    return "Unknown CPU";
}
#endif

int CpuInfo::GetTotalCores() const {
    return totalCores;
}

int CpuInfo::GetSmallCores() const {
    return smallCores;
}

int CpuInfo::GetLargeCores() const {
    return largeCores;
}

DWORD CpuInfo::GetCurrentSpeed() const {
    HKEY hKey;
    DWORD speed = 0;
    DWORD size = sizeof(DWORD);
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExW(hKey, L"~MHz", NULL, NULL, (LPBYTE)&speed, &size);
        RegCloseKey(hKey);
    }
    return speed;
}

double CpuInfo::GetBaseFrequencyMHz() const {
    double base=0, cur=0;
    if (!QueryBaseAndCurrentFrequencyACPI(base, cur)) {
        QueryBaseAndCurrentFrequencyFallback(base, cur);
    }
    return base;
}

double CpuInfo::GetCurrentFrequencyMHz() const {
    return const_cast<CpuInfo*>(this)->updateInstantFrequencyMHz();
}

std::string CpuInfo::GetName() {
    return cpuName;
}

bool CpuInfo::IsHyperThreadingEnabled() const {
    return (totalCores > (largeCores + smallCores));
}

bool CpuInfo::IsVirtualizationEnabled() const {
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    bool hasVMX = (cpuInfo[2] & (1 << 5)) != 0;

    if (!hasVMX) return false;

    bool isVMXEnabled = false;
    __try {
        unsigned __int64 msrValue = __readmsr(0x3A);
        isVMXEnabled = (msrValue & 0x5) != 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        isVMXEnabled = false;
    }

    return isVMXEnabled;
}

// WinCpuAdapter 实现
WinCpuAdapter::WinCpuAdapter() {
    m_originalCpuInfo = std::make_unique<CpuInfo>();
}

WinCpuAdapter::~WinCpuAdapter() {
    Cleanup();
}

double WinCpuAdapter::GetUsage() {
    if (!m_originalCpuInfo) return 0.0;
    return m_originalCpuInfo->GetUsage();
}

std::string WinCpuAdapter::GetName() {
    if (!m_originalCpuInfo) return "";
    return m_originalCpuInfo->GetName();
}

int WinCpuAdapter::GetTotalCores() const {
    if (!m_originalCpuInfo) return 0;
    return m_originalCpuInfo->GetTotalCores();
}

int WinCpuAdapter::GetSmallCores() const {
    if (!m_originalCpuInfo) return 0;
    return m_originalCpuInfo->GetSmallCores();
}

int WinCpuAdapter::GetLargeCores() const {
    if (!m_originalCpuInfo) return 0;
    return m_originalCpuInfo->GetLargeCores();
}

double WinCpuAdapter::GetLargeCoreSpeed() const {
    if (!m_originalCpuInfo) return 0.0;
    return m_originalCpuInfo->GetLargeCoreSpeed();
}

double WinCpuAdapter::GetSmallCoreSpeed() const {
    if (!m_originalCpuInfo) return 0.0;
    return m_originalCpuInfo->GetSmallCoreSpeed();
}

DWORD WinCpuAdapter::GetCurrentSpeed() const {
    if (!m_originalCpuInfo) return 0;
    return m_originalCpuInfo->GetCurrentSpeed();
}

bool WinCpuAdapter::IsHyperThreadingEnabled() const {
    if (!m_originalCpuInfo) return false;
    return m_originalCpuInfo->IsHyperThreadingEnabled();
}

bool WinCpuAdapter::IsVirtualizationEnabled() const {
    if (!m_originalCpuInfo) return false;
    return m_originalCpuInfo->IsVirtualizationEnabled();
}

double WinCpuAdapter::GetLastSampleIntervalMs() const {
    if (!m_originalCpuInfo) return 0.0;
    return m_originalCpuInfo->GetLastSampleIntervalMs();
}

double WinCpuAdapter::GetBaseFrequencyMHz() const {
    if (!m_originalCpuInfo) return 0.0;
    return m_originalCpuInfo->GetBaseFrequencyMHz();
}

double WinCpuAdapter::GetCurrentFrequencyMHz() const {
    if (!m_originalCpuInfo) return 0.0;
    return m_originalCpuInfo->GetCurrentFrequencyMHz();
}

bool WinCpuAdapter::Initialize() {
    // CpuInfo类在构造时自动初始化，这里只需要确认对象存在
    return m_originalCpuInfo != nullptr;
}

void WinCpuAdapter::Cleanup() {
    // CpuInfo类在析构时自动清理，这里不需要额外操作
}

bool WinCpuAdapter::Update() {
    // CpuInfo类在每次调用Get方法时自动更新数据
    // 这里可以添加额外的更新逻辑，如果需要的话
    return m_originalCpuInfo != nullptr;
}
