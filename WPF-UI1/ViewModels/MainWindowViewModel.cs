using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Windows.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using LiveChartsCore;
using LiveChartsCore.SkiaSharpView;
using LiveChartsCore.SkiaSharpView.Painting;
using SkiaSharp;
using WPF_UI1.Models;
using WPF_UI1.Services;
using Serilog;

namespace WPF_UI1.ViewModels
{
    public partial class MainWindowViewModel : ObservableObject
    {
        private readonly SharedMemoryService _sharedMemoryService;
        private readonly DispatcherTimer _updateTimer;
        private const int MAX_CHART_POINTS = 60;
        private int _consecutiveErrors = 0;
        private const int MAX_CONSECUTIVE_ERRORS = 5;

        [ObservableProperty]
        private bool isConnected;

        [ObservableProperty]
        private string connectionStatus = "正在连接...";

        [ObservableProperty]
        private string windowTitle = "系统硬件监视器";

        [ObservableProperty]
        private string lastUpdateTime = "从未更新";

        #region CPU Properties
        [ObservableProperty]
        private string cpuName = "正在检测...";

        [ObservableProperty]
        private int physicalCores;

        [ObservableProperty]
        private int logicalCores;

        [ObservableProperty]
        private int performanceCores;

        [ObservableProperty]
        private int efficiencyCores;

        [ObservableProperty]
        private double cpuUsage;

        [ObservableProperty]
        private bool hyperThreading;

        [ObservableProperty]
        private bool virtualization;

        [ObservableProperty]
        private double cpuTemperature;
        #endregion

        #region Memory Properties
        [ObservableProperty]
        private string totalMemory = "检测中...";

        [ObservableProperty]
        private string usedMemory = "检测中...";

        [ObservableProperty]
        private string availableMemory = "检测中...";

        [ObservableProperty]
        private double memoryUsagePercent;
        #endregion

        #region GPU Properties
        [ObservableProperty]
        private ObservableCollection<GpuData> gpus = new();

        [ObservableProperty]
        private GpuData? selectedGpu;

        [ObservableProperty]
        private double gpuTemperature;
        #endregion

        #region Network Properties
        [ObservableProperty]
        private ObservableCollection<NetworkAdapterData> networkAdapters = new();

        [ObservableProperty]
        private NetworkAdapterData? selectedNetworkAdapter;
        #endregion

        #region Disk Properties
        [ObservableProperty]
        private ObservableCollection<DiskData> disks = new();

        [ObservableProperty]
        private DiskData? selectedDisk;
        #endregion

        #region Chart Properties
        public ObservableCollection<ISeries> CpuTemperatureSeries { get; } = new();
        public ObservableCollection<ISeries> GpuTemperatureSeries { get; } = new();

        private readonly ObservableCollection<double> _cpuTempData = new();
        private readonly ObservableCollection<double> _gpuTempData = new();
        #endregion

        public MainWindowViewModel(SharedMemoryService sharedMemoryService)
        {
            _sharedMemoryService = sharedMemoryService;
            
            InitializeCharts();
            
            // 设置更新定时器
            _updateTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromMilliseconds(500) // 稍微降低频率以减少CPU占用
            };
            _updateTimer.Tick += UpdateTimer_Tick;
            _updateTimer.Start();

            // 初次尝试连接
            TryConnect();
        }

        private void InitializeCharts()
        {
            // CPU温度图表
            CpuTemperatureSeries.Add(new LineSeries<double>
            {
                Values = _cpuTempData,
                Name = "CPU温度",
                Stroke = new SolidColorPaint(SKColors.DeepSkyBlue) { StrokeThickness = 2 },
                Fill = null,
                GeometrySize = 0 // 隐藏数据点
            });

            // GPU温度图表
            GpuTemperatureSeries.Add(new LineSeries<double>
            {
                Values = _gpuTempData,
                Name = "GPU温度",
                Stroke = new SolidColorPaint(SKColors.Orange) { StrokeThickness = 2 },
                Fill = null,
                GeometrySize = 0
            });
        }

        private async void UpdateTimer_Tick(object? sender, EventArgs e)
        {
            await UpdateSystemInfoAsync();
        }

