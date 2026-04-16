// Platform.h - 平台抽象接口
#pragma once

#include <string>
#include <cstdint>
#include <memory>
#include <vector>

// 平台宏定义 - 在CMake中设置
// 注意：这些宏由CMake通过add_definitions()或target_compile_definitions()定义
// 此处不进行检查，因为检查应该在构建系统中完成

// 平台特定头文件包含（通过条件编译）
#ifdef TCMT_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <synchapi.h>
#elif defined(TCMT_MACOS)
#include <sys/time.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#elif defined(TCMT_LINUX)
#include <sys/time.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

namespace Platform {

// ============================================================================
// 系统时间类型（跨平台）
// ============================================================================

struct SystemTime {
    uint16_t year;          // 年份
    uint16_t month;         // 月份 (1-12)
    uint16_t dayOfWeek;     // 星期几 (0=星期日, 6=星期六)
    uint16_t day;           // 日 (1-31)
    uint16_t hour;          // 小时 (0-23)
    uint16_t minute;        // 分钟 (0-59)
    uint16_t second;        // 秒 (0-59)
    uint16_t milliseconds;  // 毫秒 (0-999)

    SystemTime() : year(0), month(0), dayOfWeek(0), day(0),
                   hour(0), minute(0), second(0), milliseconds(0) {}

    // 转换为字符串
    std::string ToString() const;

    // 获取当前系统时间
    static SystemTime Now();

    // 比较操作
    bool operator==(const SystemTime& other) const;
    bool operator!=(const SystemTime& other) const;
    bool operator<(const SystemTime& other) const;
    bool operator>(const SystemTime& other) const;
};

// ============================================================================
// 临界区/互斥锁（跨平台）
// ============================================================================

class CriticalSection {
public:
    CriticalSection();
    ~CriticalSection();

    // 禁止拷贝
    CriticalSection(const CriticalSection&) = delete;
    CriticalSection& operator=(const CriticalSection&) = delete;

    // 允许移动（如果需要）
    CriticalSection(CriticalSection&& other) noexcept;
    CriticalSection& operator=(CriticalSection&& other) noexcept;

    // 加锁
    void Enter();

    // 解锁
    void Leave();

    // 尝试加锁
    bool TryEnter();

private:
#ifdef TCMT_WINDOWS
    CRITICAL_SECTION cs_;
#elif defined(TCMT_MACOS) || defined(TCMT_LINUX)
    pthread_mutex_t mutex_;
#endif
};

// RAII包装器
class ScopedLock {
public:
    explicit ScopedLock(CriticalSection& cs) : cs_(cs) { cs_.Enter(); }
    ~ScopedLock() { cs_.Leave(); }

    // 禁止拷贝
    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;

private:
    CriticalSection& cs_;
};

// ============================================================================
// 共享内存（跨平台）
// ============================================================================

class SharedMemory {
public:
    SharedMemory();
    ~SharedMemory();

    // 创建或打开共享内存
    bool Create(const std::string& name, size_t size);

    // 打开现有共享内存
    bool Open(const std::string& name, size_t size);

    // 映射到进程地址空间
    bool Map();

    // 取消映射
    bool Unmap();

    // 获取映射地址
    void* GetAddress() const { return address_; }

    // 获取大小
    size_t GetSize() const { return size_; }

    // 是否已创建
    bool IsCreated() const { return created_; }

    // 获取错误信息
    std::string GetLastError() const { return last_error_; }

private:
    void* address_;
    size_t size_;
    bool created_;
    std::string last_error_;

#ifdef TCMT_WINDOWS
    HANDLE hMapFile_;
#elif defined(TCMT_MACOS) || defined(TCMT_LINUX)
    int shm_fd_;
    std::string shm_name_;
#endif
};

// ============================================================================
// 进程间互斥锁（跨平台）
// ============================================================================

class InterprocessMutex {
public:
    InterprocessMutex();
    ~InterprocessMutex();

    // 创建或打开互斥锁
    bool Create(const std::string& name);

    // 打开现有互斥锁
    bool Open(const std::string& name);

    // 加锁
    bool Lock(uint32_t timeout_ms = 0xFFFFFFFF);

    // 解锁
    bool Unlock();

    // 获取错误信息
    std::string GetLastError() const { return last_error_; }

private:
    std::string name_;
    std::string last_error_;

#ifdef TCMT_WINDOWS
    HANDLE mutex_;
#elif defined(TCMT_MACOS) || defined(TCMT_LINUX)
    pthread_mutex_t* mutex_ptr_;
    SharedMemory shm_;
    bool is_creator_;
#endif
};

// ============================================================================
// 文件句柄包装器（跨平台）
// ============================================================================

class FileHandle {
public:
    FileHandle() : handle_(InvalidHandle()) {}
    ~FileHandle() { Close(); }

