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

std::string TimeUtils::GetBootTimeUtc() {
    ULONGLONG uptime = GetTickCount64();
    ULONGLONG currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    ULONGLONG bootTime = currentTime - uptime;

    FILETIME ftBootTime;
    LARGE_INTEGER li;
    li.QuadPart = bootTime * 10000; // 转换为100纳秒单位
    ftBootTime.dwLowDateTime = li.LowPart;
    ftBootTime.dwHighDateTime = li.HighPart;

    SYSTEMTIME stBootTime;
    if (!FileTimeToSystemTime(&ftBootTime, &stBootTime)) {
        return "Error: Failed to convert time";
    }

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