        private async Task UpdateSystemInfoAsync()
        {
            try
            {
                var systemInfo = await Task.Run(() => _sharedMemoryService.ReadSystemInfo());
                
                if (systemInfo != null)
                {
                    _consecutiveErrors = 0; // 重置错误计数器

                    if (!IsConnected)
                    {
                        IsConnected = true;
                        ConnectionStatus = "已连接";
                        WindowTitle = "系统硬件监视器";
                    }

                    UpdateSystemData(systemInfo);
                    LastUpdateTime = DateTime.Now.ToString("HH:mm:ss");
                }
                else
                {
                    _consecutiveErrors++;
                    
                    if (_consecutiveErrors >= MAX_CONSECUTIVE_ERRORS)
                    {
                        if (IsConnected)
                        {
                            IsConnected = false;
                            ConnectionStatus = "连接已断开 - 尝试重新连接...";
                            WindowTitle = "系统硬件监视器 - 连接中断";
                            
                            // 显示错误状态的数据
                            ShowDisconnectedState();
                            
                            // 尝试重新连接
                            await Task.Run(() => TryConnect());
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Log.Error(ex, "更新系统信息时发生错误");
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
                _consecutiveErrors = 0;
                Log.Information("共享内存连接成功");
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
            CpuName = "? 连接已断开";
            TotalMemory = "未连接";
            UsedMemory = "未连接";
            AvailableMemory = "未连接";
            
            // 清空集合
            Gpus.Clear();
            NetworkAdapters.Clear();
            Disks.Clear();
            
            // 重置选择
            SelectedGpu = null;
            SelectedNetworkAdapter = null;
            SelectedDisk = null;
        }

        private void ShowErrorState(string errorMessage)
        {
            CpuName = $"?? 数据读取失败: {errorMessage}";
            TotalMemory = "读取失败";
            UsedMemory = "读取失败";
            AvailableMemory = "读取失败";
        }

        private void UpdateSystemData(SystemInfo systemInfo)
        {
            try
            {
                // 更新CPU信息 - 添加数据验证
                CpuName = ValidateAndSetString(systemInfo.CpuName, "未知处理器");
                PhysicalCores = ValidateAndSetInt(systemInfo.PhysicalCores, "物理核心");
                LogicalCores = ValidateAndSetInt(systemInfo.LogicalCores, "逻辑核心");
                PerformanceCores = ValidateAndSetInt(systemInfo.PerformanceCores, "性能核心");
                EfficiencyCores = ValidateAndSetInt(systemInfo.EfficiencyCores, "能效核心");
                CpuUsage = ValidateAndSetDouble(systemInfo.CpuUsage, "CPU使用率");
                HyperThreading = systemInfo.HyperThreading;
                Virtualization = systemInfo.Virtualization;
                CpuTemperature = ValidateAndSetDouble(systemInfo.CpuTemperature, "CPU温度");

                // 更新内存信息 - 添加数据验证
                TotalMemory = systemInfo.TotalMemory > 0 ? FormatBytes(systemInfo.TotalMemory) : "未检测到";
                UsedMemory = systemInfo.UsedMemory > 0 ? FormatBytes(systemInfo.UsedMemory) : "未知";
                AvailableMemory = systemInfo.AvailableMemory > 0 ? FormatBytes(systemInfo.AvailableMemory) : "未知";
                
                MemoryUsagePercent = systemInfo.TotalMemory > 0 
                    ? (double)systemInfo.UsedMemory / systemInfo.TotalMemory * 100.0 
                    : 0.0;

                // 更新GPU信息
                UpdateCollection(Gpus, systemInfo.Gpus ?? new List<GpuData>());
                if (SelectedGpu == null && Gpus.Count > 0)
                    SelectedGpu = Gpus[0];

                GpuTemperature = ValidateAndSetDouble(systemInfo.GpuTemperature, "GPU温度");

                // 更新网络信息
                UpdateCollection(NetworkAdapters, systemInfo.Adapters ?? new List<NetworkAdapterData>());
                if (SelectedNetworkAdapter == null && NetworkAdapters.Count > 0)
                    SelectedNetworkAdapter = NetworkAdapters[0];

                // 更新磁盘信息
                UpdateCollection(Disks, systemInfo.Disks ?? new List<DiskData>());
                if (SelectedDisk == null && Disks.Count > 0)
                    SelectedDisk = Disks[0];

                // 更新温度图表
                UpdateTemperatureCharts(systemInfo.CpuTemperature, systemInfo.GpuTemperature);

                Log.Debug($"系统数据更新完成: CPU={CpuName}, 内存使用率={MemoryUsagePercent:F1}%, GPU数量={Gpus.Count}");
            }
            catch (Exception ex)
            {
                Log.Error(ex, "更新系统数据时发生错误");
                CpuName = $"?? 数据更新失败: {ex.Message}";
            }
        }

        private string ValidateAndSetString(string? value, string fieldName)
        {
            if (string.IsNullOrWhiteSpace(value))
            {
                Log.Debug($"{fieldName} 数据为空，使用默认值");
                return $"{fieldName} 未检测到";
            }
            return value;
        }

        private int ValidateAndSetInt(int value, string fieldName)
        {
            if (value <= 0)
            {
                Log.Debug($"{fieldName} 数据异常: {value}，使用默认值0");
                return 0;
            }
            return value;
        }

        private double ValidateAndSetDouble(double value, string fieldName)
        {
            if (double.IsNaN(value) || double.IsInfinity(value) || value < 0)
            {
                Log.Debug($"{fieldName} 数据异常: {value}，使用默认值0");
                return 0.0;
            }
            return value;
        }

        private void UpdateCollection<T>(ObservableCollection<T> collection, List<T> newItems)
        {
            // 改进的更新策略：只在必要时更新集合
            try
            {
                if (newItems == null)
                {
                    if (collection.Count > 0)
                    {
                        collection.Clear();
                    }
                    return;
                }

                if (collection.Count != newItems.Count || !collection.SequenceEqual(newItems))
                {
                    collection.Clear();
                    foreach (var item in newItems)
                    {
                        collection.Add(item);
                    }
                    
                    Log.Debug($"更新集合: {typeof(T).Name}, 数量: {newItems.Count}");
                }
            }
            catch (Exception ex)
            {
                Log.Error(ex, $"更新集合时发生错误: {typeof(T).Name}");
            }
        }

        private void UpdateTemperatureCharts(double cpuTemp, double gpuTemp)
        {
            try
            {
                // 验证温度数据的有效性
                if (cpuTemp > 0 && cpuTemp < 150) // 合理的CPU温度范围
                {
                    _cpuTempData.Add(cpuTemp);
                }
                else if (_cpuTempData.Count == 0)
                {
                    _cpuTempData.Add(0); // 如果没有数据，添加0以保持图表连续性
                }

                if (gpuTemp > 0 && gpuTemp < 150) // 合理的GPU温度范围
                {
                    _gpuTempData.Add(gpuTemp);
                }
                else if (_gpuTempData.Count == 0)
                {
                    _gpuTempData.Add(0);
                }

                // 保持最大数据点数量
                while (_cpuTempData.Count > MAX_CHART_POINTS)
                    _cpuTempData.RemoveAt(0);

                while (_gpuTempData.Count > MAX_CHART_POINTS)
                    _gpuTempData.RemoveAt(0);
            }
            catch (Exception ex)
            {
                Log.Error(ex, "更新温度图表时发生错误");
            }
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

        public string FormatFrequency(double frequency)
        {
            if (frequency <= 0) return "未知";
            
            return frequency >= 1000 
                ? $"{frequency / 1000.0:F1} GHz" 
                : $"{frequency:F1} MHz";
        }

        public string FormatPercentage(double value)
        {
            if (double.IsNaN(value) || value < 0) return "未知";
            return $"{value:F1}%";
        }

        public string FormatNetworkSpeed(ulong speedBps)
        {
            if (speedBps == 0) return "未连接";

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

        [RelayCommand]
        private void ShowSmartDetails()
        {
            if (SelectedDisk != null)
            {
                // TODO: 实现SMART详情对话框
                Log.Information($"显示磁盘 {SelectedDisk.Letter}: 的SMART详情");
            }
            else
            {
                Log.Warning("未选择磁盘，无法显示SMART详情");
            }
        }

        [RelayCommand]
        private void Reconnect()
        {
            Log.Information("用户手动请求重新连接");
            _consecutiveErrors = 0;
            TryConnect();
        }

        protected override void OnPropertyChanged(PropertyChangedEventArgs e)
        {
            base.OnPropertyChanged(e);
            
            // 可以在这里添加属性变化时的特殊处理逻辑
            if (e.PropertyName == nameof(IsConnected))
            {
                Log.Debug($"连接状态变化: {IsConnected}");
            }
        }
    }
}