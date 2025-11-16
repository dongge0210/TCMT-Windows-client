using System.Windows;
using Microsoft.Extensions.DependencyInjection;
using WPF_UI1.ViewModels;
using WPF_UI1.Services;
using Serilog;

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
            
            // Configure dependency injection
            var services = new ServiceCollection();
            ConfigureServices(services);
            var serviceProvider = services.BuildServiceProvider();
            
            // Set DataContext
            DataContext = serviceProvider.GetRequiredService<MainWindowViewModel>();
            
            Log.Information("Main window initialization completed");
        }

        private void ConfigureServices(IServiceCollection services)
        {
            // Register services
            services.AddSingleton<SharedMemoryService>();
            services.AddTransient<MainWindowViewModel>();
            
            Log.Information("Service registration completed");
        }

        protected override void OnClosed(EventArgs e)
        {
            // Cleanup resources
            if (DataContext is MainWindowViewModel viewModel)
            {
                // ViewModel will automatically manage SharedMemoryService lifecycle through dependency injection
            }
            
            Log.Information("Main window closed");
            base.OnClosed(e);
        }
    }
}