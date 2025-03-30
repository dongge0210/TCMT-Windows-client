#pragma once
#include <string>
#include <chrono>

class TimeUtils {
public:
    using SystemTimePoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<int64_t, std::ratio<1, 10000000>>>;

    static std::string FormatTimePoint(const SystemTimePoint& tp);
    static std::string GetBootTimeUtc();
    static std::string GetUptime();
    static uint64_t GetUptimeMilliseconds();
    static std::string GetCurrentLocalTime();  // 新增：获取当前本地时间
    static std::string GetCurrentUtcTime();    // 新增：获取当前UTC时间
};

