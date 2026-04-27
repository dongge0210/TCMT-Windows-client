using AvaloniaUI.Models;
using Serilog;

namespace AvaloniaUI.Services.IPC;

/// <summary>
/// Schema 字段名 → SystemInfo 映射器。
/// 根据 C++ 端下发的 Schema 字段定义，动态读取共享内存并填充 SystemInfo。
/// 字段名与 C++ 端保持一致。
/// </summary>
public static class IPCSystemInfoMapper
{
    /// <summary>
    /// 从 IPCService 的 Reader 读取数据，映射为 SystemInfo
    /// </summary>
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

            // CPU 基本信息
            info.CpuName = reader.ReadString("cpuName") ?? "";
            info.PhysicalCores = reader.ReadInt32("physicalCores") ?? 0;
            info.LogicalCores = reader.ReadInt32("logicalCores") ?? 0;
            info.PerformanceCores = reader.ReadInt32("performanceCores") ?? 0;
            info.EfficiencyCores = reader.ReadInt32("efficiencyCores") ?? 0;
            info.CpuUsage = reader.ReadFloat64("cpuUsage") ?? 0;
            info.PerformanceCoreFreq = reader.ReadFloat64("performanceCoreFreq") ?? 0;
            info.EfficiencyCoreFreq = reader.ReadFloat64("efficiencyCoreFreq") ?? 0;
            info.HyperThreading = reader.ReadBool("hyperThreading") ?? false;
            info.Virtualization = reader.ReadBool("virtualization") ?? false;
            info.CpuTemperature = reader.ReadFloat64("cpuTemperature") ?? 0;

            // 内存
            info.TotalMemory = reader.ReadUInt64("totalMemory") ?? 0;
            info.UsedMemory = reader.ReadUInt64("usedMemory") ?? 0;
            info.AvailableMemory = reader.ReadUInt64("availableMemory") ?? 0;

            // GPU（简化：单卡场景，多卡后续扩展）
            info.GpuName = reader.ReadString("gpuName") ?? "";
            info.GpuBrand = reader.ReadString("gpuBrand") ?? "";
            info.GpuMemory = reader.ReadUInt64("gpuMemory") ?? 0;
            info.GpuCoreFreq = reader.ReadFloat64("gpuCoreFreq") ?? 0;
            info.GpuIsVirtual = reader.ReadBool("gpuIsVirtual") ?? false;
            info.GpuTemperature = reader.ReadFloat64("gpuTemperature") ?? 0;

            // 将单卡数据转为 GpuData 列表（兼容 ViewModel）
            if (!string.IsNullOrEmpty(info.GpuName))
            {
                info.Gpus = new List<GpuData>
                {
                    new GpuData
                    {
                        Name = info.GpuName,
                        Brand = info.GpuBrand,
                        Memory = info.GpuMemory,
                        CoreClock = info.GpuCoreFreq,
                        IsVirtual = info.GpuIsVirtual,
                        Temperature = info.GpuTemperature
                    }
                };
            }

            // 网络（简化：单适配器）
            info.NetworkAdapterName = reader.ReadString("networkAdapterName") ?? "";
            info.NetworkAdapterMac = reader.ReadString("networkAdapterMac") ?? "";
            info.NetworkAdapterIp = reader.ReadString("networkAdapterIp") ?? "";
            info.NetworkAdapterType = reader.ReadString("networkAdapterType") ?? "";
            info.NetworkAdapterSpeed = reader.ReadUInt64("networkAdapterSpeed") ?? 0;

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

            // 温度
            if (reader.HasField("sensorName0"))
            {
                int idx = 0;
                while (reader.HasField($"sensorName{idx}"))
                {
                    var name = reader.ReadString($"sensorName{idx}") ?? "";
                    var temp = reader.ReadFloat64($"sensorTemp{idx}") ?? 0;
                    info.Temperatures.Add(new TemperatureData
                    {
                        SensorName = name,
                        Temperature = temp
                    });
                    idx++;
                }
            }

            // 采样间隔
            info.CpuUsageSampleIntervalMs = reader.ReadFloat64("sampleIntervalMs") ?? 500;

            return info;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "IPC: Failed to read SystemInfo from shared memory");
            return null!;
        }
    }
}
