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

        private const string SHARED_MEMORY_NAME = "SystemMonitorSharedMemory";
        private const string GLOBAL_SHARED_MEMORY_NAME = "Global\\SystemMonitorSharedMemory";
        private const string LOCAL_SHARED_MEMORY_NAME = "Local\\SystemMonitorSharedMemory";
        private const int SHARED_MEMORY_SIZE = 65536; // 64KB

        public bool IsInitialized { get; private set; }
        public string LastError { get; private set; } = string.Empty;

        // C++数据结构定义 - 与C++端完全匹配
        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        public struct SharedMemoryBlock
        {
            // CPU信息 - 使用wchar_t数组
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 128)]
            public ushort[] cpuName; // wchar_t 在Windows上是16位
            
            public int physicalCores;
            public int logicalCores;
            public double cpuUsage;
            public int performanceCores;
            public int efficiencyCores;
            public double pCoreFreq;
            public double eCoreFreq;
            public bool hyperThreading;
            public bool virtualization;
            
            // 内存信息
            public ulong totalMemory;
            public ulong usedMemory;
            public ulong availableMemory;
            
            // 温度信息
            public double cpuTemperature;
            public double gpuTemperature;
            
            // GPU信息（支持最多2个GPU）
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)]
            public GPUDataStruct[] gpus;
            
            // 网络适配器（支持最多4个适配器）
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
            public NetworkAdapterStruct[] adapters;
            
            // 逻辑磁盘信息（支持最多8个磁盘）
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
            public SharedDiskDataStruct[] disks;
            
            // 物理磁盘SMART信息（支持最多8个物理磁盘）
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
            public PhysicalDiskSmartDataStruct[] physicalDisks;
            
            // 温度数据（支持10个传感器）
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 10)]
            public TemperatureDataStruct[] temperatures;
            
            // 计数器
            public int adapterCount;
            public int tempCount;
            public int gpuCount;
            public int diskCount;
            public int physicalDiskCount;
            
            // 最后更新时间
            public SYSTEMTIME lastUpdate;
            
            // CRITICAL_SECTION (在C#中忽略)
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 24)]
            public byte[] lockData;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        public struct GPUDataStruct
        {
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 128)]
            public ushort[] name;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)]
            public ushort[] brand;
            public ulong memory;
            public double coreClock;
            public bool isVirtual;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        public struct NetworkAdapterStruct
        {
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 128)]
            public ushort[] name;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
            public ushort[] mac;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)]
            public ushort[] ipAddress;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
            public ushort[] adapterType;
            public ulong speed;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        public struct SharedDiskDataStruct
        {
            public byte letter; // char 是8位
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 128)]
            public ushort[] label;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
            public ushort[] fileSystem;
            public ulong totalSize;
            public ulong usedSpace;
            public ulong freeSpace;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        public struct TemperatureDataStruct
        {
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)]
            public ushort[] sensorName;
            public double temperature;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        public struct PhysicalDiskSmartDataStruct
        {
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 128)]
            public ushort[] model;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)]
            public ushort[] serialNumber;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
            public ushort[] firmwareVersion;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
            public ushort[] interfaceType;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
            public ushort[] diskType;
            public ulong capacity;
            public double temperature;
            public byte healthPercentage;
            public bool isSystemDisk;
            public bool smartEnabled;
            public bool smartSupported;
            // ... 其他SMART数据省略以保持简洁
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
                    // 尝试多种命名方式打开共享内存
                    string[] names = { GLOBAL_SHARED_MEMORY_NAME, LOCAL_SHARED_MEMORY_NAME, SHARED_MEMORY_NAME };
                    
                    foreach (string name in names)
                    {
                        try
                        {
                            Log.Debug($"尝试打开共享内存: {name}");
                            
                            // 尝试打开现有的共享内存
                            _mmf = MemoryMappedFile.OpenExisting(name, MemoryMappedFileRights.Read);
                            _accessor = _mmf.CreateViewAccessor(0, SHARED_MEMORY_SIZE, MemoryMappedFileAccess.Read);
                            
                            IsInitialized = true;
                            Log.Information($"? 成功连接到共享内存: {name}");
                            
                            return true;
                        }
                        catch (FileNotFoundException)
                        {
                            Log.Debug($"? 共享内存不存在: {name}");
                            continue;
                        }
                        catch (Exception ex)
                        {
                            Log.Warning($"?? 打开共享内存失败 {name}: {ex.Message}");
                            continue;
                        }
                    }

                    LastError = "无法找到共享内存，请确保C++主程序正在运行";
                    Log.Error($"?? {LastError}");
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
                    // 读取完整的共享内存数据
                    return ReadCompleteSystemInfo();
                }
                catch (Exception ex)
                {
                    LastError = $"读取共享内存数据时发生错误: {ex.Message}";
                    Log.Error(ex, LastError);
                    
                    // 尝试重新初始化
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

            try
            {
                // 计算结构体大小并读取数据
                int structSize = Marshal.SizeOf<SharedMemoryBlock>();
                var buffer = new byte[structSize];
                
                // 读取字节数据
                int bytesToRead = Math.Min(structSize, (int)_accessor.Capacity);
                _accessor.ReadArray(0, buffer, 0, bytesToRead);

                // 转换为结构体
                var handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
                try
                {
                    var sharedData = Marshal.PtrToStructure<SharedMemoryBlock>(handle.AddrOfPinnedObject());
                    return ConvertToSystemInfo(sharedData);
                }
                finally
                {
                    handle.Free();
                }
            }
            catch (Exception ex)
            {
                Log.Error(ex, "读取完整系统信息失败，尝试使用简化方法");
                
                // 如果结构体解析失败，尝试简化的读取方法
                return ReadSimplifiedSystemInfo();
            }
        }

        private SystemInfo ReadSimplifiedSystemInfo()
        {
            var systemInfo = new SystemInfo();

            try
            {
                // 尝试直接读取关键数据字段
                // CPU名称 (偏移量0，wchar_t[128])
                string cpuName = ReadWideString(0, 128);
                if (!string.IsNullOrWhiteSpace(cpuName))
                {
                    systemInfo.CpuName = cpuName;
                }
                else
                {
                    systemInfo.CpuName = "无法读取CPU信息";
                }

                // 核心数和内存信息（根据C++结构体计算偏移量）
                int offset = 128 * 2; // 跳过wchar_t[128] cpuName
                
                try
                {
                    systemInfo.PhysicalCores = _accessor!.ReadInt32(offset);
                    systemInfo.LogicalCores = _accessor.ReadInt32(offset + 4);
                    systemInfo.CpuUsage = _accessor.ReadDouble(offset + 8);
                    systemInfo.PerformanceCores = _accessor.ReadInt32(offset + 16);
                    systemInfo.EfficiencyCores = _accessor.ReadInt32(offset + 20);
                    
                    // 跳到内存数据（大约偏移量 offset + 40）
                    int memOffset = offset + 40;
                    systemInfo.TotalMemory = _accessor.ReadUInt64(memOffset);
                    systemInfo.UsedMemory = _accessor.ReadUInt64(memOffset + 8);
                    systemInfo.AvailableMemory = _accessor.ReadUInt64(memOffset + 16);
                    
                    // 温度数据（大约偏移量 memOffset + 24）
                    int tempOffset = memOffset + 24;
                    systemInfo.CpuTemperature = _accessor.ReadDouble(tempOffset);
                    systemInfo.GpuTemperature = _accessor.ReadDouble(tempOffset + 8);
                    
                    Log.Debug($"简化读取成功: CPU={systemInfo.CpuName}, 内存={systemInfo.TotalMemory/(1024*1024*1024)}GB");
                }
                catch (Exception ex)
                {
                    Log.Warning($"读取数值数据失败: {ex.Message}");
                    // 保持默认值
                }

                systemInfo.LastUpdate = DateTime.Now;
                return systemInfo;
            }
            catch (Exception ex)
            {
                Log.Error(ex, "简化读取也失败");
                
                return new SystemInfo
                {
                    CpuName = $"数据读取失败: {ex.Message}",
                    PhysicalCores = 0,
                    LogicalCores = 0,
                    TotalMemory = 0,
                    LastUpdate = DateTime.Now
                };
            }
        }

        private SystemInfo ConvertToSystemInfo(SharedMemoryBlock sharedData)
        {
            var systemInfo = new SystemInfo();
            
            try
            {
                // CPU信息
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

                // 内存信息
                systemInfo.TotalMemory = sharedData.totalMemory;
                systemInfo.UsedMemory = sharedData.usedMemory;
                systemInfo.AvailableMemory = sharedData.availableMemory;

                // 温度信息
                systemInfo.CpuTemperature = sharedData.cpuTemperature;
                systemInfo.GpuTemperature = sharedData.gpuTemperature;

                // GPU信息
                systemInfo.Gpus.Clear();
                for (int i = 0; i < Math.Min(sharedData.gpuCount, sharedData.gpus?.Length ?? 0); i++)
                {
                    var gpu = sharedData.gpus[i];
                    systemInfo.Gpus.Add(new GpuData
                    {
                        Name = SafeWideCharArrayToString(gpu.name) ?? "未知GPU",
                        Brand = SafeWideCharArrayToString(gpu.brand) ?? "未知品牌",
                        Memory = gpu.memory,
                        CoreClock = gpu.coreClock,
                        IsVirtual = gpu.isVirtual
                    });
                }

                // 网络适配器信息
                systemInfo.Adapters.Clear();
                for (int i = 0; i < Math.Min(sharedData.adapterCount, sharedData.adapters?.Length ?? 0); i++)
                {
                    var adapter = sharedData.adapters[i];
                    systemInfo.Adapters.Add(new NetworkAdapterData
                    {
                        Name = SafeWideCharArrayToString(adapter.name) ?? "未知网卡",
                        Mac = SafeWideCharArrayToString(adapter.mac) ?? "00-00-00-00-00-00",
                        IpAddress = SafeWideCharArrayToString(adapter.ipAddress) ?? "未分配",
                        AdapterType = SafeWideCharArrayToString(adapter.adapterType) ?? "未知类型",
                        Speed = adapter.speed
                    });
                }

                // 磁盘信息
                systemInfo.Disks.Clear();
                for (int i = 0; i < Math.Min(sharedData.diskCount, sharedData.disks?.Length ?? 0); i++)
                {
                    var disk = sharedData.disks[i];
                    systemInfo.Disks.Add(new DiskData
                    {
                        Letter = (char)disk.letter,
                        Label = SafeWideCharArrayToString(disk.label) ?? "未命名",
                        FileSystem = SafeWideCharArrayToString(disk.fileSystem) ?? "未知",
                        TotalSize = disk.totalSize,
                        UsedSpace = disk.usedSpace,
                        FreeSpace = disk.freeSpace
                    });
                }

                // 温度传感器信息
                systemInfo.Temperatures.Clear();
                for (int i = 0; i < Math.Min(sharedData.tempCount, sharedData.temperatures?.Length ?? 0); i++)
                {
                    var temp = sharedData.temperatures[i];
                    string sensorName = SafeWideCharArrayToString(temp.sensorName) ?? $"传感器{i}";
                    systemInfo.Temperatures.Add(new TemperatureData
                    {
                        SensorName = sensorName,
                        Temperature = temp.temperature
                    });
                }

                // 设置兼容性字段
                if (systemInfo.Gpus.Count > 0)
                {
                    var firstGpu = systemInfo.Gpus[0];
                    systemInfo.GpuName = firstGpu.Name;
                    systemInfo.GpuBrand = firstGpu.Brand;
                    systemInfo.GpuMemory = firstGpu.Memory;
                    systemInfo.GpuCoreFreq = firstGpu.CoreClock;
                    systemInfo.GpuIsVirtual = firstGpu.IsVirtual;
                }

                if (systemInfo.Adapters.Count > 0)
                {
                    var firstAdapter = systemInfo.Adapters[0];
                    systemInfo.NetworkAdapterName = firstAdapter.Name;
                    systemInfo.NetworkAdapterMac = firstAdapter.Mac;
                    systemInfo.NetworkAdapterIp = firstAdapter.IpAddress;
                    systemInfo.NetworkAdapterType = firstAdapter.AdapterType;
                    systemInfo.NetworkAdapterSpeed = firstAdapter.Speed;
                }

                systemInfo.LastUpdate = DateTime.Now;

                Log.Debug($"? 成功解析系统信息: CPU={systemInfo.CpuName}, 内存={systemInfo.TotalMemory / (1024*1024*1024)}GB, GPU数量={systemInfo.Gpus.Count}, 网卡数量={systemInfo.Adapters.Count}, 磁盘数量={systemInfo.Disks.Count}");

                return systemInfo;
            }
            catch (Exception ex)
            {
                Log.Error(ex, "转换系统信息时发生错误，回退到简化方法");
                return ReadSimplifiedSystemInfo();
            }
        }

        private string ReadWideString(int offset, int maxChars)
        {
            try
            {
                var chars = new List<char>();
                for (int i = 0; i < maxChars; i++)
                {
                    ushort wchar = _accessor!.ReadUInt16(offset + i * 2);
                    if (wchar == 0) break;
                    chars.Add((char)wchar);
                }
                return new string(chars.ToArray()).Trim();
            }
            catch
            {
                return string.Empty;
            }
        }

        private string? SafeWideCharArrayToString(ushort[]? wcharArray)
        {
            if (wcharArray == null || wcharArray.Length == 0)
                return null;

            try
            {
                var chars = new List<char>();
                foreach (ushort wchar in wcharArray)
                {
                    if (wchar == 0) break;
                    chars.Add((char)wchar);
                }
                
                if (chars.Count == 0)
                    return null;

                string result = new string(chars.ToArray()).Trim();
                return string.IsNullOrWhiteSpace(result) ? null : result;
            }
            catch (Exception ex)
            {
                Log.Debug($"宽字符数组转换失败: {ex.Message}");
                return null;
            }
        }

        public void Dispose()
        {
            if (_disposed)
                return;

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

        ~SharedMemoryService()
        {
            Dispose();
        }
    }
}