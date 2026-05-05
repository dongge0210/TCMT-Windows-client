using AvaloniaUI.Models;
using Serilog;

namespace AvaloniaUI.Services.IPC;

public static class IPCSystemInfoMapper
{
    public static SystemInfo Read(IPCService ipc)
    {
        var reader = ipc.Reader;
        if (!reader.IsOpen)
            return null!;

        try
        {
            var info = new SystemInfo
            {
                LastUpdate = DateTime.Now
            };

            // CPU
            info.CpuName = ipc.ReadWString("cpu/name") ?? reader.ReadString("cpu/name") ?? "";
            info.PhysicalCores = reader.ReadInt32("cpu/cores/physical") ?? reader.ReadUInt8("cpu/cores/physical") ?? 0;
            info.LogicalCores = reader.ReadInt32("cpu/cores/logical") ?? reader.ReadUInt8("cpu/cores/logical") ?? 0;
            info.PerformanceCores = reader.ReadInt32("cpu/cores/performance") ?? reader.ReadUInt8("cpu/cores/performance") ?? 0;
            info.EfficiencyCores = reader.ReadInt32("cpu/cores/efficiency") ?? reader.ReadUInt8("cpu/cores/efficiency") ?? 0;
            info.CpuUsage = (double?)(reader.ReadFloat32("cpu/usage")) ?? reader.ReadFloat64("cpu/usage") ?? 0;
            info.PerformanceCoreFreq = (double?)(reader.ReadFloat32("cpu/freq/pCore")) ?? reader.ReadFloat64("cpu/freq/pCore") ?? 0;
            info.EfficiencyCoreFreq = (double?)(reader.ReadFloat32("cpu/freq/eCore")) ?? reader.ReadFloat64("cpu/freq/eCore") ?? 0;
            info.HyperThreading = reader.ReadBool("cpu/hyperThreading") ?? false;
            info.Virtualization = reader.ReadBool("cpu/virtualization") ?? false;
            info.CpuTemperature = (double?)(reader.ReadFloat32("cpu/temperature")) ?? reader.ReadFloat64("cpu/temperature") ?? 0;
            info.CpuUsageSampleIntervalMs = reader.ReadFloat32("cpu/sampleIntervalMs") ?? 500;

            // Memory
            info.TotalMemory = reader.ReadUInt64("memory/total") ?? 0;
            info.UsedMemory = reader.ReadUInt64("memory/used") ?? 0;
            info.AvailableMemory = reader.ReadUInt64("memory/available") ?? 0;
            info.CompressedMemory = reader.ReadUInt64("memory/compressed") ?? 0;

            // Battery
            info.BatteryPercent = reader.ReadInt32("battery/percent") ?? -1;
            info.AcOnline = reader.ReadBool("battery/acOnline") ?? false;

            // OS
            info.OsVersion = reader.ReadString("os/version") ?? "";

            // GPU
            info.GpuName = ipc.ReadWString("gpu/0/name") ?? reader.ReadString("gpu/0/name") ?? "";
            info.GpuBrand = ipc.ReadWString("gpu/0/brand") ?? reader.ReadString("gpu/0/brand") ?? "";
            info.GpuMemory = reader.ReadUInt64("gpu/0/memory") ?? 0;
            info.GpuCoreFreq = (double?)(reader.ReadFloat32("gpu/0/memoryPercent")) ?? reader.ReadFloat64("gpu/0/memoryPercent") ?? 0;
            var gpuUsage = (double?)(reader.ReadFloat32("gpu/0/usage")) ?? reader.ReadFloat64("gpu/0/usage") ?? 0;
            info.GpuTemperature = (double?)(reader.ReadFloat32("gpu/0/temperature")) ?? reader.ReadFloat64("gpu/0/temperature") ?? 0;
            info.GpuIsVirtual = reader.ReadBool("gpu/0/isVirtual") ?? false;

            if (!string.IsNullOrEmpty(info.GpuName))
            {
                info.Gpus = new List<GpuData>
                {
                    new GpuData
                    {
                        Name = info.GpuName,
                        Brand = info.GpuBrand,
                        Memory = info.GpuMemory,
                        Usage = gpuUsage,
                        CoreClock = info.GpuCoreFreq,
                        IsVirtual = info.GpuIsVirtual,
                        Temperature = info.GpuTemperature
                    }
                };
            }

            // Network
            if (reader.HasField("net/0/name"))
            {
                int idx = 0;
                while (reader.HasField($"net/{idx}/name") && idx < 4)
                {
                    var name = ipc.ReadWString($"net/{idx}/name") ?? reader.ReadString($"net/{idx}/name") ?? "";
                    var ip = reader.ReadString($"net/{idx}/ip") ?? "";
                    var mac = reader.ReadString($"net/{idx}/mac") ?? "";
                    var type = reader.ReadString($"net/{idx}/type") ?? "";
                    var speed = reader.ReadUInt64($"net/{idx}/speed") ?? 0;
                    var dl = reader.ReadUInt64($"net/{idx}/downloadSpeed") ?? 0;
                    var ul = reader.ReadUInt64($"net/{idx}/uploadSpeed") ?? 0;

                    if (!string.IsNullOrEmpty(ip) || !string.IsNullOrEmpty(mac))
                    {
                        info.Adapters.Add(new NetworkAdapterData
                        {
                            Name = name,
                            IpAddress = ip,
                            Mac = mac,
                            AdapterType = type,
                            Speed = speed,
                            DownloadSpeed = dl,
                            UploadSpeed = ul
                        });
                    }
                    idx++;
                }
            }

            // Legacy flat fields (fallback)
            if (info.Adapters.Count == 0)
            {
                info.NetworkAdapterName = ipc.ReadWString("net/0/name") ?? reader.ReadString("net/0/name") ?? "";
                info.NetworkAdapterMac = ipc.ReadWString("net/0/mac") ?? reader.ReadString("net/0/mac") ?? "";
                info.NetworkAdapterIp = ipc.ReadWString("net/0/ip") ?? reader.ReadString("net/0/ip") ?? "";
                info.NetworkAdapterType = ipc.ReadWString("net/0/type") ?? reader.ReadString("net/0/type") ?? "";
                info.NetworkAdapterSpeed = reader.ReadUInt64("net/0/speed") ?? 0;

                if (!string.IsNullOrEmpty(info.NetworkAdapterName))
                {
                    info.Adapters.Add(new NetworkAdapterData
                    {
                        Name = info.NetworkAdapterName,
                        Mac = info.NetworkAdapterMac,
                        IpAddress = info.NetworkAdapterIp,
                        AdapterType = info.NetworkAdapterType,
                        Speed = info.NetworkAdapterSpeed
                    });
                }
            }

            // Disks
            if (reader.HasField("disk/0/label"))
            {
                int idx = 0;
                while (reader.HasField($"disk/{idx}/label") && idx < 4)
                {
                    var label = reader.ReadString($"disk/{idx}/label") ?? "";
                    var total = reader.ReadUInt64($"disk/{idx}/total") ?? 0;
                    var used = reader.ReadUInt64($"disk/{idx}/used") ?? 0;
                    var free = reader.ReadUInt64($"disk/{idx}/free") ?? 0;
                    var fs = reader.ReadString($"disk/{idx}/fs") ?? "";

                    if (total > 0)
                    {
                        info.Disks.Add(new DiskData
                        {
                            Letter = '\0', // no drive letters on macOS
                            Label = label,
                            FileSystem = fs,
                            TotalSize = total,
                            UsedSpace = used,
                            FreeSpace = free,
                            PhysicalDiskIndex = -1
                        });
                    }
                    idx++;
                }
            }

            // Temperatures
            if (reader.HasField("sensor/0/name"))
            {
                int idx = 0;
                while (reader.HasField($"sensor/{idx}/name") && idx < 10)
                {
                    var name = reader.ReadString($"sensor/{idx}/name") ?? "";
                    var temp = (double?)(reader.ReadFloat32($"sensor/{idx}/value")) ?? reader.ReadFloat64($"sensor/{idx}/value") ?? 0;
                    info.Temperatures.Add(new TemperatureData
                    {
                        SensorName = name,
                        Temperature = temp
                    });
                    idx++;
                }
            }

            return info;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "IPC: Failed to read SystemInfo from shared memory");
            return null!;
        }
    }
}
