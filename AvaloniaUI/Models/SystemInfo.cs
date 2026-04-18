using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace AvaloniaUI.Models
{
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
        
        public ulong TotalMemory { get; set; }
        public ulong UsedMemory { get; set; }
        public ulong AvailableMemory { get; set; }
        
        public List<GpuData> Gpus { get; set; } = new();
        public string GpuName { get; set; } = string.Empty;
        public string GpuBrand { get; set; } = string.Empty;
        public ulong GpuMemory { get; set; }
        public double GpuCoreFreq { get; set; }
        public bool GpuIsVirtual { get; set; }
        
        public List<NetworkAdapterData> Adapters { get; set; } = new();
        public string NetworkAdapterName { get; set; } = string.Empty;
        public string NetworkAdapterMac { get; set; } = string.Empty;
        public string NetworkAdapterIp { get; set; } = string.Empty;
        public string NetworkAdapterType { get; set; } = string.Empty;
        public ulong NetworkAdapterSpeed { get; set; }
        
        public List<DiskData> Disks { get; set; } = new();
        public List<PhysicalDiskSmartData> PhysicalDisks { get; set; } = new();
        
        public List<TemperatureData> Temperatures { get; set; } = new();
        public double CpuTemperature { get; set; }
        public double GpuTemperature { get; set; }
        public double CpuUsageSampleIntervalMs { get; set; }
        public DateTime LastUpdate { get; set; }
    }

    public abstract class NotifyBase : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler? PropertyChanged;
        protected bool SetProperty<T>(ref T field, T value, [CallerMemberName] string? name = null)
        {
            if (EqualityComparer<T>.Default.Equals(field, value)) return false;
            field = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
            return true;
        }
    }

    public class GpuData : NotifyBase
    {
        private string _name = string.Empty;
        private string _brand = string.Empty;
        private ulong _memory;
        private double _coreClock;
        private bool _isVirtual;
        private double _temperature;
        private double _usage;

        public string Name { get => _name; set => SetProperty(ref _name, value); }
        public string Brand { get => _brand; set => SetProperty(ref _brand, value); }
        public ulong Memory { get => _memory; set => SetProperty(ref _memory, value); }
        public double CoreClock { get => _coreClock; set => SetProperty(ref _coreClock, value); }
        public bool IsVirtual { get => _isVirtual; set => SetProperty(ref _isVirtual, value); }
        public double Temperature { get => _temperature; set => SetProperty(ref _temperature, value); }
        public double Usage { get => _usage; set => SetProperty(ref _usage, value); }
    }

    public class NetworkAdapterData : NotifyBase
    {
        private string _name = string.Empty;
        private string _mac = string.Empty;
        private string _ipAddress = string.Empty;
        private string _adapterType = string.Empty;
        private ulong _speed;

        public string Name { get => _name; set => SetProperty(ref _name, value); }
        public string Mac { get => _mac; set => SetProperty(ref _mac, value); }
        public string IpAddress { get => _ipAddress; set => SetProperty(ref _ipAddress, value); }
        public string AdapterType { get => _adapterType; set => SetProperty(ref _adapterType, value); }
        public ulong Speed { get => _speed; set => SetProperty(ref _speed, value); }
    }

    public class DiskData : NotifyBase
    {
        private char _letter;
        private string _label = string.Empty;
        private string _fileSystem = string.Empty;
        private ulong _totalSize;
        private ulong _usedSpace;
        private ulong _freeSpace;
        private int _physicalDiskIndex = -1;

        public char Letter { get => _letter; set => SetProperty(ref _letter, value); }
        public string Label { get => _label; set => SetProperty(ref _label, value); }
        public string FileSystem { get => _fileSystem; set => SetProperty(ref _fileSystem, value); }
        public ulong TotalSize { get => _totalSize; set => SetProperty(ref _totalSize, value); }
        public ulong UsedSpace { get => _usedSpace; set => SetProperty(ref _usedSpace, value); }
        public ulong FreeSpace { get => _freeSpace; set => SetProperty(ref _freeSpace, value); }
        public int PhysicalDiskIndex { get => _physicalDiskIndex; set => SetProperty(ref _physicalDiskIndex, value); }

        public double UsagePercent => TotalSize > 0 ? (double)UsedSpace / TotalSize * 100 : 0;
    }

    public class SmartAttributeData
    {
        public byte Id { get; set; }
        public byte Current { get; set; }
        public byte Worst { get; set; }
        public byte Threshold { get; set; }
        public ulong RawValue { get; set; }
        public string Name { get; set; } = string.Empty;
        public string Description { get; set; } = string.Empty;
        public bool IsCritical { get; set; }
        public double PhysicalValue { get; set; }
        public string Units { get; set; } = string.Empty;
    }

    public class PhysicalDiskSmartData
    {
        public string Model { get; set; } = string.Empty;
        public string SerialNumber { get; set; } = string.Empty;
        public string FirmwareVersion { get; set; } = string.Empty;
        public string InterfaceType { get; set; } = string.Empty;
        public string DiskType { get; set; } = string.Empty;
        public ulong Capacity { get; set; }
        public double Temperature { get; set; }
        public byte HealthPercentage { get; set; }
        public bool IsSystemDisk { get; set; }
        public bool SmartEnabled { get; set; }
        public bool SmartSupported { get; set; }
        public ulong PowerOnHours { get; set; }
        public ulong PowerCycleCount { get; set; }
        public ulong ReallocatedSectorCount { get; set; }
        public ulong CurrentPendingSector { get; set; }
        public ulong UncorrectableErrors { get; set; }
        public double WearLeveling { get; set; }
        public ulong TotalBytesWritten { get; set; }
        public ulong TotalBytesRead { get; set; }
        public List<char> LogicalDriveLetters { get; set; } = new();
        public List<SmartAttributeData> Attributes { get; set; } = new();
    }

    public class TemperatureData
    {
        public string SensorName { get; set; } = string.Empty;
        public double Temperature { get; set; }
    }

    public class PhysicalDiskView : NotifyBase
    {
        private PhysicalDiskSmartData? _disk;
        public PhysicalDiskSmartData? Disk { get => _disk!; set => SetProperty(ref _disk, value); }
        public ObservableCollection<DiskData> Partitions { get; } = new();
        public string LettersDisplay => Partitions.Count == 0 ? "无分区" : string.Join(", ", Partitions.Select(p => p.Letter + ":"));
        public string DisplayName => Disk == null ? "未知磁盘" : $"{Disk.Model} ({LettersDisplay})";
    }
}