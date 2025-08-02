using System.Collections.ObjectModel;

namespace WPF_UI1.Models
{
    // 对应C++中的SystemInfo结构
    public class SystemInfo
    {
        public string CpuName { get; set; } = string.Empty;
        public int PhysicalCores { get; set; }
        public int LogicalCores { get; set; }
        public double CpuUsage { get; set; }
        public int PerformanceCores { get; set; }
        public int EfficiencyCores { get; set; }
        public double PerformanceCoreFreq { get; set; }
        public double EfficiencyCoreFreq { get; set; }
        public bool HyperThreading { get; set; }
        public bool Virtualization { get; set; }

        // 内存信息
        public ulong TotalMemory { get; set; }
        public ulong UsedMemory { get; set; }
        public ulong AvailableMemory { get; set; }

        // GPU信息
        public List<GpuData> Gpus { get; set; } = new();
        public string GpuName { get; set; } = string.Empty;
        public string GpuBrand { get; set; } = string.Empty;
        public ulong GpuMemory { get; set; }
        public double GpuCoreFreq { get; set; }
        public bool GpuIsVirtual { get; set; }

        // 网络信息
        public List<NetworkAdapterData> Adapters { get; set; } = new();
        public string NetworkAdapterName { get; set; } = string.Empty;
        public string NetworkAdapterMac { get; set; } = string.Empty;
        public string NetworkAdapterIp { get; set; } = string.Empty;
        public string NetworkAdapterType { get; set; } = string.Empty;
        public ulong NetworkAdapterSpeed { get; set; }

        // 磁盘信息
        public List<DiskData> Disks { get; set; } = new();

        // 温度信息
        public List<TemperatureData> Temperatures { get; set; } = new();
        public double CpuTemperature { get; set; }
        public double GpuTemperature { get; set; }

        public DateTime LastUpdate { get; set; }
    }

    public class GpuData
    {
        public string Name { get; set; } = string.Empty;
        public string Brand { get; set; } = string.Empty;
        public ulong Memory { get; set; }
        public double CoreClock { get; set; }
        public bool IsVirtual { get; set; }
    }

    public class NetworkAdapterData
    {
        public string Name { get; set; } = string.Empty;
        public string Mac { get; set; } = string.Empty;
        public string IpAddress { get; set; } = string.Empty;
        public string AdapterType { get; set; } = string.Empty;
        public ulong Speed { get; set; }
    }

    public class DiskData
    {
        public char Letter { get; set; }
        public string Label { get; set; } = string.Empty;
        public string FileSystem { get; set; } = string.Empty;
        public ulong TotalSize { get; set; }
        public ulong UsedSpace { get; set; }
        public ulong FreeSpace { get; set; }
    }

    public class TemperatureData
    {
        public string SensorName { get; set; } = string.Empty;
        public double Temperature { get; set; }
    }
}