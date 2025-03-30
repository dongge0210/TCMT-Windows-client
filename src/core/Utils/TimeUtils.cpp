#include "TimeUtils.h"
#include <iomanip>
#include <sstream>
#include <windows.h>

std::string TimeUtils::FormatTimePoint(const SystemTimePoint& tp) {
    auto sys_tp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(tp);
    std::time_t time = std::chrono::system_clock::to_time_t(sys_tp);
    std::tm tm;
    gmtime_s(&tm, &time);
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string TimeUtils::GetCurrentLocalTime() {
    SYSTEMTIME localTime;
    GetLocalTime(&localTime);

    std::stringstream ss;
    ss << std::setfill('0')
        << localTime.wYear << "-"
        << std::setw(2) << localTime.wMonth << "-"
        << std::setw(2) << localTime.wDay << " "
        << std::setw(2) << localTime.wHour << ":"
        << std::setw(2) << localTime.wMinute << ":"
        << std::setw(2) << localTime.wSecond;

    return ss.str();
}

std::string TimeUtils::GetCurrentUtcTime() {
    SYSTEMTIME utcTime;
    GetSystemTime(&utcTime);

    std::stringstream ss;
    ss << std::setfill('0')
        << utcTime.wYear << "-"
        << std::setw(2) << utcTime.wMonth << "-"
        << std::setw(2) << utcTime.wDay << " "
        << std::setw(2) << utcTime.wHour << ":"
        << std::setw(2) << utcTime.wMinute << ":"
        << std::setw(2) << utcTime.wSecond;

    return ss.str();
}

std::string TimeUtils::GetBootTimeUtc() {
    // 获取当前系统时间
    FILETIME now;
    GetSystemTimeAsFileTime(&now);

    // 获取系统运行时间（毫秒）
    ULONGLONG uptime = GetTickCount64();

    // 将当前时间转换为64位整数（100纳秒为单位）
    ULONGLONG currentTime = ((ULONGLONG)now.dwHighDateTime << 32) | now.dwLowDateTime;

    // 计算启动时间（100纳秒为单位）
    ULONGLONG bootTime = currentTime - (uptime * 10000); // 转换毫秒到100纳秒

    // 转换回FILETIME结构
    FILETIME ftBootTime;
    ftBootTime.dwHighDateTime = bootTime >> 32;
    ftBootTime.dwLowDateTime = bootTime & 0xFFFFFFFF;

    // 转换为本地时间
    FILETIME localBootTime;
    FileTimeToLocalFileTime(&ftBootTime, &localBootTime);

    // 转换为SYSTEMTIME结构
    SYSTEMTIME stBootTime;
    FileTimeToSystemTime(&localBootTime, &stBootTime);

    // 格式化输出
    std::stringstream ss;
    ss << std::setfill('0')
        << stBootTime.wYear << "-"
        << std::setw(2) << stBootTime.wMonth << "-"
        << std::setw(2) << stBootTime.wDay << " "
        << std::setw(2) << stBootTime.wHour << ":"
        << std::setw(2) << stBootTime.wMinute << ":"
        << std::setw(2) << stBootTime.wSecond;

    return ss.str();
}

uint64_t TimeUtils::GetUptimeMilliseconds() {
    return GetTickCount64();
}

std::string TimeUtils::GetUptime() {
    ULONGLONG uptime = GetTickCount64();

    // 转换为天、小时、分钟
    ULONGLONG days = uptime / (1000 * 60 * 60 * 24);
    ULONGLONG hours = (uptime % (1000 * 60 * 60 * 24)) / (1000 * 60 * 60);
    ULONGLONG minutes = (uptime % (1000 * 60 * 60)) / (1000 * 60);
    ULONGLONG seconds = (uptime % (1000 * 60)) / 1000;

    std::stringstream ss;

    // 格式化输出
    if (days > 0) {
        ss << days << "天 ";
    }
    if (hours > 0 || days > 0) {
        ss << hours << "小时 ";
    }
    if (minutes > 0 || hours > 0 || days > 0) {
        ss << minutes << "分钟 ";
    }
    ss << seconds << "秒";

    return ss.str();
}
