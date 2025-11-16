using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using LiveChartsCore;
using LiveChartsCore.SkiaSharpView;
using LiveChartsCore.SkiaSharpView.Painting;
using SkiaSharp;
using Serilog;
using WPF_UI1.Models;
using WPF_UI1.Services;

namespace WPF_UI1.ViewModels
{
 public partial class MainWindowViewModel : ObservableObject
 {
 private readonly SharedMemoryService _sharedMemoryService;
 private readonly DiagnosticsPipeClient _diagClient = new();
 private readonly DispatcherTimer _updateTimer;
 private const int MAX_CHART_POINTS =60;
 private int _consecutiveErrors =0;
 private const int MAX_CONSECUTIVE_ERRORS =5;

 private readonly Dictionary<string, char> _lastPartitionByPhysical = new();

 [ObservableProperty] private bool isConnected;
 [ObservableProperty] private string connectionStatus = "Connecting...";
 [ObservableProperty] private string windowTitle = "System Hardware Monitor";
 [ObservableProperty] private string lastUpdateTime = "Never updated";

 #region CPU
 [ObservableProperty] private string cpuName = "Detecting...";
 [ObservableProperty] private int physicalCores;
 [ObservableProperty] private int logicalCores;
 [ObservableProperty] private int performanceCores;
 [ObservableProperty] private int efficiencyCores;
 [ObservableProperty] private double cpuUsage;
 [ObservableProperty] private bool hyperThreading;
 [ObservableProperty] private bool virtualization;
 [ObservableProperty] private double cpuTemperature;
 [ObservableProperty] private double cpuBaseFrequencyMHz;
 [ObservableProperty] private double cpuCurrentFrequencyMHz;
 [ObservableProperty] private double cpuUsageSampleIntervalMs;
 #endregion

 #region Memory
 [ObservableProperty] private string totalMemory = "Detecting...";
 [ObservableProperty] private string usedMemory = "Detecting...";
 [ObservableProperty] private string availableMemory = "Detecting...";
 [ObservableProperty] private double memoryUsagePercent;
 #endregion

 #region GPU
 [ObservableProperty] private ObservableCollection<GpuData> gpus = new();
 [ObservableProperty] private GpuData? selectedGpu;
 [ObservableProperty] private double gpuTemperature;
 #endregion

 #region Network
 [ObservableProperty] private ObservableCollection<NetworkAdapterData> networkAdapters = new();
 [ObservableProperty] private NetworkAdapterData? selectedNetworkAdapter;
 #endregion

 #region Disk
 [ObservableProperty] private ObservableCollection<DiskData> disks = new();
 [ObservableProperty] private DiskData? selectedDisk;
 [ObservableProperty] private ObservableCollection<PhysicalDiskView> physicalDisks = new();
 [ObservableProperty] private PhysicalDiskView? selectedPhysicalDisk;
 #endregion

 #region TPM
 [ObservableProperty] private bool hasTpm;
 [ObservableProperty] private string tpmManufacturer = "N/A";
 [ObservableProperty] private string tpmManufacturerId = "N/A";
 [ObservableProperty] private string tpmVersion = "N/A";
 [ObservableProperty] private string tpmFirmwareVersion = "N/A";
 [ObservableProperty] private string tpmStatus = "N/A";
 [ObservableProperty] private bool tpmEnabled;
 [ObservableProperty] private bool tpmIsActivated;
 [ObservableProperty] private bool tpmIsOwned;
 [ObservableProperty] private bool tpmReady;
 [ObservableProperty] private bool tpmTbsAvailable;
 [ObservableProperty] private bool tpmPhysicalPresenceRequired;
 [ObservableProperty] private uint tpmSpecVersion;
 [ObservableProperty] private uint tpmTbsVersion;
 [ObservableProperty] private string tpmErrorMessage = "N/A";
 [ObservableProperty] private string tpmDetectionMethod = "N/A";
 #endregion

 #region Charts
 public ObservableCollection<ISeries> CpuTemperatureSeries { get; } = new();
 public ObservableCollection<ISeries> GpuTemperatureSeries { get; } = new();
 private readonly ObservableCollection<double> _cpuTempData = new();
 private readonly ObservableCollection<double> _gpuTempData = new();
 #endregion

