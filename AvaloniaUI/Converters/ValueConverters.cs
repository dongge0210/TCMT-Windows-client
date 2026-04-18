using System;
using System.Globalization;
using Avalonia.Data.Converters;
using Avalonia.Media;

namespace AvaloniaUI.Converters;

/// <summary>
/// Converts null to default value, useful for binding errors
/// </summary>
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
    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value is bool isConnected)
        {
            return isConnected ? new SolidColorBrush(Color.Parse("#FF4CAF50")) : new SolidColorBrush(Color.Parse("#FFF44336"));
        }
        return new SolidColorBrush(Colors.Gray);
    }

    public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        throw new NotImplementedException();
    }
}

/// <summary>
/// Converts percentage value to warning color (green -> yellow -> red)
/// </summary>
public class PercentToWarningColorConverter : IValueConverter
{
    public double WarningThreshold { get; set; } = 80;
    public double CriticalThreshold { get; set; } = 95;

    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value is double percent)
        {
            if (percent >= CriticalThreshold)
                return new SolidColorBrush(Color.Parse("#FFF44336")); // Red
            if (percent >= WarningThreshold)
                return new SolidColorBrush(Color.Parse("#FFFF9800")); // Orange
            return new SolidColorBrush(Color.Parse("#FF4CAF50")); // Green
        }
        return new SolidColorBrush(Colors.Gray);
    }

    public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        throw new NotImplementedException();
    }
}

/// <summary>
/// Converts temperature to warning color (green -> orange -> red)
/// </summary>
public class TemperatureToWarningColorConverter : IValueConverter
{
    public double WarningThreshold { get; set; } = 70;
    public double CriticalThreshold { get; set; } = 85;

    public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value is double temp)
        {
            if (temp >= CriticalThreshold)
                return new SolidColorBrush(Color.Parse("#FFF44336")); // Red
            if (temp >= WarningThreshold)
                return new SolidColorBrush(Color.Parse("#FFFF9800")); // Orange
            return new SolidColorBrush(Color.Parse("#FF4CAF50")); // Green
        }
        return new SolidColorBrush(Colors.Gray);
    }

    public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        throw new NotImplementedException();
    }
}