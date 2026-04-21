using System.IO;
using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;
using System.Text;
using AvaloniaUI.Models;
using Serilog;

namespace AvaloniaUI.Services;

public class SharedMemoryService : IDisposable
{
    private MemoryMappedFile? _mmf;
    private MemoryMappedViewAccessor? _accessor;
    private readonly object _lock = new();
    private bool _disposed = false;

    private const string SHARED_MEMORY_NAME = "SystemMonitorSharedMemory";
    private const string GLOBAL_SHARED_MEMORY_NAME = "Global\\SystemMonitorSharedMemory";
    private const string LOCAL_SHARED_MEMORY_NAME = "Local\\SystemMonitorSharedMemory";

    public bool IsInitialized { get; private set; }
    public string LastError { get; private set; } = string.Empty;

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
        public double cpuUsageSampleIntervalMs;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)]
        public GPUDataStruct[] gpus;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public NetworkAdapterStruct[] adapters;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public SharedDiskDataStruct[] disks;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public PhysicalDiskSmartDataStruct[] physicalDisks;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 10)]
        public TemperatureDataStruct[] temperatures;
        public int adapterCount;
        public int tempCount;
        public int gpuCount;
        public int diskCount;
        public int physicalDiskCount;
        public TpmInfoStruct tpm;
        public byte tpmCount;
        public SYSTEMTIME lastUpdate;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 40)]
        public byte[] lockData;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct TpmInfoStruct
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)] public ushort[] manufacturer;
        public ushort vendorId;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)] public ushort[] firmwareVersion;
        public byte firmwareVersionMajor;
        public byte firmwareVersionMinor;
        public byte firmwareVersionBuild;
        public uint supportedAlgorithms;
        public uint activeAlgorithms;
        public byte status;
        public byte selfTestStatus;
        public ulong totalVotes;
        [MarshalAs(UnmanagedType.I1)] public bool isPresent;
        [MarshalAs(UnmanagedType.I1)] public bool isEnabled;
        [MarshalAs(UnmanagedType.I1)] public bool isActive;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct GPUDataStruct
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 128)] public ushort[] name;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)] public ushort[] brand;
        public ulong memory;
        public double coreClock;
        [MarshalAs(UnmanagedType.I1)] public bool isVirtual;
        public double usage;
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
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)] public byte[] logicalDriveLetters;
        public int logicalDriveCount;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 256)] public ushort[] partitionLabels; // 8 x 32 = 256
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

            // MemoryMappedFile is Windows-only; skip on other platforms
            if (!OperatingSystem.IsWindows())
            {
                Log.Debug("SharedMemoryService: not supported on this platform (non-Windows)");
                return false;
            }

            try
            {
                string[] names = { GLOBAL_SHARED_MEMORY_NAME, LOCAL_SHARED_MEMORY_NAME, SHARED_MEMORY_NAME };
                int structSize = Marshal.SizeOf<SharedMemoryBlock>();

                foreach (string name in names)
                {
                    try
                    {
                        _mmf = MemoryMappedFile.OpenExisting(name, MemoryMappedFileRights.Read);
                        _accessor = _mmf.CreateViewAccessor(0, structSize, MemoryMappedFileAccess.Read);
                        IsInitialized = true;
                        Log.Information("Connected to shared memory: {Name}, Size={Size} bytes", name, structSize);
                        return true;
                    }
                    catch (FileNotFoundException)
                    {
                        Log.Debug("Shared memory not found: {Name}", name);
                        continue;
                    }
                    catch (Exception ex)
                    {
                        Log.Warning("Failed to open shared memory {Name}: {Message}", name, ex.Message);
                        continue;
                    }
                }

                LastError = "Cannot find shared memory, make sure C++ core is running";
                Log.Error(LastError);
                return false;
            }
            catch (Exception ex)
            {
                LastError = $"Failed to initialize shared memory: {ex.Message}";
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
                LastError = $"Failed to read shared memory: {ex.Message}";
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
            throw new InvalidOperationException("Shared memory not initialized");

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

    private SystemInfo ConvertToSystemInfo(SharedMemoryBlock sharedData)
    {
        // Validate data - check if cpuName is empty (data not yet written by C++ core)
        string? cpuName = SafeWideCharArrayToString(sharedData.cpuName);
        if (string.IsNullOrEmpty(cpuName) || sharedData.physicalCores == 0)
        {
            // Data not yet initialized by C++ core
            return new SystemInfo { CpuName = "等待数据..." };
        }

        var systemInfo = new SystemInfo();
        try
        {
            systemInfo.CpuName = cpuName ?? "Unknown CPU";
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

            // GPU - 过滤无效设备
            systemInfo.Gpus.Clear();
            if (sharedData.gpus != null)
            {
                for (int i = 0; i < Math.Min(sharedData.gpuCount, sharedData.gpus.Length); i++)
                {
                    var g = sharedData.gpus[i];
                    var name = SafeWideCharArrayToString(g.name);
                    // 过滤无效名称
                    if (string.IsNullOrWhiteSpace(name) || name.Contains("Unknown", StringComparison.OrdinalIgnoreCase))
                        continue;
                    systemInfo.Gpus.Add(new GpuData
                    {
                        Name = name,
                        Brand = SafeWideCharArrayToString(g.brand) ?? "",
                        Memory = g.memory,
                        CoreClock = g.coreClock,
                        IsVirtual = g.isVirtual,
                        Usage = g.usage
                    });
                }
            }

            // Network - 过滤无效设备
            systemInfo.Adapters.Clear();
            if (sharedData.adapters != null)
            {
                for (int i = 0; i < Math.Min(sharedData.adapterCount, sharedData.adapters.Length); i++)
                {
                    var a = sharedData.adapters[i];
                    var name = SafeWideCharArrayToString(a.name);
                    // 过滤无效名称
                    if (string.IsNullOrWhiteSpace(name) || name.Contains("Unknown", StringComparison.OrdinalIgnoreCase))
                        continue;
                    systemInfo.Adapters.Add(new NetworkAdapterData
                    {
                        Name = name,
                        Mac = SafeWideCharArrayToString(a.mac) ?? "",
                        IpAddress = SafeWideCharArrayToString(a.ipAddress) ?? "",
                        AdapterType = SafeWideCharArrayToString(a.adapterType) ?? "",
                        Speed = a.speed
                    });
                }
            }

            // Disks - 过滤无效磁盘
            systemInfo.Disks.Clear();
            if (sharedData.disks != null)
            {
                for (int i = 0; i < Math.Min(sharedData.diskCount, sharedData.disks.Length); i++)
                {
                    var d = sharedData.disks[i];
                    // 过滤：容量为0 或 文件系统为 Unknown
                    if (d.totalSize == 0)
                        continue;
                    var fileSystem = SafeWideCharArrayToString(d.fileSystem);
                    if (!string.IsNullOrWhiteSpace(fileSystem) && fileSystem.Contains("Unknown", StringComparison.OrdinalIgnoreCase))
                        continue;
                    systemInfo.Disks.Add(new DiskData
                    {
                        Letter = (char)d.letter,
                        Label = SafeWideCharArrayToString(d.label) ?? "",
                        FileSystem = fileSystem ?? "",
                        TotalSize = d.totalSize,
                        UsedSpace = d.usedSpace,
                        FreeSpace = d.freeSpace,
                        PhysicalDiskIndex = -1
                    });
                }
            }

            // Physical Disks + SMART - 过滤无效磁盘
            systemInfo.PhysicalDisks.Clear();
            if (sharedData.physicalDisks != null)
            {
                for (int i = 0; i < Math.Min(sharedData.physicalDiskCount, sharedData.physicalDisks.Length); i++)
                {
                    var pd = sharedData.physicalDisks[i];
                    var model = SafeWideCharArrayToString(pd.model);
                    // 过滤：容量为0、型号为空或包含 Unknown
                    if (pd.capacity == 0)
                        continue;
                    if (string.IsNullOrWhiteSpace(model) || model.Contains("Unknown", StringComparison.OrdinalIgnoreCase))
                        continue;
                    var physicalDisk = new PhysicalDiskSmartData
                    {
                        Model = model,
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

                    // 读取分区卷标
                    if (pd.partitionLabels != null && pd.partitionLabels.Length > 0)
                    {
                        for (int l = 0; l < physicalDisk.LogicalDriveLetters.Count && l < 8; l++)
                        {
                            // 每个卷标32个ushort，共256字节
                            int offset = l * 32;
                            var labelArray = new ushort[32];
                            Array.Copy(pd.partitionLabels, offset, labelArray, 0, 32);
                            var label = SafeWideCharArrayToString(labelArray);
                            if (!string.IsNullOrWhiteSpace(label))
                            {
                                physicalDisk.PartitionLabels.Add(label);
                            }
                            else
                            {
                                // 没有卷标时使用盘符
                                physicalDisk.PartitionLabels.Add(physicalDisk.LogicalDriveLetters[l].ToString() + ":");
                            }
                        }
                    }

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

            // Map physical disks to logical disks
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

            // Temperatures
            systemInfo.Temperatures.Clear();
            if (sharedData.temperatures != null)
            {
                for (int i = 0; i < Math.Min(sharedData.tempCount, sharedData.temperatures.Length); i++)
                {
                    var t = sharedData.temperatures[i];
                    systemInfo.Temperatures.Add(new TemperatureData
                    {
                        SensorName = SafeWideCharArrayToString(t.sensorName) ?? $"Sensor {i}",
                        Temperature = t.temperature
                    });
                }
            }

            // TPM
            systemInfo.Tpm = null;
            if (sharedData.tpmCount > 0 && sharedData.tpm.isPresent)
            {
                systemInfo.Tpm = new TpmData
                {
                    Manufacturer = SafeWideCharArrayToString(sharedData.tpm.manufacturer) ?? "未知",
                    FirmwareVersion = $"{sharedData.tpm.firmwareVersionMajor}.{sharedData.tpm.firmwareVersionMinor}.{sharedData.tpm.firmwareVersionBuild}",
                    Status = sharedData.tpm.status == 0 ? "就绪" : (sharedData.tpm.status == 1 ? "错误" : "已禁用"),
                    SelfTestStatus = sharedData.tpm.selfTestStatus == 0 ? "成功" : "失败",
                    IsEnabled = sharedData.tpm.isEnabled,
                    IsActive = sharedData.tpm.isActive
                };
            }

            systemInfo.LastUpdate = DateTime.Now;
            Log.Debug("Read from shared memory: CPU={CPU}, GPU={GPU}, Net={Net}, Disk={Disk}, Phys={Phys}",
                systemInfo.CpuName, systemInfo.Gpus.Count, systemInfo.Adapters.Count, systemInfo.Disks.Count, systemInfo.PhysicalDisks.Count);
            return systemInfo;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to convert shared memory data");
            return new SystemInfo { CpuName = "Conversion failed" };
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