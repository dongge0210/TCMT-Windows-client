using System;
using System.Globalization;
using System.Windows.Data;
using System.Windows.Media;

namespace WPF_UI1.Converters
{
    public class BooleanToColorConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is bool boolValue)
            {
                return boolValue 
                    ? new SolidColorBrush(Color.FromRgb(76, 175, 80))  // Green
                    : new SolidColorBrush(Color.FromRgb(244, 67, 54)); // Red
            }
            return new SolidColorBrush(Colors.Gray);
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }

    public class BooleanToYesNoConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is bool boolValue)
            {
                return boolValue ? "启用" : "禁用";
            }
            return "未知";
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }

    public class BooleanToSupportConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is bool boolValue)
            {
                return boolValue ? "支持" : "不支持";
            }
            return "未知";
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }

    public class BytesToStringConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is ulong bytes)
            {
                return FormatBytes(bytes);
            }
            return "0 B";
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }

        private static string FormatBytes(ulong bytes)
        {
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
    }

    public class NetworkSpeedConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is ulong speedBps)
            {
                return FormatNetworkSpeed(speedBps);
            }
            return "0 bps";
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }

        private static string FormatNetworkSpeed(ulong speedBps)
        {
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
    }

    public class DiskUsagePercentageConverter : IMultiValueConverter
    {
        public object Convert(object[] values, Type targetType, object parameter, CultureInfo culture)
        {
            if (values.Length == 2 && 
                values[0] is ulong usedSpace && 
                values[1] is ulong totalSize && 
                totalSize > 0)
            {
                double percentage = (double)usedSpace / totalSize * 100.0;
                
                if (targetType == typeof(double))
                {
                    return percentage;
                }
                else if (targetType == typeof(string))
                {
                    return $"{percentage:F1}%";
                }
            }
            
            return targetType == typeof(double) ? 0.0 : "0%";
        }

        public object[] ConvertBack(object value, Type[] targetTypes, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }
}