 [ObservableProperty] private string sharedMemoryDiagOffsets = string.Empty;
 [ObservableProperty] private ObservableCollection<string> diagLogs = new();

 public MainWindowViewModel(SharedMemoryService sharedMemoryService)
 {
 _sharedMemoryService = sharedMemoryService;
 _diagClient.SnapshotReceived += OnDiagSnapshot;
 _diagClient.Start();
 InitializeCharts();
 _updateTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(500) };
 _updateTimer.Tick += UpdateTimer_Tick;
 _updateTimer.Start();
 TryConnect();
 }

 private void OnDiagSnapshot(DiagnosticsPipeSnapshot snap)
 {
 try
 {
 string offsets = $"ts={snap.timestamp} seq={snap.writeSequence} abi=0x{snap.abiVersion:X8}\n";
 if (snap.offsets != null)
 {
 offsets += $"tempSensors={snap.offsets.tempSensors} tempCount={snap.offsets.tempSensorCount}\n";
 offsets += $"smartDisks={snap.offsets.smartDisks} smartCount={snap.offsets.smartDiskCount}\n";
 offsets += $"futureReserved={snap.offsets.futureReserved} hash={snap.offsets.sharedmemHash} extPad={snap.offsets.extensionPad}\n";
 }
 SharedMemoryDiagOffsets = offsets;
 if (snap.logs != null && snap.logs.Length >0)
 {
 App.Current.Dispatcher.Invoke(() =>
 {
 foreach (var l in snap.logs)
 {
 diagLogs.Add(l);
 if (diagLogs.Count >200) diagLogs.RemoveAt(0);
 }
 });
 }
 }
 catch (Exception ex)
 {
 Log.Debug("处理诊断快照失败: " + ex.Message);
 }
 }

 partial void OnSelectedDiskChanged(DiskData? value)
 {
 var serial = SelectedPhysicalDisk?.Disk?.SerialNumber;
 if (!string.IsNullOrEmpty(serial) && value != null)
 _lastPartitionByPhysical[serial] = value.Letter;
 }

 partial void OnSelectedPhysicalDiskChanged(PhysicalDiskView? value)
 {
 if (value == null) return;
 var serial = value.Disk?.SerialNumber ?? string.Empty;
 if (!string.IsNullOrEmpty(serial) && _lastPartitionByPhysical.TryGetValue(serial, out var letter))
 {
 var part = value.Partitions.FirstOrDefault(p => char.ToUpperInvariant(p.Letter) == char.ToUpperInvariant(letter));
 if (part != null)
 {
 SelectedDisk = part;
 return;
 }
 }
 if (value.Partitions.Count >0) SelectedDisk = value.Partitions[0];
 }

 private void InitializeCharts()
 {
 CpuTemperatureSeries.Add(new LineSeries<double>
 {
 Values = _cpuTempData,
 Name = "CPU温度",
 Stroke = new SolidColorPaint(SKColors.DeepSkyBlue) { StrokeThickness =2 },
 Fill = null,
 GeometrySize =0
 });
 GpuTemperatureSeries.Add(new LineSeries<double>
 {
 Values = _gpuTempData,
 Name = "GPU温度",
 Stroke = new SolidColorPaint(SKColors.Orange) { StrokeThickness =2 },
 Fill = null,
 GeometrySize =0
 });
 }

 private async void UpdateTimer_Tick(object? sender, EventArgs e) => await UpdateSystemInfoAsync();

 private async Task UpdateSystemInfoAsync()
 {
 try
 {
 var systemInfo = await Task.Run(() => _sharedMemoryService.ReadSystemInfo());
 if (systemInfo != null)
 {
 _consecutiveErrors =0;
 if (!IsConnected)
 {
 IsConnected = true;
 ConnectionStatus = "Connected";
 WindowTitle = "System Hardware Monitor";
 }
 UpdateSystemData(systemInfo);
 LastUpdateTime = DateTime.Now.ToString("HH:mm:ss");
 }
 else
 {
 _consecutiveErrors++;
 if (_consecutiveErrors >= MAX_CONSECUTIVE_ERRORS && IsConnected)
 {
 IsConnected = false;
 ConnectionStatus = "连接已断开 - 尝试重新连接...";
 WindowTitle = "系统硬件监视器 -连接中断";
 ShowDisconnectedState();
 await Task.Run(TryConnect);
 }
 }
 }
 catch (Exception ex)
 {
 Log.Error(ex, "Error occurred while updating system information");
 _consecutiveErrors++;
 if (_consecutiveErrors >= MAX_CONSECUTIVE_ERRORS)
 {
 IsConnected = false;
 ConnectionStatus = $"错误: {ex.Message}";
 ShowErrorState(ex.Message);
 }
 }
 }

