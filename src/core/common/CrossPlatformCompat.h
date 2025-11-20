#pragma once

// 跨平台兼容性头文件
// 解决Windows平台缺少unistd.h等问题

#ifdef PLATFORM_WINDOWS

// Windows平台下的unistd.h替代实现
#include <io.h>
#include <process.h>
#include <direct.h>
#include <sys/stat.h>
#include <sys/types.h>
// ensure mode_t is defined before use
#ifndef _MODE_T_DEFINED
typedef unsigned int mode_t;
#define _MODE_T_DEFINED
#endif

// 定义Unix风格的函数和常量
#define access _access
#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0

#define unlink _unlink
#define close _close
#define read _read
#define write _write
#define lseek _lseek
#define fsync _commit

#define getpid _getpid
#define sleep(x) Sleep((x) * 1000)
#define usleep(x) Sleep((x) / 1000)

#define S_IRUSR _S_IREAD
#define S_IWUSR _S_IWRITE
#define S_IXUSR _S_IEXEC
#define S_IRGRP 0
#define S_IWGRP 0
#define S_IXGRP 0
#define S_IROTH 0
#define S_IWOTH 0
#define S_IXOTH 0

// 文件类型定义
#ifndef S_IFMT
#define S_IFMT   _S_IFMT
#endif
#ifndef S_IFDIR
#define S_IFDIR  _S_IFDIR
#endif
#ifndef S_IFCHR
#define S_IFCHR  _S_IFCHR
#endif
#ifndef S_IFREG
#define S_IFREG  _S_IFREG
#endif

// 路径分隔符 (guard against redefinition)
#ifndef PATH_SEPARATOR
#define PATH_SEPARATOR '\\'
#endif
#ifndef PATH_SEPARATOR_STR
#define PATH_SEPARATOR_STR "\\"
#endif

// 其他Unix常量
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

// 函数声明
inline int mkdir(const char* path, mode_t mode) {
    return _mkdir(path);
}

// 线程相关
#ifndef _SSIZE_T_DEFINED
typedef SSIZE_T ssize_t;
#endif

#else
// 非Windows平台使用标准头文件
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#ifndef PATH_SEPARATOR
#define PATH_SEPARATOR '/'
#endif
#ifndef PATH_SEPARATOR_STR
#define PATH_SEPARATOR_STR "/"
#endif

#endif

// 通用函数
#ifdef __cplusplus
#include <string>
#include <vector>

// 跨平台路径处理
namespace CrossPlatform {
    // 路径分隔符
    inline char GetPathSeparator() {
        return PATH_SEPARATOR;
    }
    
    // 获取当前工作目录
    inline std::string GetCurrentWorkingDirectory() {
#ifdef PLATFORM_WINDOWS
        char buffer[MAX_PATH];
        if (_getcwd(buffer, sizeof(buffer))) {
            return std::string(buffer);
        }
#else
        char buffer[PATH_MAX];
        if (getcwd(buffer, sizeof(buffer))) {
            return std::string(buffer);
        }
#endif
        return "";
    }
    
    // 检查文件是否存在
    inline bool FileExists(const std::string& path) {
#ifdef PLATFORM_WINDOWS
        return _access(path.c_str(), F_OK) == 0;
#else
        return access(path.c_str(), F_OK) == 0;
#endif
    }
    
    // 检查文件是否可读
    inline bool FileReadable(const std::string& path) {
#ifdef PLATFORM_WINDOWS
        return _access(path.c_str(), R_OK) == 0;
#else
        return access(path.c_str(), R_OK) == 0;
#endif
    }
    
    // 创建目录
    inline bool CreateDirectory(const std::string& path) {
#ifdef PLATFORM_WINDOWS
        return _mkdir(path.c_str()) == 0;
#else
        return mkdir(path.c_str(), 0755) == 0;
#endif
    }
    
    // 删除文件
    inline bool RemoveFile(const std::string& path) {
#ifdef PLATFORM_WINDOWS
        return _unlink(path.c_str()) == 0;
#else
        return unlink(path.c_str()) == 0;
#endif
    }
    
    // 获取环境变量
    inline std::string GetEnvironmentVariable(const std::string& name) {
#ifdef PLATFORM_WINDOWS
        char buffer[32767]; // Maximum environment variable size on Windows
        DWORD size = GetEnvironmentVariableA(name.c_str(), buffer, sizeof(buffer));
        if (size > 0 && size < sizeof(buffer)) {
            return std::string(buffer);
        }
#else
        const char* value = getenv(name.c_str());
        if (value) {
            return std::string(value);
        }
#endif
        return "";
    }
    
    // 设置环境变量
    inline bool SetEnvironmentVariable(const std::string& name, const std::string& value) {
#ifdef PLATFORM_WINDOWS
        return SetEnvironmentVariableA(name.c_str(), value.c_str()) != FALSE;
#else
        return setenv(name.c_str(), value.c_str(), 1) == 0;
#endif
    }
    
    // 获取进程ID
    inline int GetProcessId() {
#ifdef PLATFORM_WINDOWS
        return _getpid();
#else
        return getpid();
#endif
    }
    
    // 睡眠函数（毫秒）
    inline void SleepMs(unsigned int milliseconds) {
#ifdef PLATFORM_WINDOWS
        Sleep(milliseconds);
#else
        usleep(milliseconds * 1000);
#endif
    }
}

#endif // __cplusplus