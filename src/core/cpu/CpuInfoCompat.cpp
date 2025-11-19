#include "CpuInfo.h"
#include "../Utils/Logger.h"

#ifdef PLATFORM_MACOS
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_types.h>
#include <mach/mach_host.h>
#include <sys/utsname.h>
#include <thread>
#include <chrono>

// Cross-platform stub implementation for macOS
CpuInfo::CpuInfo() {
    try {
        DetectCores();
        cpuName = GetNameFromSysctl();
        // Initialize basic values
        cpuUsage = 0.0;
        counterInitialized = false;
        totalCores = 0;
        largeCores = 0;
        smallCores = 0;
        lastUpdateTime = 0;
        lastSampleTick = 0;
        prevSampleTick = 0;
        lastSampleIntervalMs = 0.0;
        cachedInstantMHz = 0.0;
        lastFreqTick = 0;
        freqCounterInitialized = false;
    }
    catch (const std::exception& e) {
        Logger::Error("CPU信息初始化失败: " + std::string(e.what()));
    }
}

CpuInfo::~CpuInfo() {
}

void CpuInfo::DetectCores() {
    totalCores = 0;
    largeCores = 0;
    smallCores = 0;
    
    size_t len = sizeof(totalCores);
    if (sysctlbyname("hw.ncpu", &totalCores, &len, nullptr, 0) == 0) {
        // For simplicity, assume all cores are performance cores on macOS
        largeCores = totalCores;
    }
}

void CpuInfo::UpdateCoreSpeeds() {
    // macOS stub implementation
    lastUpdateTime = static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

double CpuInfo::updateUsage() {
    // Simple CPU usage calculation for macOS
    static uint64_t lastIdle = 0, lastTotal = 0;
    
    natural_t numCPUsU = 0;
    mach_msg_type_number_t numCPUs = 0;
    processor_cpu_load_info_t cpuLoad;
    mach_msg_type_number_t numCPUsLoad = 0;
    
    kern_return_t kr = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,
                                       &numCPUsU, 
                                       (processor_info_array_t*)&cpuLoad, &numCPUsLoad);
    if (kr == KERN_SUCCESS) {
        
        uint64_t totalTicks = 0;
        uint64_t idleTicks = 0;
        
        for (natural_t i = 0; i < numCPUs; i++) {
            totalTicks += cpuLoad[i].cpu_ticks[CPU_STATE_USER] +
                         cpuLoad[i].cpu_ticks[CPU_STATE_SYSTEM] +
                         cpuLoad[i].cpu_ticks[CPU_STATE_IDLE] +
                         cpuLoad[i].cpu_ticks[CPU_STATE_NICE];
            idleTicks += cpuLoad[i].cpu_ticks[CPU_STATE_IDLE];
        }
        
        if (lastTotal > 0) {
            uint64_t totalDiff = totalTicks - lastTotal;
            uint64_t idleDiff = idleTicks - lastIdle;
            
            if (totalDiff > 0) {
                cpuUsage = 100.0 * (1.0 - (double)idleDiff / totalDiff);
                if (cpuUsage < 0.0) cpuUsage = 0.0;
                if (cpuUsage > 100.0) cpuUsage = 100.0;
            }
        }
        
        lastIdle = idleTicks;
        lastTotal = totalTicks;
        
        vm_deallocate(mach_task_self(), (vm_address_t)cpuLoad, (vm_size_t)numCPUsU * sizeof(processor_cpu_load_info_t));
    }
    
    return cpuUsage;
}

double CpuInfo::GetUsage() {
    return updateUsage();
}

std::string CpuInfo::GetNameFromSysctl() {
    size_t len = 0;
    sysctlbyname("machdep.cpu.brand_string", nullptr, &len, nullptr, 0);
    if (len > 0) {
        std::string cpuModel;
        cpuModel.resize(len);
        sysctlbyname("machdep.cpu.brand_string", &cpuModel[0], &len, nullptr, 0);
        cpuModel.resize(len - 1);
        return cpuModel;
    }
    return "Unknown CPU";
}

std::string CpuInfo::GetName() {
    return cpuName;
}

DWORD CpuInfo::GetCurrentSpeed() const {
    return static_cast<DWORD>(cachedInstantMHz);
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

double CpuInfo::GetLargeCoreSpeed() const {
    return cachedInstantMHz;
}

double CpuInfo::GetSmallCoreSpeed() const {
    return cachedInstantMHz;
}

double CpuInfo::GetBaseFrequencyMHz() const {
    return cachedInstantMHz; // Using cached value as base frequency for macOS
}

double CpuInfo::GetCurrentFrequencyMHz() const {
    return cachedInstantMHz;
}

bool CpuInfo::IsHyperThreadingEnabled() const {
    return false; // Simplified for macOS
}

bool CpuInfo::IsVirtualizationEnabled() const {
    return false; // Simplified for macOS
}

void CpuInfo::InitializeCounter() {
    counterInitialized = true;
}

void CpuInfo::CleanupCounter() {
    counterInitialized = false;
}

void CpuInfo::InitializeFrequencyCounter() {
    // Stub for macOS
}

void CpuInfo::CleanupFrequencyCounter() {
    // Stub for macOS
}

#endif // PLATFORM_MACOS