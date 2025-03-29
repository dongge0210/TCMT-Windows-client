#include "CpuInfo.h"
#include "Logger.h"
#include <intrin.h>
#include <windows.h>
#include <vector>
#include <pdh.h>

#pragma comment(lib, "pdh.lib")

CpuInfo::CpuInfo() :
    totalCores(0),
    largeCores(0),
    smallCores(0),
    cpuUsage(0.0),
    counterInitialized(false),
    lastUpdateTime(0) {

    try {
        DetectCores();
        cpuName = GetNameFromRegistry();
        InitializeCounter();
        UpdateCoreSpeeds();  // 初始化频率信息
    }
    catch (const std::exception& e) {
        Logger::Error("CPU信息初始化失败: " + std::string(e.what()));
    }
}

CpuInfo::~CpuInfo() {
    CleanupCounter();
}

void CpuInfo::InitializeCounter() {
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
    Sleep(100); // 等待首次采样完成
    updateUsage(); // 初始化采样
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
        return cpuUsage;
    }

    PDH_STATUS status = PdhCollectQueryData(queryHandle);
    if (status != ERROR_SUCCESS) {
        Logger::Error("无法收集CPU使用率数据");
        return cpuUsage;
    }

    PDH_FMT_COUNTERVALUE counterValue;
    status = PdhGetFormattedCounterValue(counterHandle,
        PDH_FMT_DOUBLE,
        NULL,
        &counterValue);

    if (status != ERROR_SUCCESS) {
        Logger::Error("无法格式化CPU使用率数据");
        return cpuUsage;
    }

    cpuUsage = counterValue.doubleValue;
    return cpuUsage;
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

double CpuInfo::GetUsage() {
    return updateUsage();
}

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
