#pragma once
#include <string>
#include <chrono>

class TimeUtils {
public:
    using SystemTimePoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<int64_t, std::ratio<1, 10000000>>>;

    static std::string FormatTimePoint(const SystemTimePoint& tp);
    static std::string GetBootTimeUtc();
    static std::string GetUptime();            // 新增：获取系统运行时间
    static uint64_t GetUptimeMilliseconds();   // 新增：获取运行时间（毫秒）
};
