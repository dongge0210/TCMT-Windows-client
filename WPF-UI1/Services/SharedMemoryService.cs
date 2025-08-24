using System.IO;
using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;
using System.Text;
using WPF_UI1.Models;
using Serilog;

namespace WPF_UI1.Services
{
    public class SharedMemoryService : IDisposable
    {
        private MemoryMappedFile? _mmf;
        private MemoryMappedViewAccessor? _accessor;
        private readonly object _lock = new();
        private bool _disposed = false;

        // 注意：C++端 CreateFileMapping 使用 sizeof(SharedMemoryBlock) (~>120KB+)
        // 此处不再使用固定 64KB，而是动态计算结构体大小
        private const string SHARED_MEMORY_NAME = "SystemMonitorSharedMemory";
        private const string GLOBAL_SHARED_MEMORY_NAME = "Global\\SystemMonitorSharedMemory";
        private const string LOCAL_SHARED_MEMORY_NAME = "Local\\SystemMonitorSharedMemory";

        public bool IsInitialized { get; private set; }
        public string LastError { get; private set; } = string.Empty;

        // 与C++结构严格匹配（#pragma pack(1) 且 bool 为1字节）
        // 统一指定 Pack=1, 并对每个bool加 MarshalAs(UnmanagedType.I1)
        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        public struct SharedMemoryBlock
        {
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 128)]
            public ushort[] cpuName;
            public int physicalCores;
            public int logicalCores;
            public double cpuUsage;
            public int performanceCores;
            public int efficiencyCores;
            public double pCoreFreq;
            public double eCoreFreq;
            [MarshalAs(UnmanagedType.I1)] public bool hyperThreading;
            [MarshalAs(UnmanagedType.I1)] public bool virtualization;
            public ulong totalMemory;
            public ulong usedMemory;
            public ulong availableMemory;
            public double cpuTemperature;
            public double gpuTemperature;
            public double cpuUsageSampleIntervalMs; // 新增：CPU采样间隔
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)]
            public GPUDataStruct[] gpus;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
            public NetworkAdapterStruct[] adapters;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
            public SharedDiskDataStruct[] disks;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
            public PhysicalDiskSmartDataStruct[] physicalDisks; // 大结构，仅为保持偏移
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 10)]
            public TemperatureDataStruct[] temperatures;
            public int adapterCount;
            public int tempCount;
            public int gpuCount;
            public int diskCount;
            public int physicalDiskCount;
            public SYSTEMTIME lastUpdate;
            // CRITICAL_SECTION: 在 Win64 下一般 40 字节（pack=1），用占位符字节数组
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 40)]
            public byte[] lockData;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        public struct GPUDataStruct
        {
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 128)] public ushort[] name;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)] public ushort[] brand;
            public ulong memory;
            public double coreClock;
            [MarshalAs(UnmanagedType.I1)] public bool isVirtual;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        public struct NetworkAdapterStruct
        {
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 128)] public ushort[] name;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)] public ushort[] mac;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)] public ushort[] ipAddress;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)] public ushort[] adapterType;
            public ulong speed;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        public struct SharedDiskDataStruct
        {
            public byte letter;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 128)] public ushort[] label;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)] public ushort[] fileSystem;
            public ulong totalSize;
            public ulong usedSpace;
            public ulong freeSpace;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        public struct TemperatureDataStruct
        {
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)] public ushort[] sensorName;
            public double temperature;
        }

        // ------------- SMART 相关子结构（用于保持偏移一致）-------------
        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        public struct SmartAttributeDataStruct
        {
            public byte id;
            public byte flags;
            public byte current;
            public byte worst;
            public byte threshold;
            public ulong rawValue;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)] public ushort[] name;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 128)] public ushort[] description;
            [MarshalAs(UnmanagedType.I1)] public bool isCritical;
            public double physicalValue;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)] public ushort[] units;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        public struct PhysicalDiskSmartDataStruct
        {
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 128)] public ushort[] model;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)] public ushort[] serialNumber;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)] public ushort[] firmwareVersion;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)] public ushort[] interfaceType;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)] public ushort[] diskType;
            public ulong capacity;
            public double temperature;
            public byte healthPercentage;
            [MarshalAs(UnmanagedType.I1)] public bool isSystemDisk;
            [MarshalAs(UnmanagedType.I1)] public bool smartEnabled;
            [MarshalAs(UnmanagedType.I1)] public bool smartSupported;
            // SMART 属性 32 个
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)] public SmartAttributeDataStruct[] attributes;
            public int attributeCount;
            public ulong powerOnHours;
            public ulong powerCycleCount;
            public ulong reallocatedSectorCount;
            public ulong currentPendingSector;
            public ulong uncorrectableErrors;
            public double wearLeveling;
            public ulong totalBytesWritten;
            public ulong totalBytesRead;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)] public byte[] logicalDriveLetters; // char[8]
            public int logicalDriveCount;
            public SYSTEMTIME lastScanTime;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct SYSTEMTIME
        {
            public ushort wYear;
            public ushort wMonth;
            public ushort wDayOfWeek;
            public ushort wDay;
            public ushort wHour;
            public ushort wMinute;
            public ushort wSecond;
            public ushort wMilliseconds;
        }

        public bool Initialize()
        {
            lock (_lock)
            {
                if (IsInitialized)
                    return true;

                try
                {
                    string[] names = { GLOBAL_SHARED_MEMORY_NAME, LOCAL_SHARED_MEMORY_NAME, SHARED_MEMORY_NAME };
                    int structSize = Marshal.SizeOf<SharedMemoryBlock>();

                    foreach (string name in names)
                    {
                        try
                        {
                            Log.Debug($"尝试打开共享内存: {name}");
                            _mmf = MemoryMappedFile.OpenExisting(name, MemoryMappedFileRights.Read);
                            // 视图长度使用真实结构体大小
                            _accessor = _mmf.CreateViewAccessor(0, structSize, MemoryMappedFileAccess.Read);
                            IsInitialized = true;
                            Log.Information($"? 成功连接到共享内存: {name}, Size={structSize} bytes");
                            return true;
                        }
                        catch (FileNotFoundException)
                        {
                            Log.Debug($"共享内存不存在: {name}");
                            continue;
                        }
                        catch (Exception ex)
                        {
                            Log.Warning($"打开共享内存失败 {name}: {ex.Message}");
                            continue;
                        }
                    }

                    LastError = "无法找到共享内存，请确保C++主程序正在运行";
                    Log.Error(LastError);
                    return false;
                }
                catch (Exception ex)
                {
                    LastError = $"初始化共享内存时发生错误: {ex.Message}";
                    Log.Error(ex, LastError);
                    return false;
                }
            }
        }

        public SystemInfo? ReadSystemInfo()
        {
            lock (_lock)
            {
                if (!IsInitialized || _accessor == null)
                {
                    if (!Initialize())
                        return null;
                }

                try
                {
                    return ReadCompleteSystemInfo();
                }
                catch (Exception ex)
                {
                    LastError = $"读取共享内存数据时发生错误: {ex.Message}";
                    Log.Error(ex, LastError);
                    Dispose();
                    IsInitialized = false;
                    return null;
                }
            }
        }

        private SystemInfo ReadCompleteSystemInfo()
        {
            if (_accessor == null)
                throw new InvalidOperationException("共享内存访问器未初始化");

            int structSize = Marshal.SizeOf<SharedMemoryBlock>();
            var raw = new byte[structSize];
            int bytesToRead = (int)Math.Min((long)structSize, _accessor.Capacity);
            _accessor.ReadArray(0, raw, 0, bytesToRead);

            var handle = GCHandle.Alloc(raw, GCHandleType.Pinned);
            try
            {
                var data = Marshal.PtrToStructure<SharedMemoryBlock>(handle.AddrOfPinnedObject());
                return ConvertToSystemInfo(data);
            }
            finally
            {
                handle.Free();
            }
        }

        // 简化读取保留（结构匹配后通常不再触发）
        private SystemInfo ReadSimplifiedSystemInfo()
        {
            var systemInfo = new SystemInfo
            {
                CpuName = "结构不匹配 - 使用降级读取",
                LastUpdate = DateTime.Now
            };
            return systemInfo;
        }

        private SystemInfo ConvertToSystemInfo(SharedMemoryBlock sharedData)
        {
            var systemInfo = new SystemInfo();
            try
            {
                systemInfo.CpuName = SafeWideCharArrayToString(sharedData.cpuName) ?? "未知处理器";
                systemInfo.PhysicalCores = sharedData.physicalCores;
                systemInfo.LogicalCores = sharedData.logicalCores;
                systemInfo.PerformanceCores = sharedData.performanceCores;
                systemInfo.EfficiencyCores = sharedData.efficiencyCores;
                systemInfo.CpuUsage = sharedData.cpuUsage;
                systemInfo.PerformanceCoreFreq = sharedData.pCoreFreq;
                systemInfo.EfficiencyCoreFreq = sharedData.eCoreFreq;
                systemInfo.HyperThreading = sharedData.hyperThreading;
                systemInfo.Virtualization = sharedData.virtualization;
                systemInfo.TotalMemory = sharedData.totalMemory;
                systemInfo.UsedMemory = sharedData.usedMemory;
                systemInfo.AvailableMemory = sharedData.availableMemory;
                systemInfo.CpuTemperature = sharedData.cpuTemperature;
                systemInfo.GpuTemperature = sharedData.gpuTemperature;
                systemInfo.CpuUsageSampleIntervalMs = sharedData.cpuUsageSampleIntervalMs;

                // GPU
                systemInfo.Gpus.Clear();
                if (sharedData.gpus != null)
                {
                    for (int i = 0; i < Math.Min(sharedData.gpuCount, sharedData.gpus.Length); i++)
                    {
                        var g = sharedData.gpus[i];
                        systemInfo.Gpus.Add(new GpuData
                        {
                            Name = SafeWideCharArrayToString(g.name) ?? "未知GPU",
                            Brand = SafeWideCharArrayToString(g.brand) ?? "未知品牌",
                            Memory = g.memory,
                            CoreClock = g.coreClock,
                            IsVirtual = g.isVirtual
                        });
                    }
                }
                if (systemInfo.Gpus.Count > 0)
                {
                    var fg = systemInfo.Gpus[0];
                    systemInfo.GpuName = fg.Name;
                    systemInfo.GpuBrand = fg.Brand;
                    systemInfo.GpuMemory = fg.Memory;
                    systemInfo.GpuCoreFreq = fg.CoreClock;
                    systemInfo.GpuIsVirtual = fg.IsVirtual;
                }

                // 网络
                systemInfo.Adapters.Clear();
                if (sharedData.adapters != null)
                {
                    for (int i = 0; i < Math.Min(sharedData.adapterCount, sharedData.adapters.Length); i++)
                    {
                        var a = sharedData.adapters[i];
                        systemInfo.Adapters.Add(new NetworkAdapterData
                        {
                            Name = SafeWideCharArrayToString(a.name) ?? "未知网卡",
                            Mac = SafeWideCharArrayToString(a.mac) ?? "00-00-00-00-00-00",
                            IpAddress = SafeWideCharArrayToString(a.ipAddress) ?? "未分配",
                            AdapterType = SafeWideCharArrayToString(a.adapterType) ?? "未知类型",
                            Speed = a.speed
                        });
                    }
                }
                if (systemInfo.Adapters.Count > 0)
                {
                    var fa = systemInfo.Adapters[0];
                    systemInfo.NetworkAdapterName = fa.Name;
                    systemInfo.NetworkAdapterMac = fa.Mac;
                    systemInfo.NetworkAdapterIp = fa.IpAddress;
                    systemInfo.NetworkAdapterType = fa.AdapterType;
                    systemInfo.NetworkAdapterSpeed = fa.Speed;
                }

                // 逻辑磁盘
                systemInfo.Disks.Clear();
                if (sharedData.disks != null)
                {
                    for (int i = 0; i < Math.Min(sharedData.diskCount, sharedData.disks.Length); i++)
                    {
                        var d = sharedData.disks[i];
                        systemInfo.Disks.Add(new DiskData
                        {
                            Letter = (char)d.letter,
                            Label = SafeWideCharArrayToString(d.label) ?? "未命名",
                            FileSystem = SafeWideCharArrayToString(d.fileSystem) ?? "未知",
                            TotalSize = d.totalSize,
                            UsedSpace = d.usedSpace,
                            FreeSpace = d.freeSpace,
                            PhysicalDiskIndex = -1 // 初始为未关联
                        });
                    }
                }

                // 物理磁盘 + SMART
                systemInfo.PhysicalDisks.Clear();
                if (sharedData.physicalDisks != null)
                {
                    for (int i = 0; i < Math.Min(sharedData.physicalDiskCount, sharedData.physicalDisks.Length); i++)
                    {
                        var pd = sharedData.physicalDisks[i];
                        var physicalDisk = new PhysicalDiskSmartData
                        {
                            Model = SafeWideCharArrayToString(pd.model) ?? "未知型号",
                            SerialNumber = SafeWideCharArrayToString(pd.serialNumber) ?? string.Empty,
                            FirmwareVersion = SafeWideCharArrayToString(pd.firmwareVersion) ?? string.Empty,
                            InterfaceType = SafeWideCharArrayToString(pd.interfaceType) ?? string.Empty,
                            DiskType = SafeWideCharArrayToString(pd.diskType) ?? string.Empty,
                            Capacity = pd.capacity,
                            Temperature = pd.temperature,
                            HealthPercentage = pd.healthPercentage,
                            IsSystemDisk = pd.isSystemDisk,
                            SmartEnabled = pd.smartEnabled,
                            SmartSupported = pd.smartSupported,
                            PowerOnHours = pd.powerOnHours,
                            PowerCycleCount = pd.powerCycleCount,
                            ReallocatedSectorCount = pd.reallocatedSectorCount,
                            CurrentPendingSector = pd.currentPendingSector,
                            UncorrectableErrors = pd.uncorrectableErrors,
                            WearLeveling = pd.wearLeveling,
                            TotalBytesWritten = pd.totalBytesWritten,
                            TotalBytesRead = pd.totalBytesRead
                        };

                        // 关联逻辑驱动器字母
                        if (pd.logicalDriveLetters != null && pd.logicalDriveLetters.Length > 0)
                        {
                            for (int b = 0; b < Math.Min(pd.logicalDriveLetters.Length, pd.logicalDriveCount); b++)
                            {
                                byte letterByte = pd.logicalDriveLetters[b];
                                if (letterByte == 0) break;
                                char letter = (char)letterByte;
                                if (char.IsLetter(letter))
                                {
                                    physicalDisk.LogicalDriveLetters.Add(letter);
                                }
                            }
                        }

                        // SMART 属性
                        physicalDisk.Attributes.Clear();
                        if (pd.attributes != null)
                        {
                            int attrCount = Math.Min(pd.attributeCount, pd.attributes.Length);
                            for (int a = 0; a < attrCount; a++)
                            {
                                var sa = pd.attributes[a];
                                var attr = new SmartAttributeData
                                {
                                    Id = sa.id,
                                    Current = sa.current,
                                    Worst = sa.worst,
                                    Threshold = sa.threshold,
                                    RawValue = sa.rawValue,
                                    Name = SafeWideCharArrayToString(sa.name) ?? $"Attr {sa.id}",
                                    Description = SafeWideCharArrayToString(sa.description) ?? string.Empty,
                                    IsCritical = sa.isCritical,
                                    PhysicalValue = sa.physicalValue,
                                    Units = SafeWideCharArrayToString(sa.units) ?? string.Empty
                                };
                                physicalDisk.Attributes.Add(attr);
                            }
                        }

                        systemInfo.PhysicalDisks.Add(physicalDisk);
                    }
                }

                // 建立逻辑盘 -> 物理盘索引映射
                if (systemInfo.PhysicalDisks.Count > 0 && systemInfo.Disks.Count > 0)
                {
                    for (int pi = 0; pi < systemInfo.PhysicalDisks.Count; pi++)
                    {
                        var pd = systemInfo.PhysicalDisks[pi];
                        foreach (var drvLetter in pd.LogicalDriveLetters)
                        {
                            for (int di = 0; di < systemInfo.Disks.Count; di++)
                            {
                                if (char.ToUpperInvariant(systemInfo.Disks[di].Letter) == char.ToUpperInvariant(drvLetter))
                                {
                                    systemInfo.Disks[di].PhysicalDiskIndex = pi;
                                }
                            }
                        }
                    }
                }

                // 温度传感器
                systemInfo.Temperatures.Clear();
                if (sharedData.temperatures != null)
                {
                    for (int i = 0; i < Math.Min(sharedData.tempCount, sharedData.temperatures.Length); i++)
                    {
                        var t = sharedData.temperatures[i];
                        systemInfo.Temperatures.Add(new TemperatureData
                        {
                            SensorName = SafeWideCharArrayToString(t.sensorName) ?? $"传感器{i}",
                            Temperature = t.temperature
                        });
                    }
                }

                systemInfo.LastUpdate = DateTime.Now;
                Log.Debug($"解析共享内存成功: CPU={systemInfo.CpuName}, GPU数={systemInfo.Gpus.Count}, 网卡数={systemInfo.Adapters.Count}, 逻辑盘={systemInfo.Disks.Count}, 物理盘={systemInfo.PhysicalDisks.Count}");
                return systemInfo;
            }
            catch (Exception ex)
            {
                Log.Error(ex, "转换共享内存数据失败，回退简化模式");
                return ReadSimplifiedSystemInfo();
            }
        }

        private string? SafeWideCharArrayToString(ushort[]? wcharArray)
        {
            if (wcharArray == null || wcharArray.Length == 0) return null;
            try
            {
                int len = 0;
                while (len < wcharArray.Length && wcharArray[len] != 0) len++;
                if (len == 0) return null;
                var chars = new char[len];
                for (int i = 0; i < len; i++) chars[i] = (char)wcharArray[i];
                var s = new string(chars).Trim();
                return string.IsNullOrWhiteSpace(s) ? null : s;
            }
            catch { return null; }
        }

        public void Dispose()
        {
            if (_disposed) return;
            lock (_lock)
            {
                _accessor?.Dispose();
                _mmf?.Dispose();
                _accessor = null;
                _mmf = null;
                IsInitialized = false;
                _disposed = true;
            }
            GC.SuppressFinalize(this);
        }

        ~SharedMemoryService() => Dispose();
    }
}