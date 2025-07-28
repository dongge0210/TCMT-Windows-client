// DataStruct.h
#pragma once
#include <windows.h>
#include <string>
#include <vector>

#pragma pack(push, 1) // 确保内存对齐

// SMART属性信息
struct SmartAttributeData {
    uint8_t id;                    // 属性ID
    uint8_t flags;                 // 状态标志
    uint8_t current;               // 当前值
    uint8_t worst;                 // 最坏值
    uint8_t threshold;             // 阈值
    uint64_t rawValue;             // 原始值
    wchar_t name[64];              // 属性名称
    wchar_t description[128];      // 属性描述
    bool isCritical;               // 是否关键属性
    double physicalValue;          // 物理值（经过转换）
    wchar_t units[16];             // 单位
};

// 物理磁盘SMART信息
struct PhysicalDiskSmartData {
    wchar_t model[128];            // 磁盘型号
    wchar_t serialNumber[64];      // 序列号
    wchar_t firmwareVersion[32];   // 固件版本
    wchar_t interfaceType[32];     // 接口类型 (SATA/NVMe/etc)
    wchar_t diskType[16];          // 磁盘类型 (SSD/HDD)
    uint64_t capacity;             // 总容量（字节）
    double temperature;            // 温度
    uint8_t healthPercentage;      // 健康百分比
    bool isSystemDisk;             // 是否系统盘
    bool smartEnabled;             // SMART是否启用
    bool smartSupported;           // 是否支持SMART
    
    // SMART属性数组（最多32个常用属性）
    SmartAttributeData attributes[32];
    int attributeCount;            // 实际属性数量
    
    // 关键健康指标
    uint64_t powerOnHours;         // 通电时间（小时）
    uint64_t powerCycleCount;      // 开机次数
    uint64_t reallocatedSectorCount; // 重新分配扇区数
    uint64_t currentPendingSector; // 当前待处理扇区
    uint64_t uncorrectableErrors;  // 不可纠正错误
    double wearLeveling;           // 磨损均衡（SSD）
    uint64_t totalBytesWritten;    // 总写入字节数
    uint64_t totalBytesRead;       // 总读取字节数
    
    // 关联的逻辑驱动器
    char logicalDriveLetters[8];   // 关联的驱动器盘符
    int logicalDriveCount;         // 关联驱动器数量
    
    SYSTEMTIME lastScanTime;       // 最后扫描时间
};

// GPU信息
struct GPUData {
    wchar_t name[128];    // GPU名称
    wchar_t brand[64];    // 品牌
    uint64_t memory;      // 显存（字节）
    double coreClock;     // 核心频率（MHz）
    bool isVirtual;       // 新增：是否为虚拟显卡
};

// 网络适配器信息
struct NetworkAdapterData {
    wchar_t name[128];    // 适配器名称
    wchar_t mac[32];      // MAC地址
    wchar_t ipAddress[64]; // 新增：IP地址
    wchar_t adapterType[32]; // 新增：网卡类型（无线/有线）
    uint64_t speed;       // 速度（bps）
};

// 磁盘信息
struct DiskData {
    char letter;          // 盘符（如'C'）
    std::string label;    // 卷标
    std::string fileSystem;// 文件系统
    uint64_t totalSize = 0; // 总容量（字节）
    uint64_t usedSpace = 0; // 已用空间（字节）
    uint64_t freeSpace = 0; // 可用空间（字节）
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
    double cpuUsage;      // 确保使用double类型
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
    std::vector<PhysicalDiskSmartData> physicalDisks; // 新增：物理磁盘SMART数据
    std::vector<std::pair<std::string, double>> temperatures;
    std::string osVersion;
    std::string gpuName;            // Added
    std::string gpuBrand;           // Added
    uint64_t gpuMemory;             // Added
    double gpuCoreFreq;             // Added
    bool gpuIsVirtual;              // 新增：GPU是否为虚拟显卡
    std::string networkAdapterName; // Added
    std::string networkAdapterMac;  // Added
    std::string networkAdapterIp;   // 新增：网络适配器IP地址
    std::string networkAdapterType; // 新增：网络适配器类型（无线/有线）
    uint64_t networkAdapterSpeed;   // Added
    double cpuTemperature; // 新增：CPU温度
    double gpuTemperature; // 新增：GPU温度
    SYSTEMTIME lastUpdate;
};

// 共享内存主结构
struct SharedMemoryBlock {
    wchar_t cpuName[128];       // CPU名称 - wchar_t array
    int physicalCores;        // 物理核心数
    int logicalCores;         // 逻辑核心数
    double cpuUsage;          // 改为double类型，提高精度
    int performanceCores;     // 性能核心数
    int efficiencyCores;      // 能效核心数
    double pCoreFreq;         // 性能核心频率（GHz）
    double eCoreFreq;         // 能效核心频率（GHz）
    bool hyperThreading;      // 超线程是否启用
    bool virtualization;      // 虚拟化是否启用
    uint64_t totalMemory;     // 总内存（字节）
    uint64_t usedMemory;      // 已用内存（字节）
    uint64_t availableMemory; // 可用内存（字节）
    double cpuTemperature; // 新增：CPU温度
    double gpuTemperature; // 新增：GPU温度

    // GPU信息（支持最多2个GPU）
    GPUData gpus[2];

    // 网络适配器（支持最多4个适配器）
    NetworkAdapterData adapters[4];

    // 逻辑磁盘信息（支持最多8个磁盘）
    struct SharedDiskData {
        char letter;             // 盘符（如'C'）
        wchar_t label[128];      // 卷标 - Using wchar_t array for shared memory
        wchar_t fileSystem[32];  // 文件系统 - Using wchar_t array for shared memory
        uint64_t totalSize;      // 总容量（字节）
        uint64_t usedSpace;      // 已用空间（字节）
        uint64_t freeSpace;      // 可用空间（字节）
    } disks[8];

    // 物理磁盘SMART信息（支持最多8个物理磁盘）
    PhysicalDiskSmartData physicalDisks[8];

    // 温度数据（支持10个传感器）
    TemperatureData temperatures[10];

    int adapterCount;
    int tempCount;
    int gpuCount;
    int diskCount;
    int physicalDiskCount;       // 新增：物理磁盘数量
    SYSTEMTIME lastUpdate;
    CRITICAL_SECTION lock;
};
#pragma pack(pop)