 private void TryConnect()
 {
 Log.Information("尝试连接共享内存...");
 if (_sharedMemoryService.Initialize())
 {
 IsConnected = true;
 ConnectionStatus = "已连接";
 WindowTitle = "系统硬件监视器";
 _consecutiveErrors =0;
 Log.Information("Shared memory connection successful");
 }
 else
 {
 IsConnected = false;
 ConnectionStatus = $"连接失败: {_sharedMemoryService.LastError}";
 WindowTitle = "系统硬件监视器 - 未连接";
 Log.Warning($"共享内存连接失败: {_sharedMemoryService.LastError}");
 ShowDisconnectedState();
 }
 }

 private void ShowDisconnectedState()
 {
 CpuName = "?连接已断开";
 TotalMemory = "Disconnected";
 UsedMemory = "Disconnected";
 AvailableMemory = "Disconnected";
 Gpus.Clear(); NetworkAdapters.Clear(); Disks.Clear(); PhysicalDisks.Clear();
 SelectedGpu = null; SelectedNetworkAdapter = null; SelectedDisk = null; SelectedPhysicalDisk = null;
 }

 private void ShowErrorState(string msg)
 {
 CpuName = $"?? 数据读取失败: {msg}";
 TotalMemory = "Read failed";
 UsedMemory = "Read failed";
 AvailableMemory = "Read failed";
 }

