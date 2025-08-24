using System.Windows;
using Microsoft.Extensions.DependencyInjection;
using WPF_UI1.ViewModels;
using WPF_UI1.Services;
using Serilog;
using System.Windows.Controls; // 事件处理需要
using WPF_UI1.Models; // DiskData

namespace WPF_UI1
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            
            // 配置依赖注入
            var services = new ServiceCollection();
            ConfigureServices(services);
            var serviceProvider = services.BuildServiceProvider();
            
            // 设置DataContext
            DataContext = serviceProvider.GetRequiredService<MainWindowViewModel>();
            
            Log.Information("主窗口初始化完成");
        }

        private void ConfigureServices(IServiceCollection services)
        {
            // 注册服务
            services.AddSingleton<SharedMemoryService>();
            services.AddTransient<MainWindowViewModel>();
            
            Log.Information("服务注册完成");
        }

        protected override void OnClosed(EventArgs e)
        {
            // 清理资源
            if (DataContext is MainWindowViewModel viewModel)
            {
                // ViewModel会自动通过依赖注入管理SharedMemoryService的生命周期
            }
            
            Log.Information("主窗口已关闭");
            base.OnClosed(e);
        }

        private void RadioButton_Checked(object sender, RoutedEventArgs e)
        {
            if (DataContext is MainWindowViewModel vm && sender is RadioButton rb && rb.Tag is DiskData disk)
            {
                vm.SelectedDisk = disk;
            }
        }
    }
}