    // 从现有句柄创建（转移所有权）
    explicit FileHandle(void* handle) : handle_(handle) {}

    // 禁止拷贝
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    // 允许移动
    FileHandle(FileHandle&& other) noexcept : handle_(other.handle_) {
        other.handle_ = InvalidHandle();
    }

    FileHandle& operator=(FileHandle&& other) noexcept {
        if (this != &other) {
            Close();
            handle_ = other.handle_;
            other.handle_ = InvalidHandle();
        }
        return *this;
    }

    // 检查是否有效
    bool IsValid() const { return handle_ != InvalidHandle(); }

    // 获取原始句柄（不转移所有权）
    void* Get() const { return handle_; }

    // 关闭句柄
    void Close();

    // 无效句柄值
    static void* InvalidHandle();

private:
    void* handle_;
};

// ============================================================================
// 字符串转换（跨平台）
// ============================================================================

class StringConverter {
public:
    // UTF-8 到宽字符串（Windows）或UTF-16（其他平台）
    static std::wstring Utf8ToWide(const std::string& utf8);

    // 宽字符串到UTF-8
    static std::string WideToUtf8(const std::wstring& wide);

    // 当前代码页到UTF-8（主要用于Windows）
    static std::string AnsiToUtf8(const std::string& ansi);

    // UTF-8到当前代码页（主要用于Windows）
    static std::string Utf8ToAnsi(const std::string& utf8);

    // 检查字符串是否为有效的UTF-8
    static bool IsValidUtf8(const std::string& str);
};

// ============================================================================
// 系统工具函数（跨平台）
// ============================================================================

class SystemUtils {
public:
    // 获取系统错误信息
    static std::string GetLastErrorString();

    // 睡眠（毫秒）
    static void Sleep(uint32_t milliseconds);

    // 获取当前进程ID
    static uint64_t GetCurrentProcessId();

    // 获取当前线程ID
    static uint64_t GetCurrentThreadId();

    // 获取系统滴答计数（毫秒）
    static uint64_t GetTickCount();

    // 获取高精度时间戳（微秒）
    static uint64_t GetHighResolutionTime();

    // 获取环境变量
    static std::string GetEnvironmentVariable(const std::string& name);

    // 设置环境变量
    static bool SetEnvironmentVariable(const std::string& name, const std::string& value);
};

// ============================================================================
// 初始化/清理函数
// ============================================================================

// 平台初始化（在程序开始时调用）
bool PlatformInitialize();

// 平台清理（在程序结束时调用）
void PlatformCleanup();

} // namespace Platform

// ============================================================================
// 方便的类型别名
// ============================================================================

using PlatformSystemTime = Platform::SystemTime;
using PlatformCriticalSection = Platform::CriticalSection;
using PlatformScopedLock = Platform::ScopedLock;
using PlatformSharedMemory = Platform::SharedMemory;
using PlatformInterprocessMutex = Platform::InterprocessMutex;
using PlatformFileHandle = Platform::FileHandle;

// ============================================================================
// 平台特定函数声明（通过条件编译实现）
// ============================================================================

#ifdef TCMT_WINDOWS

// Windows特定辅助函数
namespace PlatformWindows {
    // 转换Windows SYSTEMTIME到Platform::SystemTime
    Platform::SystemTime ConvertSystemTime(const SYSTEMTIME& st);

    // 转换Platform::SystemTime到Windows SYSTEMTIME
    SYSTEMTIME ConvertToSystemTime(const Platform::SystemTime& pt);

    // 启用Windows特权
    bool EnablePrivilege(const wchar_t* privilegeName);

    // 格式化Windows错误消息
    std::string FormatWindowsErrorMessage(uint32_t errorCode);
}

#elif defined(TCMT_MACOS)

// macOS特定辅助函数
namespace PlatformMacOS {
    // 获取SMC（System Management Controller）访问
    bool OpenSMCConnection();
    void CloseSMCConnection();

    // 读取SMC键值
    bool ReadSMCKey(const char* key, uint32_t* outSize, void* outValue);

    // 获取IOKit服务
    void* GetIOKitService(const char* serviceName);

    // 释放IOKit对象
    void ReleaseIOKitObject(void* object);
}

#endif