 private void UpdateSystemData(SystemInfo si)
 {
 try
 {
 CpuName = ValidateAndSetString(si.CpuName, "Unknown Processor");
 PhysicalCores = ValidateAndSetInt(si.PhysicalCores, "Physical Cores");
 LogicalCores = ValidateAndSetInt(si.LogicalCores, "Logical Cores");
 PerformanceCores = ValidateAndSetInt(si.PerformanceCores, "Performance Cores");
 EfficiencyCores = ValidateAndSetInt(si.EfficiencyCores, "Efficiency Cores");
 CpuUsage = ValidateAndSetDouble(si.CpuUsage, "CPU使用率");
 HyperThreading = si.HyperThreading; Virtualization = si.Virtualization;
 CpuTemperature = ValidateAndSetDouble(si.CpuTemperature, "CPU温度");
 CpuBaseFrequencyMHz = ValidateAndSetDouble(si.CpuBaseFrequencyMHz, "CPU基准频率");
 CpuCurrentFrequencyMHz = ValidateAndSetDouble(si.CpuCurrentFrequencyMHz, "CPU即时频率");
 CpuUsageSampleIntervalMs = ValidateAndSetDouble(si.CpuUsageSampleIntervalMs, "Sample Interval");
 TotalMemory = si.TotalMemory >0 ? FormatBytes(si.TotalMemory) : "Not Detected";
 UsedMemory = si.UsedMemory >0 ? FormatBytes(si.UsedMemory) : "Unknown";
 AvailableMemory = si.AvailableMemory >0 ? FormatBytes(si.AvailableMemory) : "Unknown";
 MemoryUsagePercent = si.TotalMemory >0 ? (double)si.UsedMemory / si.TotalMemory *100.0 :0.0;

 UpdateCollection(Gpus, si.Gpus ?? new List<GpuData>());
 if (SelectedGpu == null && Gpus.Count >0) SelectedGpu = Gpus[0];
 GpuTemperature = ValidateAndSetDouble(si.GpuTemperature, "GPU温度");

 string? prevNetKey = SelectedNetworkAdapter != null ? $"{SelectedNetworkAdapter.Name}|{SelectedNetworkAdapter.Mac}" : null;
 UpdateCollection(NetworkAdapters, si.Adapters ?? new List<NetworkAdapterData>());
 if (prevNetKey != null)
 {
 var restored = NetworkAdapters.FirstOrDefault(a => $"{a.Name}|{a.Mac}" == prevNetKey);
 if (restored != null) SelectedNetworkAdapter = restored;
 }
 if (SelectedNetworkAdapter == null && NetworkAdapters.Count >0) SelectedNetworkAdapter = NetworkAdapters[0];

 string? prevDiskKey = SelectedDisk != null ? $"{SelectedDisk.Letter}:{SelectedDisk.Label}" : null;
 string? prevPhysicalKey = SelectedPhysicalDisk?.Disk?.SerialNumber;
 UpdateCollection(Disks, si.Disks ?? new List<DiskData>());
 BuildOrUpdatePhysicalDisks(si);
 if (prevDiskKey != null)
 SelectedDisk = Disks.FirstOrDefault(d => $"{d.Letter}:{d.Label}" == prevDiskKey) ?? SelectedDisk;
 if (SelectedDisk == null && Disks.Count >0) SelectedDisk = Disks[0];
 if (prevPhysicalKey != null)
 SelectedPhysicalDisk = PhysicalDisks.FirstOrDefault(p => p.Disk.SerialNumber == prevPhysicalKey) ?? SelectedPhysicalDisk;
 if (SelectedPhysicalDisk == null && PhysicalDisks.Count >0)
 SelectedPhysicalDisk = PhysicalDisks[0];

 HasTpm = si.HasTpm; TpmManufacturer = si.TpmManufacturer ?? "N/A"; TpmManufacturerId = si.TpmManufacturerId ?? "N/A"; TpmVersion = si.TpmVersion ?? "N/A"; TpmFirmwareVersion = si.TpmFirmwareVersion ?? "N/A"; TpmStatus = si.TpmStatus ?? "N/A"; TpmEnabled = si.TpmEnabled; TpmIsActivated = si.TpmIsActivated; TpmIsOwned = si.TpmIsOwned; TpmReady = si.TpmReady; TpmTbsAvailable = si.TpmTbsAvailable; TpmPhysicalPresenceRequired = si.TpmPhysicalPresenceRequired; TpmSpecVersion = si.TpmSpecVersion; TpmTbsVersion = si.TpmTbsVersion; TpmErrorMessage = si.TpmErrorMessage ?? "N/A"; TpmDetectionMethod = si.TpmDetectionMethod ?? "N/A";

 UpdateTemperatureCharts(si.CpuTemperature, si.GpuTemperature);
 Log.Debug($"系统数据更新完成: CPU={CpuName}, 内存使用率={MemoryUsagePercent:F1}%, GPU数量={Gpus.Count}");
 }
 catch (Exception ex)
 {
 Log.Error(ex, "Error occurred while updating system data");
 CpuName = $"?? 数据更新失败: {ex.Message}";
 }
 }

 private void BuildOrUpdatePhysicalDisks(SystemInfo si)
 {
 try
 {
 var map = PhysicalDisks.ToDictionary(p => p.Disk.SerialNumber, p => p);
 var alive = new HashSet<string>();
 foreach (var pd in si.PhysicalDisks)
 {
 if (!map.TryGetValue(pd.SerialNumber, out var view))
 {
 view = new PhysicalDiskView { Disk = pd };
 PhysicalDisks.Add(view);
 }
 else
 {
 view.Disk = pd;
 }
 alive.Add(pd.SerialNumber);
 var partitions = si.Disks.Where(d => d.PhysicalDiskIndex >=0 && d.PhysicalDiskIndex < si.PhysicalDisks.Count && si.PhysicalDisks[d.PhysicalDiskIndex].SerialNumber == pd.SerialNumber).ToList();
 view.Partitions.Clear();
 foreach (var part in partitions) view.Partitions.Add(part);
 }
 for (int i = PhysicalDisks.Count -1; i >=0; i--)
 if (!alive.Contains(PhysicalDisks[i].Disk.SerialNumber)) PhysicalDisks.RemoveAt(i);
 if (SelectedPhysicalDisk != null && !PhysicalDisks.Contains(SelectedPhysicalDisk)) SelectedPhysicalDisk = PhysicalDisks.FirstOrDefault();
 }
 catch (Exception ex) { Log.Error(ex, "Failed to build physical disk grouping"); }
 }

