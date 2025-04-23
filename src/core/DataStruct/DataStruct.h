// DataStruct.h
#pragma once
#include <windows.h>
#include <string>
#include <vector>

#pragma pack(push, 1) // 确保内存对齐

// GPU信息
struct GPUData {
    wchar_t name[128];    // GPU名称
    wchar_t brand[64];    // 品牌
    uint64_t memory;      // 显存（字节）
    double coreClock;     // 核心频率（MHz）
};

// 网络适配器信息
struct NetworkAdapterData {
    wchar_t name[128];    // 适配器名称
    wchar_t mac[32];      // MAC地址
    uint64_t speed;       // 速度（bps）
};

// 磁盘信息
struct DiskData {
    wchar_t letter;       // 盘符（如L'C'）
    wchar_t label[128];   // 卷标
    wchar_t fileSystem[64];// 文件系统
    uint64_t totalSize;   // 总容量（字节）
    uint64_t usedSpace;   // 已用空间（字节）
    uint64_t freeSpace;   // 可用空间（字节）
    // Only modify the DiskData structure to add freeSpace field
    struct DiskData {
        wchar_t letter;       // 盘符（如L'C'）
        wchar_t label[128];   // 卷标
        wchar_t fileSystem[64];// 文件系统
        uint64_t totalSize;   // 总容量（字节）
        uint64_t usedSpace;   // 已用空间（字节）
        uint64_t freeSpace;   // 可用空间（字节）
    };
};

// 温度传感器信息
struct TemperatureData {
    wchar_t sensorName[64]; // 传感器名称
    double temperature;     // 温度（摄氏度）
};

// SystemInfo结构
struct SystemInfo {
    std::string cpuName;
    int physicalCores;
    int logicalCores;
    double cpuUsage;
    int performanceCores;
    int efficiencyCores;
    double performanceCoreFreq;
    double efficiencyCoreFreq;
    bool hyperThreading;
    bool virtualization;
    uint64_t totalMemory;
    uint64_t usedMemory;
    uint64_t availableMemory;
    std::vector<GPUData> gpus;
    std::vector<NetworkAdapterData> adapters;
    std::vector<DiskData> disks;
    std::vector<std::pair<std::string, double>> temperatures;
    std::string osVersion;          // Added
    std::string networkAdapterName; // Added
    std::string networkAdapterMac;  // Added
    uint64_t networkAdapterSpeed;   // Added
    std::string gpuName;            // Added
    uint64_t gpuMemory;             // Added
    double gpuCoreFreq;             // Added
    std::string gpuBrand;           // Added
    SYSTEMTIME lastUpdate;
};

// 共享内存主结构
struct SharedMemoryBlock {
    char cpuName[128];        // CPU名称
    int physicalCores;        // 物理核心数
    int logicalCores;         // 逻辑核心数
    float cpuUsage;           // CPU使用率（百分比）
    int performanceCores;     // 性能核心数
    int efficiencyCores;      // 能效核心数
    double pCoreFreq;         // 性能核心频率（GHz）
    double eCoreFreq;         // 能效核心频率（GHz）
    bool hyperThreading;      // 超线程是否启用
    bool virtualization;      // 虚拟化是否启用
    uint64_t totalMemory;     // 总内存（字节）
    uint64_t usedMemory;      // 已用内存（字节）
    uint64_t availableMemory; // 可用内存（字节）

    // GPU信息（支持最多2个GPU）
    GPUData gpus[2];

    // 网络适配器（支持最多4个适配器）
    NetworkAdapterData adapters[4];

    // 磁盘信息（支持最多8个磁盘）
    DiskData disks[8];

    // 温度数据（支持10个传感器）
    TemperatureData temperatures[10];

    int adapterCount;
    int tempCount;
    int gpuCount;
    int diskCount;
    SYSTEMTIME lastUpdate;
    CRITICAL_SECTION lock;
};
#pragma pack(pop)