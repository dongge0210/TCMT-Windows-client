// DataStruct.h
#pragma once
#include "../common/PlatformDefs.h"
#include <string>
#include <vector>
#include <stdint.h>
#include "../usb/USBInfo.h"

#ifdef PLATFORM_WINDOWS
    #include <windows.h>
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    #include <sys/time.h>
    #include <time.h>
#endif

// 跨平台时间结构
struct CrossPlatformTime {
#ifdef PLATFORM_WINDOWS
    SYSTEMTIME windowsTime;
#else
    time_t unixTime;
    uint32_t milliseconds; // 毫秒部分
#endif
};

#pragma pack(push, 1) // 确保内存对齐

// 温度传感器结构
struct TemperatureSensor {
 char name[32]; // UTF-8，不足填0
 int16_t valueC_x10; // 温度*10 (0.1°C)，不可用 -1
 uint8_t flags; // bit0=valid, bit1=urgentLast
};

// SMART磁盘评分结构
struct SmartDiskScore {
 char diskId[32];
 int16_t score; //0-100，-1 不可用
 int32_t hoursOn;
 int16_t wearPercent; //0-100，-1 不可用
 uint16_t reallocated;
 uint16_t pending;
 uint16_t uncorrectable;
 int16_t temperatureC; // -1 不可用
 uint8_t recentGrowthFlags; // bit0=reallocated增, bit1=wear突增
};

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
    
    CrossPlatformTime lastScanTime; // 最后扫描时间
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
    char letter;          // 盘符（如'C'），macOS/Linux上为0
    std::string label;    // 卷标
    std::string fileSystem;// 文件系统
    uint64_t totalSize = 0; // 总容量（字节）
    uint64_t usedSpace = 0; // 已用空间（字节）
    uint64_t freeSpace = 0; // 可用空间（字节）
    
    // 跨平台特有字段
    std::string mountPoint;   // macOS/Linux挂载点
    std::string devicePath;   // 设备路径（如 /dev/disk1s1）
    bool isRemovable = false; // 是否为可移动设备
    bool isSSD = false;       // 是否为SSD
    std::string interfaceType; // SATA, NVMe, USB等
    std::string diskUUID;     // 磁盘UUID（macOS）
    std::string volumeUUID;   // 卷UUID（macOS）
    bool isAPFS = false;      // 是否为APFS文件系统（macOS）
    bool isEncrypted = false; // 是否加密（macOS）
};

// 温度传感器信息
struct TemperatureData {
    wchar_t sensorName[64]; // 传感器名称
    double temperature;     // 温度（摄氏度）
};

// TPM信息
struct TpmData {
    wchar_t manufacturerName[128];  // TPM制造商名称
    wchar_t manufacturerId[32];     // 制造商ID
    wchar_t version[32];            // TPM版本
    wchar_t firmwareVersion[32];    // 固件版本
    wchar_t status[64];             // TPM状态
    bool isEnabled;                 // TPM是否启用
    bool isActivated;               // TPM是否激活
    bool isOwned;                   // TPM是否已拥有
    bool isReady;                   // TPM是否就绪
    bool tbsAvailable;              // TBS是否可用
    bool physicalPresenceRequired;  // 是否需要物理存在
    uint32_t specVersion;           // TPM规范版本
    uint32_t tbsVersion;            // TBS版本
    wchar_t errorMessage[256];      // 错误信息
    wchar_t detectionMethod[64];    // 检测方法 ("TBS", "WMI", "TBS+WMI", "未检测到")
    bool wmiDetectionWorked;        // WMI检测是否成功
    bool tbsDetectionWorked;        // TBS检测是否成功
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
    double cpuBaseFrequencyMHz = 0.0; // 新增：CPU 基准频率（MHz）
    double cpuCurrentFrequencyMHz = 0.0; // 新增：CPU 即时频率（MHz）
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
    double cpuUsageSampleIntervalMs = 0.0; // 新增：CPU使用率采样间隔（毫秒）
    // USB信息
    std::vector<USBDeviceInfo> usbDevices; // 新增：USB设备列表
    // TPM信息（扩展）
    bool hasTpm = false;            // 是否存在TPM
    std::string tpmManufacturer;    // TPM制造商
    std::string tpmManufacturerId;  // 制造商ID
    std::string tpmVersion;         // TPM版本
    std::string tpmFirmwareVersion; // 固件版本
    std::string tpmStatus;          // TPM状态
    bool tpmEnabled = false;        // TPM是否启用
    bool tpmIsActivated = false;    // 是否激活
    bool tpmIsOwned = false;        // 是否已拥有
    bool tpmReady = false;          // TPM是否就绪
    bool tpmTbsAvailable = false;   // TBS是否可用
    bool tpmPhysicalPresenceRequired = false; // 是否需要物理存在
    uint32_t tpmSpecVersion = 0;    // 规范版本
    uint32_t tpmTbsVersion = 0;     // TBS版本
    std::string tpmErrorMessage;    // 错误信息
    std::string tmpDetectionMethod; // 检测方法
    bool tmpWmiDetectionWorked = false; // WMI检测是否成功
    bool tmpTbsDetectionWorked = false; // TBS检测是否成功
    CrossPlatformTime lastUpdate;
    
