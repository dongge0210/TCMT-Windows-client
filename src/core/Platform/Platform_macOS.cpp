// Platform_macOS.cpp - macOS平台特定实现

// 此文件仅在TCMT_MACOS定义时编译
// 通过Platform.cpp中的条件编译包含

#ifndef TCMT_MACOS
#error "This file should only be compiled for macOS platform (TCMT_MACOS defined)"
#endif

#include "Platform.h"
#include <sys/time.h>
#include <mach/mach_time.h>
#include <mach/mach_init.h>
#include <mach/thread_act.h>
#include <mach/mach_port.h>
#include <mach/mach_host.h>
#include <mach/vm_map.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <cstring>

namespace Platform {

// ============================================================================
// SystemTime实现（macOS特定）
// ============================================================================

SystemTime SystemTime::Now() {
    timeval tv;
    gettimeofday(&tv, nullptr);

    time_t rawtime = tv.tv_sec;
    struct tm* timeinfo = gmtime(&rawtime);

    SystemTime result;
    result.year = timeinfo->tm_year + 1900;
    result.month = timeinfo->tm_mon + 1;
    result.dayOfWeek = timeinfo->tm_wday;
    result.day = timeinfo->tm_mday;
    result.hour = timeinfo->tm_hour;
    result.minute = timeinfo->tm_min;
    result.second = timeinfo->tm_sec;
    result.milliseconds = tv.tv_usec / 1000;

    return result;
}

// ============================================================================
// CriticalSection实现（macOS特定）
// ============================================================================

CriticalSection::CriticalSection() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex_, &attr);
    pthread_mutexattr_destroy(&attr);
}

CriticalSection::~CriticalSection() {
    pthread_mutex_destroy(&mutex_);
}

void CriticalSection::Enter() {
    pthread_mutex_lock(&mutex_);
}

void CriticalSection::Leave() {
    pthread_mutex_unlock(&mutex_);
}

bool CriticalSection::TryEnter() {
    return pthread_mutex_trylock(&mutex_) == 0;
}

// ============================================================================
// SharedMemory实现（macOS特定）
// ============================================================================

SharedMemory::SharedMemory()
    : address_(nullptr), size_(0), created_(false), shm_fd_(-1) {
}

SharedMemory::~SharedMemory() {
    Unmap();
}

bool SharedMemory::Create(const std::string& name, size_t size) {
    Unmap();

    // POSIX shm_open names: must start with '/', max NAME_MAX (31 on macOS) chars.
    // InterprocessMutex appends "_mutex" to name internally, so base name must
    // leave headroom. Use a 20-char limit (20 + 7 + 1 = 28 < 31).
    std::string safe_name = name;
    size_t pos = safe_name.find('/');
    while (pos != std::string::npos) { safe_name.erase(pos, 1); pos = safe_name.find('/'); }
    if (safe_name.size() > 20) safe_name = safe_name.substr(0, 20);
    shm_name_ = "/" + safe_name;

    // macOS: shm_unlink does NOT allow immediate re-creation of same name in same
    // process if the kernel hasn't fully freed the object. Always unlink first.
    shm_unlink(shm_name_.c_str());

    // 创建共享内存对象
    shm_fd_ = shm_open(shm_name_.c_str(), O_CREAT | O_RDWR, 0666);
    if (shm_fd_ == -1) {
        last_error_ = "shm_open(" + shm_name_ + ") failed: " + strerror(errno);
        return false;
    }

    // 设置大小（macOS上不能shrink现有shm，所以确保先用shm_unlink清理）
    if (ftruncate(shm_fd_, static_cast<off_t>(size)) == -1) {
        last_error_ = "ftruncate(" + shm_name_ + ") failed: " + strerror(errno);
        close(shm_fd_);
        shm_fd_ = -1;
        return false;
    }

    size_ = size;
    created_ = true;
    return Map();
}

bool SharedMemory::Open(const std::string& name, size_t size) {
    Unmap();

    // Same safe name transformation as Create (must match Create exactly)
    std::string safe_name = name;
    size_t pos = safe_name.find('/');
    while (pos != std::string::npos) { safe_name.erase(pos, 1); pos = safe_name.find('/'); }
    if (safe_name.size() > 20) safe_name = safe_name.substr(0, 20);
    shm_name_ = "/" + safe_name;

    shm_fd_ = shm_open(shm_name_.c_str(), O_RDWR, 0);
    if (shm_fd_ == -1) {
        last_error_ = "shm_open(" + shm_name_ + ") failed: " + strerror(errno);
        return false;
    }

    size_ = size;
    created_ = false;
    return Map();
}

bool SharedMemory::Map() {
    if (shm_fd_ == -1) {
        last_error_ = "No shared memory file descriptor";
        return false;
    }

    address_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE,
                    MAP_SHARED, shm_fd_, 0);

    if (address_ == MAP_FAILED) {
        address_ = nullptr;
        last_error_ = "mmap failed: " + std::string(strerror(errno));
        return false;
    }

    return true;
}

