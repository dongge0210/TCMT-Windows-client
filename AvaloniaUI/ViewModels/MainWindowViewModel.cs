using System;
using System.Collections.ObjectModel;
using System.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Serilog;
using AvaloniaUI.Models;
using AvaloniaUI.Services;

namespace AvaloniaUI.ViewModels;

public partial class MainWindowViewModel : ObservableObject, IDisposable
{
    private readonly SharedMemoryService _sharedMemory;
    private readonly DispatcherTimer _timer;
    private bool _disposed = false;
    private const int MaxChartPoints = 60;
    private int _consecutiveErrors = 0;
    private const int MaxConsecutiveErrors = 5;

    // Track previous selections
    private string? _previousNetworkKey;
    private string? _previousDiskKey;

    public MainWindowViewModel()
    {
        _sharedMemory = new SharedMemoryService();
        _timer = new DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(500)
        };
        _timer.Tick += UpdateTimer_Tick;
    }

    public void Start()
    {
        try
        {
            if (_sharedMemory.Initialize())
            {
                IsConnected = true;
                ConnectionStatus = "已连接";
                WindowTitle = "系统硬件监视器";
                _timer.Start();
                Log.Information("Started monitoring");
            }
            else
            {
                IsConnected = false;
                ConnectionStatus = $"连接失败: {_sharedMemory.LastError}";
                WindowTitle = "系统硬件监视器 - 未连接";
                ShowDisconnectedState();
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to start monitoring");
            IsConnected = false;
            ConnectionStatus = $"错误: {ex.Message}";
        }
    }

    private void ShowDisconnectedState()
    {
        CpuName = "未连接";
        TotalMemory = "未检测到";
        UsedMemory = "未检测到";
        GpuList.Clear();
        NetworkList.Clear();
        DiskList.Clear();
        PhysicalDiskList.Clear();
        SelectedGpu = null;
        SelectedNetwork = null;
        SelectedDisk = null;
    }

    [RelayCommand]
    private void Reconnect()
    {
        _consecutiveErrors = 0;
        try
        {
            _sharedMemory.Dispose();
            if (_sharedMemory.Initialize())
            {
                IsConnected = true;
                ConnectionStatus = "已连接";
                WindowTitle = "系统硬件监视器";
                Log.Information("Reconnected to shared memory");
            }
            else
            {
                IsConnected = false;
                ConnectionStatus = $"重连失败: {_sharedMemory.LastError}";
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Reconnect failed");
            IsConnected = false;
            ConnectionStatus = "重连失败";
        }
    }

    [RelayCommand]
    private void ShowWindow()
    {
        if (Application.Current?.ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            desktop.MainWindow?.Show();
            desktop.MainWindow?.Activate();
        }
    }

    [RelayCommand]
    private void Quit()
    {
        Dispose();
        if (Application.Current?.ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            desktop.Shutdown();
        }
    }

    private void UpdateTimer_Tick(object? sender, EventArgs e)
    {
        try
        {
            var info = _sharedMemory.ReadSystemInfo();
            if (info != null)
            {
                _consecutiveErrors = 0;
                if (!IsConnected)
                {
                    IsConnected = true;
                    ConnectionStatus = "已连接";
                    WindowTitle = "系统硬件监视器";
                }
                UpdateSystemData(info);
                LastUpdate = DateTime.Now;
            }
            else
            {
                _consecutiveErrors++;
                if (_consecutiveErrors >= MaxConsecutiveErrors && IsConnected)
                {
                    IsConnected = false;
                    ConnectionStatus = "连接已断开";
                    WindowTitle = "系统硬件监视器 - 已断开";
                    ShowDisconnectedState();
                }
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error updating data");
            _consecutiveErrors++;
            if (_consecutiveErrors >= MaxConsecutiveErrors)
            {
                IsConnected = false;
                ConnectionStatus = $"错误: {ex.Message}";
            }
        }
    }

    private void UpdateSystemData(SystemInfo info)
    {
        // CPU
        CpuName = string.IsNullOrWhiteSpace(info.CpuName) ? "未知CPU" : info.CpuName;
        PhysicalCores = info.PhysicalCores;
        LogicalCores = info.LogicalCores;
        PerformanceCores = info.PerformanceCores;
        EfficiencyCores = info.EfficiencyCores;
        CpuUsage = ValidateDouble(info.CpuUsage);
        HyperThreading = info.HyperThreading;
        Virtualization = info.Virtualization;
        CpuTemperature = ValidateDouble(info.CpuTemperature);
        CpuFrequency = FormatFrequency(info.PerformanceCoreFreq);
        CpuEfficiencyFrequency = FormatFrequency(info.EfficiencyCoreFreq);

        // Memory
        TotalMemory = info.TotalMemory > 0 ? FormatBytes(info.TotalMemory) : "未检测到";
        UsedMemory = info.UsedMemory > 0 ? FormatBytes(info.UsedMemory) : "未检测到";
        AvailableMemory = info.AvailableMemory > 0 ? FormatBytes(info.AvailableMemory) : "未检测到";
        MemoryPercent = info.TotalMemory > 0 ? (double)info.UsedMemory / info.TotalMemory * 100 : 0;
        AddMemoryHistoryPoint(MemoryPercent);

        // GPU
        UpdateCollection(GpuList, info.Gpus);
        if (info.Gpus.Count > 0)
        {
            if (SelectedGpu == null)
                SelectedGpu = info.Gpus[0];
            else
            {
                var restored = info.Gpus.FirstOrDefault(g => g.Name == SelectedGpu.Name);
                if (restored != null) SelectedGpu = restored;
            }
        }
        // Ensure selected is never null
        if (SelectedGpu == null) SelectedGpu = GpuList.FirstOrDefault();
        GpuTemperature = ValidateDouble(info.GpuTemperature);

        // Network - preserve selection
        if (SelectedNetwork != null)
            _previousNetworkKey = $"{SelectedNetwork.Name}|{SelectedNetwork.Mac}";
        UpdateCollection(NetworkList, info.Adapters);
        if (_previousNetworkKey != null)
        {
            var restored = NetworkList.FirstOrDefault(a => $"{a.Name}|{a.Mac}" == _previousNetworkKey);
            if (restored != null) SelectedNetwork = restored;
        }
        if (SelectedNetwork == null && NetworkList.Count > 0)
            SelectedNetwork = NetworkList[0];
        if (SelectedNetwork == null) SelectedNetwork = NetworkList.FirstOrDefault();

        // Physical Disks with SMART
        BuildOrUpdatePhysicalDisks(info);

        // Disk - preserve selection
        if (SelectedDisk != null)
            _previousDiskKey = $"{SelectedDisk.Letter}:{SelectedDisk.Label}";
        UpdateCollection(DiskList, info.Disks);
        if (_previousDiskKey != null)
        {
            var restored = DiskList.FirstOrDefault(d => $"{d.Letter}:{d.Label}" == _previousDiskKey);
            if (restored != null) SelectedDisk = restored;
        }
        if (SelectedDisk == null && DiskList.Count > 0)
            SelectedDisk = DiskList[0];
        if (SelectedDisk == null) SelectedDisk = DiskList.FirstOrDefault();

        // Temperature charts
        UpdateTemperatureCharts(info.CpuTemperature, info.GpuTemperature);
    }

    private void BuildOrUpdatePhysicalDisks(SystemInfo info)
    {
        try
        {
            var map = PhysicalDiskList.ToDictionary(p => p.Disk?.SerialNumber ?? "", p => p);
            var alive = new HashSet<string>();

            foreach (var pd in info.PhysicalDisks)
            {
                if (!map.TryGetValue(pd.SerialNumber, out var view))
                {
                    view = new PhysicalDiskView { Disk = pd };
                    PhysicalDiskList.Add(view);
                }
                else
                {
                    view.Disk = pd;
                }
                alive.Add(pd.SerialNumber);
            }

            for (int i = PhysicalDiskList.Count - 1; i >= 0; i--)
            {
                if (!alive.Contains(PhysicalDiskList[i].Disk?.SerialNumber ?? ""))
                    PhysicalDiskList.RemoveAt(i);
            }
            // Ensure selected is never null
            if (SelectedPhysicalDisk == null) SelectedPhysicalDisk = PhysicalDiskList.FirstOrDefault();
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to build physical disks");
        }
    }

    private void UpdateCollection<T>(ObservableCollection<T> collection, List<T> newItems)
    {
        try
        {
            if (newItems == null)
            {
                if (collection.Count > 0) collection.Clear();
                return;
            }

            collection.Clear();
            foreach (var item in newItems)
            {
                collection.Add(item);
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to update collection");
        }
    }

    private void UpdateTemperatureCharts(double cpuTemp, double gpuTemp)
    {
        try
        {
            if (cpuTemp > 0 && cpuTemp < 150)
            {
                CpuTempData.Add(cpuTemp);
            }
            else if (CpuTempData.Count == 0)
            {
                CpuTempData.Add(0);
            }

            if (gpuTemp > 0 && gpuTemp < 150)
            {
                GpuTempData.Add(gpuTemp);
            }
            else if (GpuTempData.Count == 0)
            {
                GpuTempData.Add(0);
            }

            while (CpuTempData.Count > MaxChartPoints)
                CpuTempData.RemoveAt(0);
            while (GpuTempData.Count > MaxChartPoints)
                GpuTempData.RemoveAt(0);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to update temperature charts");
        }
    }

    private double ValidateDouble(double value)
    {
        if (double.IsNaN(value) || double.IsInfinity(value) || value < 0) return 0;
        return value;
    }

    private string FormatBytes(ulong bytes)
    {
        if (bytes == 0) return "0 B";
        const ulong KB = 1024UL;
        const ulong MB = KB * KB;
        const ulong GB = MB * KB;
        const ulong TB = GB * KB;

        return bytes switch
        {
            >= TB => $"{(double)bytes / TB:F1} TB",
            >= GB => $"{(double)bytes / GB:F1} GB",
            >= MB => $"{(double)bytes / MB:F1} MB",
            >= KB => $"{(double)bytes / KB:F1} KB",
            _ => $"{bytes} B"
        };
    }

    private string FormatFrequency(double frequency)
    {
        if (frequency <= 0) return "N/A";
        return frequency >= 1000 ? $"{frequency / 1000.0:F1} GHz" : $"{frequency:F0} MHz";
    }

    private string FormatNetworkSpeed(ulong speedBps)
    {
        if (speedBps == 0) return "N/A";
        const ulong Kbps = 1000UL;
        const ulong Mbps = Kbps * Kbps;
        const ulong Gbps = Mbps * Kbps;

        return speedBps switch
        {
            >= Gbps => $"{(double)speedBps / Gbps:F1} Gbps",
            >= Mbps => $"{(double)speedBps / Mbps:F1} Mbps",
            >= Kbps => $"{(double)speedBps / Kbps:F1} Kbps",
            _ => $"{speedBps} bps"
        };
    }

    public void AddMemoryHistoryPoint(double value)
    {
        MemoryHistory.Add(value);
        while (MemoryHistory.Count > MaxChartPoints)
        {
            MemoryHistory.RemoveAt(0);
        }
        OnPropertyChanged(nameof(MemoryHistory));
    }

    #region Properties

    [ObservableProperty]
    private string _windowTitle = "系统硬件监视器";

    [ObservableProperty]
    private bool _isConnected;

    [ObservableProperty]
    private string _connectionStatus = "连接中...";

    // CPU
    [ObservableProperty]
    private string _cpuName = "检测中...";

    [ObservableProperty]
    private int _physicalCores;

    [ObservableProperty]
    private int _logicalCores;

    [ObservableProperty]
    private int _performanceCores;

    [ObservableProperty]
    private int _efficiencyCores;

    [ObservableProperty]
    private double _cpuUsage;

    [ObservableProperty]
    private bool _hyperThreading;

    [ObservableProperty]
    private bool _virtualization;

    [ObservableProperty]
    private double _cpuTemperature;

    [ObservableProperty]
    private string _cpuFrequency = "N/A";

    [ObservableProperty]
    private string _cpuEfficiencyFrequency = "N/A";

    // Memory
    [ObservableProperty]
    private string _totalMemory = "检测中...";

    [ObservableProperty]
    private string _usedMemory = "检测中...";

    [ObservableProperty]
    private string _availableMemory = "检测中...";

    [ObservableProperty]
    private double _memoryUsed;

    [ObservableProperty]
    private double _memoryTotal;

    [ObservableProperty]
    private double _memoryPercent;

    // GPU
    [ObservableProperty]
    private ObservableCollection<GpuData> _gpuList = new();

    [ObservableProperty]
    private GpuData? _selectedGpu;

    [ObservableProperty]
    private double _gpuTemperature;

    // Network
    [ObservableProperty]
    private ObservableCollection<NetworkAdapterData> _networkList = new();

    [ObservableProperty]
    private NetworkAdapterData? _selectedNetwork;

    [ObservableProperty]
    private string _networkDownloadSpeed = "N/A";

    [ObservableProperty]
    private string _networkUploadSpeed = "N/A";

    // Disk
    [ObservableProperty]
    private ObservableCollection<DiskData> _diskList = new();

    [ObservableProperty]
    private DiskData? _selectedDisk;

    // Physical Disks with SMART
    [ObservableProperty]
    private ObservableCollection<PhysicalDiskView> _physicalDiskList = new();

    [ObservableProperty]
    private PhysicalDiskView? _selectedPhysicalDisk;

    // Last update
    [ObservableProperty]
    private DateTime _lastUpdate = DateTime.Now;

    // Chart data
    [ObservableProperty]
    private ObservableCollection<double> _memoryHistory = new();

    #endregion

    #region Temperature Chart Data
    public ObservableCollection<double> CpuTempData { get; } = new();
    public ObservableCollection<double> GpuTempData { get; } = new();
    #endregion

    public void Dispose()
    {
        if (_disposed) return;
        _timer.Stop();
        _sharedMemory.Dispose();
        _disposed = true;
        GC.SuppressFinalize(this);
    }

    ~MainWindowViewModel() => Dispose();
}