    // 跨平台内存分区信息 (macOS特有)
    uint64_t activeMemory = 0;      // 活跃内存 (字节)
    uint64_t inactiveMemory = 0;    // 非活跃内存 (字节)
    uint64_t wiredMemory = 0;       // 有线内存 (字节, macOS)
    uint64_t compressedMemory = 0;  // 压缩内存 (字节, macOS)
    double memoryPressure = 0.0;    // 内存压力 (0.0-1.0, macOS)
    uint64_t swapTotal = 0;         // 总交换空间 (字节)
    uint64_t swapUsed = 0;          // 已用交换空间 (字节)
};

// 共享内存主结构
struct SharedMemoryBlock {
    uint32_t abiVersion; //0x00010014
    uint32_t writeSequence; // 奇偶协议：启动0
    uint32_t snapshotVersion; // 完整刷新+1
    uint32_t reservedHeader; // 对齐

    uint16_t cpuLogicalCores;
    int16_t cpuUsagePercent_x10; // 未实现 -1
    uint64_t memoryTotalMB;
    uint64_t memoryUsedMB;

    TemperatureSensor tempSensors[32];
    uint16_t tempSensorCount;

    SmartDiskScore smartDisks[16];
    uint8_t smartDiskCount;

    char baseboardManufacturer[128];
    char baseboardProduct[64];
    char baseboardVersion[64];
    char baseboardSerial[64];
    char biosVendor[64];
    char biosVersion[64];
    char biosDate[32];
    uint8_t secureBootEnabled;
    uint8_t tpmPresent;
    uint16_t memorySlotsTotal;
    uint16_t memorySlotsUsed;

    uint8_t futureReserved[64]; // bit0=degradeMode bit1=hashMismatch bit2=sequenceStallWarn
    uint8_t sharedmemHash[32]; // SHA256(结构除自身)

    // USB设备信息
    struct USBDeviceData {
        char drivePath[256];      // 驱动器路径 (Windows: "E:\\", macOS/Linux: "/Volumes/USB")
        char volumeLabel[64];     // 卷标名称
        char devicePath[64];      // 设备路径 (macOS/Linux: "/dev/disk2s1")
        char mountPoint[64];      // 挂载点 (macOS/Linux特有)
        uint64_t totalSize;       // 总容量（字节）
        uint64_t freeSpace;       // 可用空间（字节）
        uint8_t isUpdateReady;    // 是否包含update文件夹
        uint8_t state;            // 状态 (0=Removed, 1=Inserted, 2=UpdateReady)
        uint8_t isRemovable;      // 是否为可移动设备
        uint8_t reserved;         // 对齐
        CrossPlatformTime lastUpdate; // 最后更新时间
    };
    
    USBDeviceData usbDevices[8];  // 最多8个USB设备
    uint8_t usbDeviceCount;       // USB设备数量

    uint8_t extensionPad[118]; //预留 (调整为128-10=118)
};

#pragma pack(pop)

// Optional size check only when C# interop layer is actually used.
// Define CSHARP_INTEROP to enforce a fixed layout for the managed mirror.
#ifdef PLATFORM_WINDOWS
#ifdef CSHARP_INTEROP
constexpr size_t EXPECTED_SHARED_MEMORY_BLOCK_SIZE = sizeof(SharedMemoryBlock); // set explicit value if needed
static_assert(sizeof(SharedMemoryBlock) == EXPECTED_SHARED_MEMORY_BLOCK_SIZE, "SharedMemoryBlock size mismatch (CSHARP_INTEROP) – update managed offsets.");
#endif // CSHARP_INTEROP
#endif // PLATFORM_WINDOWS