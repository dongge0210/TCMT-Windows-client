using System.Runtime.InteropServices;
using AvaloniaUI.Models;
using Serilog;

namespace AvaloniaUI.Services.IPC;

public static class IPCSystemInfoMapper
{
    private static bool IsMacOS => RuntimeInformation.IsOSPlatform(OSPlatform.OSX);

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
            info.CpuName = ipc.ReadWString("cpu/name") ?? ipc.ReadString("cpu/name") ?? "";
            info.PhysicalCores = reader.ReadInt32("cpu/cores/physical") ?? reader.ReadUInt8("cpu/cores/physical") ?? 0;
            info.LogicalCores = reader.ReadInt32("cpu/cores/logical") ?? 0;
            info.PerformanceCores = reader.ReadInt32("cpu/cores/performance") ?? reader.ReadUInt8("cpu/cores/performance") ?? 0;
            info.EfficiencyCores = reader.ReadInt32("cpu/cores/efficiency") ?? reader.ReadUInt8("cpu/cores/efficiency") ?? 0;
            info.CpuUsage = (double?)(reader.ReadFloat32("cpu/usage")) ?? reader.ReadFloat64("cpu/usage") ?? 0;
            info.PerformanceCoreFreq = (double?)(reader.ReadFloat32("cpu/freq/pCore")) ?? reader.ReadFloat64("cpu/freq/pCore") ?? 0;
            info.EfficiencyCoreFreq = (double?)(reader.ReadFloat32("cpu/freq/eCore")) ?? reader.ReadFloat64("cpu/freq/eCore") ?? 0;
            info.HyperThreading = reader.ReadBool("cpu/hyperThreading") ?? false;
            info.Virtualization = reader.ReadBool("cpu/virtualization") ?? false;
            info.CpuTemperature = (double?)(reader.ReadFloat32("cpu/temperature")) ?? reader.ReadFloat64("cpu/temperature") ?? 0;

            // Memory
            info.TotalMemory = reader.ReadUInt64("memory/total") ?? 0;
            info.UsedMemory = reader.ReadUInt64("memory/used") ?? 0;
            info.AvailableMemory = reader.ReadUInt64("memory/available") ?? 0;
            info.CompressedMemory = reader.ReadUInt64("memory/compressed") ?? 0;

            // GPU
            info.GpuName = ipc.ReadWString("gpu/0/name") ?? ipc.ReadString("gpu/0/name") ?? "";
            info.GpuBrand = ipc.ReadWString("gpu/0/brand") ?? "";
            info.GpuMemory = reader.ReadUInt64("gpu/0/memory") ?? 0;
            var gpuUsage = (double?)(reader.ReadFloat32("gpu/0/usage")) ?? reader.ReadFloat64("gpu/0/usage") ?? 0.0;
            info.GpuCoreFreq = (double?)(reader.ReadFloat32("gpu/0/coreFreq")) ?? reader.ReadFloat64("gpu/0/coreFreq") ?? 0;
            info.GpuIsVirtual = reader.ReadBool("gpu/0/isVirtual") ?? false;
            info.GpuTemperature = (double?)(reader.ReadFloat32("gpu/0/temperature")) ?? reader.ReadFloat64("gpu/0/temperature") ?? 0;

            // On Apple Silicon, CPU/GPU share the same die temperature.
            // Use CPU temp as SoC temp and hide GPU temp.
            if (IsMacOS && info.GpuTemperature > 0 && info.CpuTemperature <= 0)
                info.CpuTemperature = info.GpuTemperature;
            if (IsMacOS)
                info.GpuTemperature = 0;

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
            info.NetworkAdapterName = ipc.ReadWString("net/0/name") ?? ipc.ReadString("net/0/name") ?? "";
            info.NetworkAdapterMac = ipc.ReadWString("net/0/mac") ?? ipc.ReadString("net/0/mac") ?? "";
            info.NetworkAdapterIp = ipc.ReadWString("net/0/ip") ?? ipc.ReadString("net/0/ip") ?? "";
            info.NetworkAdapterType = ipc.ReadWString("net/0/type") ?? ipc.ReadString("net/0/type") ?? "";
            info.NetworkAdapterSpeed = reader.ReadUInt64("net/0/speed") ?? 0;

            if (!string.IsNullOrEmpty(info.NetworkAdapterName))
            {
                info.Adapters = new List<NetworkAdapterData>
                {
                    new NetworkAdapterData
                    {
                        Name = info.NetworkAdapterName,
                        Mac = info.NetworkAdapterMac,
                        IpAddress = info.NetworkAdapterIp,
                        AdapterType = info.NetworkAdapterType,
                        Speed = info.NetworkAdapterSpeed
                    }
                };
            }

            // Disks
            int diskIdx = 0;
            while (reader.HasField($"disk/{diskIdx}/label"))
            {
                var label = reader.ReadString($"disk/{diskIdx}/label") ?? reader.ReadWString($"disk/{diskIdx}/label") ?? "";
                if (!string.IsNullOrEmpty(label))
                {
                    var total = reader.ReadUInt64($"disk/{diskIdx}/total") ?? 0;
                    var used = reader.ReadUInt64($"disk/{diskIdx}/used") ?? 0;
                    var free = reader.ReadUInt64($"disk/{diskIdx}/free") ?? 0;
                    var fs = reader.ReadString($"disk/{diskIdx}/fs") ?? "";
                    info.Disks.Add(new DiskData
                    {
                        Label = label,
                        FileSystem = fs,
                        TotalSize = total,
                        UsedSpace = used,
                        FreeSpace = free
                    });
                }
                diskIdx++;
            }

            info.CpuUsageSampleIntervalMs = reader.ReadFloat64("cpu/sampleIntervalMs") ?? 500;
            return info;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "IPC: Failed to read SystemInfo from shared memory");
            return null!;
        }
    }
}
