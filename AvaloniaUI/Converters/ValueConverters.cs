using System;
using System.Globalization;
using Avalonia.Data.Converters;
using Avalonia.Media;

namespace AvaloniaUI.Converters;

public class NullToDefaultConverter : IValueConverter
{
    public object? DefaultValue { get; set; } = "N/A";

    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value == null)
            return DefaultValue ?? "N/A";
        return value;
    }

    public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        throw new NotImplementedException();
    }
}

public class BoolToColorConverter : IValueConverter
{
    private static readonly SolidColorBrush GreenBrush = new(Color.Parse("#FF4CAF50"));
    private static readonly SolidColorBrush RedBrush = new(Color.Parse("#FFF44336"));
    private static readonly SolidColorBrush GrayBrush = new(Colors.Gray);

    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value is bool isConnected)
            return isConnected ? GreenBrush : RedBrush;
        return GrayBrush;
    }

    public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        throw new NotImplementedException();
    }
}

public class PercentToWarningColorConverter : IValueConverter
{
    private static readonly SolidColorBrush RedBrush = new(Color.Parse("#FFF44336"));
    private static readonly SolidColorBrush OrangeBrush = new(Color.Parse("#FFFF9800"));
    private static readonly SolidColorBrush GreenBrush = new(Color.Parse("#FF4CAF50"));
    private static readonly SolidColorBrush GrayBrush = new(Colors.Gray);

    public double WarningThreshold { get; set; } = 80;
    public double CriticalThreshold { get; set; } = 95;

    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value is double percent)
        {
            if (percent >= CriticalThreshold) return RedBrush;
            if (percent >= WarningThreshold) return OrangeBrush;
            return GreenBrush;
        }
        return GrayBrush;
    }

    public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        throw new NotImplementedException();
    }
}

public class TemperatureToWarningColorConverter : IValueConverter
{
    private static readonly SolidColorBrush RedBrush = new(Color.Parse("#FFF44336"));
    private static readonly SolidColorBrush OrangeBrush = new(Color.Parse("#FFFF9800"));
    private static readonly SolidColorBrush GreenBrush = new(Color.Parse("#FF4CAF50"));
    private static readonly SolidColorBrush GrayBrush = new(Colors.Gray);

    public double WarningThreshold { get; set; } = 70;
    public double CriticalThreshold { get; set; } = 85;

    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value is double temp)
        {
            if (temp >= CriticalThreshold) return RedBrush;
            if (temp >= WarningThreshold) return OrangeBrush;
            return GreenBrush;
        }
        return GrayBrush;
    }

    public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        throw new NotImplementedException();
    }
}