 private string ValidateAndSetString(string? value, string fieldName)
 {
 if (string.IsNullOrWhiteSpace(value))
 {
 Log.Debug($"{fieldName} 数据为空，使用默认值");
 return $"{fieldName} 未检测到";
 }
 return value!;
 }
 private int ValidateAndSetInt(int value, string fieldName)
 {
 if (value <=0)
 {
 Log.Debug($"{fieldName} 数据异常: {value}，使用默认值0");
 return 0;
 }
 return value;
 }
 private double ValidateAndSetDouble(double value, string fieldName)
 {
 if (double.IsNaN(value) || double.IsInfinity(value) || value <0)
 {
 Log.Debug($"{fieldName} 数据异常: {value} 使用默认值0");
 return 0.0;
 }
 return value;
 }

 private void UpdateCollection<T>(ObservableCollection<T> collection, List<T> newItems)
 {
 try
 {
 if (newItems == null)
 {
 if (collection.Count >0) collection.Clear();
 return;
 }
 if (collection.Count != newItems.Count || !collection.SequenceEqual(newItems))
 {
 collection.Clear();
 foreach (var item in newItems) collection.Add(item);
 }
 }
 catch (Exception ex) { Log.Error(ex, $"更新集合时发生错误: {typeof(T).Name}"); }
 }

 private void UpdateTemperatureCharts(double cpuTemp, double gpuTemp)
 {
 try
 {
 if (cpuTemp >0 && cpuTemp <150) _cpuTempData.Add(cpuTemp); else if (_cpuTempData.Count ==0) _cpuTempData.Add(0);
 if (gpuTemp >0 && gpuTemp <150) _gpuTempData.Add(gpuTemp); else if (_gpuTempData.Count ==0) _gpuTempData.Add(0);
 while (_cpuTempData.Count > MAX_CHART_POINTS) _cpuTempData.RemoveAt(0);
 while (_gpuTempData.Count > MAX_CHART_POINTS) _gpuTempData.RemoveAt(0);
 }
 catch (Exception ex) { Log.Error(ex, "Error occurred while updating temperature chart"); }
 }

 private string FormatBytes(ulong bytes)
 {
 if (bytes ==0) return "0 B";
 const ulong KB =1024UL, MB = KB * KB, GB = MB * KB, TB = GB * KB;
 return bytes >= TB ? $"{(double)bytes / TB:F1} TB" : bytes >= GB ? $"{(double)bytes / GB:F1} GB" : bytes >= MB ? $"{(double)bytes / MB:F1} MB" : bytes >= KB ? $"{(double)bytes / KB:F1} KB" : $"{bytes} B";
 }
 public string FormatFrequency(double f) => f <=0 ? "Unknown" : (f >=1000 ? $"{f /1000.0:F1} GHz" : $"{f:F1} MHz");
 public string FormatPercentage(double v) => (double.IsNaN(v) || v <0) ? "Unknown" : $"{v:F1}%";
 public string FormatNetworkSpeed(ulong bps)
 {
 if (bps ==0) return "Disconnected";
 const ulong K =1000UL, M = K * K, G = M * K;
 return bps >= G ? $"{(double)bps / G:F1} Gbps" : bps >= M ? $"{(double)bps / M:F1} Mbps" : bps >= K ? $"{(double)bps / K:F1} Kbps" : $"{bps} bps";
 }

 [RelayCommand] private void ShowSmartDetails()
 {
 if (SelectedDisk != null) Log.Information($"显示磁盘 {SelectedDisk.Letter}: 的SMART详情");
 else Log.Warning("No disk selected");
 }
 [RelayCommand] private void Reconnect() { Log.Information("User manually requested reconnection"); _consecutiveErrors =0; TryConnect(); }

 protected override void OnPropertyChanged(PropertyChangedEventArgs e)
 {
 base.OnPropertyChanged(e);
 if (e.PropertyName == nameof(IsConnected)) Log.Debug($"连接状态变化: {IsConnected}");
 }
 }
}