bool SharedMemory::Unmap() {
    if (address_) {
        munmap(address_, size_);
        address_ = nullptr;
    }

    if (shm_fd_ != -1) {
        close(shm_fd_);
        shm_fd_ = -1;

        // 如果是创建者，删除共享内存对象
        if (created_) {
            shm_unlink(shm_name_.c_str());
        }
    }

    size_ = 0;
    created_ = false;
    shm_name_.clear();
    return true;
}

// ============================================================================
// InterprocessMutex实现（macOS特定）
// ============================================================================

InterprocessMutex::InterprocessMutex()
    : mutex_ptr_(nullptr), is_creator_(false) {
}

InterprocessMutex::~InterprocessMutex() {
    if (mutex_ptr_) {
        pthread_mutex_destroy(mutex_ptr_);
        delete mutex_ptr_;
    }
}

bool InterprocessMutex::Create(const std::string& name) {
    name_ = name;

    // 创建共享内存来存放互斥锁
    if (!shm_.Create(name + "_mutex", sizeof(pthread_mutex_t))) {
        last_error_ = "Failed to create shared memory for mutex: " + shm_.GetLastError();
        return false;
    }

    is_creator_ = true;
    mutex_ptr_ = static_cast<pthread_mutex_t*>(shm_.GetAddress());

    // 初始化进程间互斥锁属性
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    if (pthread_mutex_init(mutex_ptr_, &attr) != 0) {
        last_error_ = "Failed to initialize interprocess mutex";
        pthread_mutexattr_destroy(&attr);
        return false;
    }

    pthread_mutexattr_destroy(&attr);
    return true;
}

bool InterprocessMutex::Open(const std::string& name) {
    name_ = name;

    // 打开现有的共享内存
    if (!shm_.Open(name + "_mutex", sizeof(pthread_mutex_t))) {
        last_error_ = "Failed to open shared memory for mutex: " + shm_.GetLastError();
        return false;
    }

    is_creator_ = false;
    mutex_ptr_ = static_cast<pthread_mutex_t*>(shm_.GetAddress());
    return true;
}

bool InterprocessMutex::Lock(uint32_t timeout_ms) {
    if (!mutex_ptr_) {
        last_error_ = "Mutex not created or opened";
        return false;
    }

    // 简单实现，不支持超时（POSIX互斥锁不支持超时）
    if (pthread_mutex_lock(mutex_ptr_) != 0) {
        last_error_ = "Failed to lock mutex";
        return false;
    }

    return true;
}

bool InterprocessMutex::Unlock() {
    if (!mutex_ptr_) {
        last_error_ = "Mutex not created or opened";
        return false;
    }

    if (pthread_mutex_unlock(mutex_ptr_) != 0) {
        last_error_ = "Failed to unlock mutex";
        return false;
    }

    return true;
}

// ============================================================================
// FileHandle实现（macOS特定）
// ============================================================================

void FileHandle::Close() {
    if (handle_ != InvalidHandle()) {
        close(reinterpret_cast<intptr_t>(handle_));
        handle_ = InvalidHandle();
    }
}

void* FileHandle::InvalidHandle() {
    return reinterpret_cast<void*>(-1);
}

// ============================================================================
// StringConverter实现（macOS特定）
// ============================================================================

std::wstring StringConverter::Utf8ToWide(const std::string& utf8) {
    // macOS使用UTF-8作为原生编码，宽字符为32位
    std::wstring result;
    result.reserve(utf8.size());

    const char* ptr = utf8.c_str();
    size_t len = utf8.size();
    size_t i = 0;

    while (i < len) {
        wchar_t wc = 0;
        unsigned char c = static_cast<unsigned char>(ptr[i]);

        if ((c & 0x80) == 0) {
            // 1字节UTF-8
            wc = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            // 2字节UTF-8
            if (i + 1 < len) {
                wc = ((c & 0x1F) << 6) | (ptr[i + 1] & 0x3F);
                i += 2;
            } else {
                wc = '?';
                i += 1;
            }
        } else if ((c & 0xF0) == 0xE0) {
            // 3字节UTF-8
            if (i + 2 < len) {
                wc = ((c & 0x0F) << 12) | ((ptr[i + 1] & 0x3F) << 6) | (ptr[i + 2] & 0x3F);
                i += 3;
            } else {
                wc = '?';
                i += 1;
            }
        } else if ((c & 0xF8) == 0xF0) {
            // 4字节UTF-8（需要代理对，简化处理）
            wc = 0xFFFD; // 替换字符
            i += 4;
        } else {
            wc = '?';
            i += 1;
        }

        result.push_back(wc);
    }

    return result;
}

