// Platform.cpp - 平台抽象实现主文件
// 通过条件编译包含平台特定实现

#include "Platform.h"
#include <ctime>
#include <chrono>
#include <iomanip>
#include <sstream>

// ============================================================================
// 平台特定实现包含
// ============================================================================

#ifdef TCMT_WINDOWS
#include "Platform_Windows.cpp"
#elif defined(TCMT_MACOS)
#include "Platform_macOS.cpp"
#elif defined(TCMT_LINUX)
#include "Platform_Linux.cpp"
#else
// #error "Unsupported platform" - 暂时注释以进行调试
// 临时包含一个空实现
namespace Platform {
    // 空实现
}
#endif

// ============================================================================
// SystemTime实现（平台无关部分）
// ============================================================================

namespace Platform {

std::string SystemTime::ToString() const {
    std::ostringstream ss;
    ss << std::setfill('0')
       << std::setw(4) << year << "-"
       << std::setw(2) << month << "-"
       << std::setw(2) << day << " "
       << std::setw(2) << hour << ":"
       << std::setw(2) << minute << ":"
       << std::setw(2) << second << "."
       << std::setw(3) << milliseconds;
    return ss.str();
}

bool SystemTime::operator==(const SystemTime& other) const {
    return year == other.year &&
           month == other.month &&
           day == other.day &&
           hour == other.hour &&
           minute == other.minute &&
           second == other.second &&
           milliseconds == other.milliseconds;
}

bool SystemTime::operator!=(const SystemTime& other) const {
    return !(*this == other);
}

bool SystemTime::operator<(const SystemTime& other) const {
    if (year != other.year) return year < other.year;
    if (month != other.month) return month < other.month;
    if (day != other.day) return day < other.day;
    if (hour != other.hour) return hour < other.hour;
    if (minute != other.minute) return minute < other.minute;
    if (second != other.second) return second < other.second;
    return milliseconds < other.milliseconds;
}

bool SystemTime::operator>(const SystemTime& other) const {
    return other < *this;
}

// ============================================================================
// CriticalSection实现（平台无关部分）
// ============================================================================

CriticalSection::CriticalSection(CriticalSection&& other) noexcept {
#ifdef TCMT_WINDOWS
    // Windows: 从other转移资源
    cs_ = other.cs_;
    other.cs_ = CRITICAL_SECTION();
    InitializeCriticalSection(&cs_);
#elif defined(TCMT_MACOS) || defined(TCMT_LINUX)
    // macOS/Linux: 从other转移资源
    mutex_ = other.mutex_;
    // 将other重置为初始状态
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&other.mutex_, &attr);
    pthread_mutexattr_destroy(&attr);
#endif
}

CriticalSection& CriticalSection::operator=(CriticalSection&& other) noexcept {
    if (this != &other) {
        // 使用swap方式交换资源
        // 注意：对于CriticalSection，简单的按位交换可能不安全
        // 这里使用简化实现，假设平台特定类型可以安全交换
#ifdef TCMT_WINDOWS
        // Windows: 销毁当前对象，从other转移资源
        DeleteCriticalSection(&cs_);
        cs_ = other.cs_;
        other.cs_ = CRITICAL_SECTION();
        InitializeCriticalSection(&cs_);
#elif defined(TCMT_MACOS) || defined(TCMT_LINUX)
        // macOS/Linux: 销毁当前对象，从other转移资源
        pthread_mutex_destroy(&mutex_);
        mutex_ = other.mutex_;
        // 将other重置为初始状态
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&other.mutex_, &attr);
        pthread_mutexattr_destroy(&attr);
#endif
    }
    return *this;
}

// ============================================================================
// StringConverter实现（平台无关部分）
// ============================================================================

// StringConverter的实现通过条件编译包含在平台特定文件中
// 这些函数在Platform.h中声明为静态成员函数
// 具体实现在Platform_Windows.cpp或Platform_macOS.cpp中提供

// ============================================================================
// SystemUtils实现（平台无关部分）
// ============================================================================

void SystemUtils::Sleep(uint32_t milliseconds) {
#ifdef TCMT_WINDOWS
    ::Sleep(milliseconds);
#elif defined(TCMT_MACOS) || defined(TCMT_LINUX)
    usleep(milliseconds * 1000);
#endif
}

uint64_t SystemUtils::GetTickCount() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

uint64_t SystemUtils::GetHighResolutionTime() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

std::string SystemUtils::GetEnvironmentVariable(const std::string& name) {
#ifdef TCMT_WINDOWS
    char* value = nullptr;
    size_t size = 0;
    _dupenv_s(&value, &size, name.c_str());
    if (value) {
        std::string result(value);
        free(value);
        return result;
    }
    return "";
#elif defined(TCMT_MACOS) || defined(TCMT_LINUX)
    const char* value = std::getenv(name.c_str());
    return value ? std::string(value) : "";
#else
    // 如果没有平台宏定义，返回空字符串
    return "";
#endif
}

bool SystemUtils::SetEnvironmentVariable(const std::string& name, const std::string& value) {
#ifdef TCMT_WINDOWS
    return _putenv_s(name.c_str(), value.c_str()) == 0;
#elif defined(TCMT_MACOS) || defined(TCMT_LINUX)
    return setenv(name.c_str(), value.c_str(), 1) == 0;
#else
    // 如果没有平台宏定义，返回false
    return false;
#endif
}

} // namespace Platform