std::string StringConverter::WideToUtf8(const std::wstring& wide) {
    std::string result;
    result.reserve(wide.size() * 4); // 最多4字节每个字符

    for (wchar_t wc : wide) {
        if (wc <= 0x7F) {
            // 1字节UTF-8
            result.push_back(static_cast<char>(wc));
        } else if (wc <= 0x7FF) {
            // 2字节UTF-8
            result.push_back(static_cast<char>(0xC0 | (wc >> 6)));
            result.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
        } else if (wc <= 0xFFFF) {
            // 3字节UTF-8
            result.push_back(static_cast<char>(0xE0 | (wc >> 12)));
            result.push_back(static_cast<char>(0x80 | ((wc >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
        } else {
            // 4字节UTF-8（简化，假设wc <= 0x10FFFF）
            result.push_back(static_cast<char>(0xF0 | (wc >> 18)));
            result.push_back(static_cast<char>(0x80 | ((wc >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((wc >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
        }
    }

    return result;
}

std::string StringConverter::AnsiToUtf8(const std::string& ansi) {
    // macOS没有ANSI概念，假设输入已经是UTF-8
    return ansi;
}

std::string StringConverter::Utf8ToAnsi(const std::string& utf8) {
    // macOS没有ANSI概念，直接返回UTF-8
    return utf8;
}

bool StringConverter::IsValidUtf8(const std::string& str) {
    const char* ptr = str.c_str();
    size_t len = str.size();
    size_t i = 0;

    while (i < len) {
        unsigned char c = static_cast<unsigned char>(ptr[i]);

        if ((c & 0x80) == 0) {
            // 1字节UTF-8
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            // 2字节UTF-8
            if (i + 1 >= len || (ptr[i + 1] & 0xC0) != 0x80) {
                return false;
            }
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3字节UTF-8
            if (i + 2 >= len || (ptr[i + 1] & 0xC0) != 0x80 || (ptr[i + 2] & 0xC0) != 0x80) {
                return false;
            }
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            // 4字节UTF-8
            if (i + 3 >= len || (ptr[i + 1] & 0xC0) != 0x80 ||
                (ptr[i + 2] & 0xC0) != 0x80 || (ptr[i + 3] & 0xC0) != 0x80) {
                return false;
            }
            i += 4;
        } else {
            return false;
        }
    }

    return true;
}

// ============================================================================
// SystemUtils实现（macOS特定）
// ============================================================================

std::string SystemUtils::GetLastErrorString() {
    return strerror(errno);
}

uint64_t SystemUtils::GetCurrentProcessId() {
    return static_cast<uint64_t>(getpid());
}

uint64_t SystemUtils::GetCurrentThreadId() {
    uint64_t tid = 0;
    pthread_threadid_np(NULL, &tid);
    return tid;
}

// ============================================================================
// 平台初始化/清理（macOS特定）
// ============================================================================

bool PlatformInitialize() {
    // macOS不需要特殊初始化
    return true;
}

void PlatformCleanup() {
    // macOS不需要特殊清理
}

// ============================================================================
// macOS特定辅助函数
// ============================================================================

namespace PlatformMacOS {

// SMC连接句柄
static void* g_smc_connection = nullptr;

bool OpenSMCConnection() {
    // 动态加载IOKit框架
    void* iokit = dlopen("/System/Library/Frameworks/IOKit.framework/IOKit", RTLD_LAZY);
    if (!iokit) {
        return false;
    }

    // 获取函数指针（简化实现，实际需要更多工作）
    // 这里只是示例，实际SMC访问需要更多代码
    g_smc_connection = reinterpret_cast<void*>(1); // 占位符
    return true;
}

void CloseSMCConnection() {
    if (g_smc_connection) {
        g_smc_connection = nullptr;
    }
}

bool ReadSMCKey(const char* key, uint32_t* outSize, void* outValue) {
    if (!g_smc_connection || !key || !outSize || !outValue) {
        return false;
    }

    // 简化实现，实际需要调用IOKit函数
    // 这里返回假数据用于测试
    if (strcmp(key, "TC0P") == 0) {
        // CPU温度
        float temp = 45.0f; // 示例温度
        *outSize = sizeof(float);
        memcpy(outValue, &temp, sizeof(float));
        return true;
    } else if (strcmp(key, "TG0P") == 0) {
        // GPU温度
        float temp = 55.0f; // 示例温度
        *outSize = sizeof(float);
        memcpy(outValue, &temp, sizeof(float));
        return true;
    }

    return false;
}

void* GetIOKitService(const char* serviceName) {
    // 简化实现
    return reinterpret_cast<void*>(1);
}

void ReleaseIOKitObject(void* object) {
    // 简化实现，实际需要释放IOKit对象
}

} // namespace PlatformMacOS

} // namespace Platform