#include "OSInfo.h"
#include "../Utils/Logger.h"

#ifdef PLATFORM_WINDOWS
    #include "../utils/WinUtils.h"
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <winternl.h>
    #include <ntstatus.h>
    #ifndef STATUS_SUCCESS
    #define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
    #endif
    // 使用动态加载方式获取RtlGetVersion函数
    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
#elif defined(PLATFORM_MACOS)
    #include <sys/sysctl.h>
    #include <sys/utsname.h>
    #include <CoreFoundation/CoreFoundation.h>
    #include <unistd.h>
    #include <fstream>
    #include <sstream>
    #include <thread>
    #include <chrono>
    #include <ctime>
#elif defined(PLATFORM_LINUX)
    #include <sys/sysinfo.h>
    #include <sys/utsname.h>
    #include <fstream>
    #include <sstream>
    #include <unistd.h>
    #include <thread>
    #include <chrono>
    #include <ctime>
#endif

OSInfo::OSInfo() : initialized(false) {
    DetectSystemInfo();
}

void OSInfo::Initialize() {
    DetectSystemInfo();
}

void OSInfo::DetectSystemInfo() {
    try {
#ifdef PLATFORM_WINDOWS
        DetectWindowsSystemInfo();
#elif defined(PLATFORM_MACOS)
        DetectMacSystemInfo();
#elif defined(PLATFORM_LINUX)
        DetectLinuxSystemInfo();
#endif
        initialized = true;
    }
    catch (const std::exception& e) {
        Logger::Error("OSInfo detection failed: " + std::string(e.what()));
        initialized = false;
    }
}

std::string OSInfo::GetVersion() const {
    return osVersion;
}

// macOS系统信息检测实现
#ifdef PLATFORM_MACOS
void OSInfo::DetectMacSystemInfo() {
    try {
        struct utsname uts;
        uname(&uts);
        
        // 获取操作系统版本
        osVersion = std::string(uts.sysname) + " " + GetKernelVersion();
        
        // 获取主机名
        hostname = std::string(uts.nodename);
        
        // 获取架构
        architecture = GetArchitecture();
        
        // 获取系统运行时间
        GetMacSystemReport();
        
        // 获取硬件信息
        GetMacHardwareInfo();
        
        // 获取软件信息
        GetMacSoftwareInfo();
        
        // 获取安全信息
        GetMacSecurityInfo();
        
        Logger::Info("macOS system info detected: " + osVersion);
    }
    catch (const std::exception& e) {
        Logger::Error("DetectMacSystemInfo failed: " + std::string(e.what()));
    }
}

std::string OSInfo::GetKernelVersion() const {
    try {
        size_t size = 0;
        sysctlbyname("kern.osversion", nullptr, &size, nullptr, 0);
        if (size == 0) return "Unknown";
        
        std::vector<char> buffer(size);
        if (sysctlbyname("kern.osversion", buffer.data(), &size, nullptr, 0) == 0) {
            return std::string(buffer.data(), size - 1);
        }
    }
    catch (const std::exception& e) {
        Logger::Error("GetMacKernelVersion failed: " + std::string(e.what()));
    }
    return "Unknown";
}

std::string OSInfo::GetArchitecture() const {
    try {
        size_t size = 0;
        sysctlbyname("hw.machine", nullptr, &size, nullptr, 0);
        if (size == 0) return "Unknown";
        
        std::vector<char> buffer(size);
        if (sysctlbyname("hw.machine", buffer.data(), &size, nullptr, 0) == 0) {
            std::string arch(buffer.data(), size - 1);
            
            // 标准化架构名称
            if (arch == "x86_64") return "x86_64";
            if (arch == "arm64") return "arm64";
            if (arch == "i386") return "i386";
            if (arch == "arm") return "arm";
            return arch;
        }
    }
    catch (const std::exception& e) {
        Logger::Error("GetMacArchitecture failed: " + std::string(e.what()));
    }
    return "Unknown";
}

std::string OSInfo::GetMacSystemReport() {
    try {
        // 获取系统报告
        size_t size = 0;
        sysctlbyname("kern.version", nullptr, &size, nullptr, 0);
        if (size == 0) return "";
        
        std::vector<char> buffer(size);
        if (sysctlbyname("kern.version", buffer.data(), &size, nullptr, 0) == 0) {
            return std::string(buffer.data(), size - 1);
        }
    }
    catch (const std::exception& e) {
        Logger::Error("GetMacSystemReport failed: " + std::string(e.what()));
    }
    return "";
}

std::string OSInfo::GetHostname() const {
    try {
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            return std::string(hostname); // Return hostname on success
        }
        return "Unknown"; // 失败时返回 Unknown
    }
    catch (const std::exception& e) {
        Logger::Error("GetHostname failed: " + std::string(e.what()));
    }
    return "Unknown";
}

std::string OSInfo::GetSystemUptime() const {
    try {
#ifdef PLATFORM_WINDOWS
        return std::to_string(GetTickCount64() / 1000) + " seconds";
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
            return std::to_string(ts.tv_sec) + "." + std::to_string(ts.tv_nsec / 1000000) + " seconds";
        }
#endif
    }
    catch (const std::exception& e) {
        Logger::Error("GetSystemUptime failed: " + std::string(e.what()));
    }
    return "Unknown";
}

std::string OSInfo::GetBootTime() const {
    try {
        time_t bootTime = 0;
        
#ifdef PLATFORM_WINDOWS
        bootTime = GetTickCount64() / 1000 - GetTickCount64() / 1000 * 60; // 粗略的估算
#else
        struct timespec ts;
        int mib[2] = {CTL_KERN, KERN_BOOTTIME };
        size_t size = sizeof(bootTime);
        if (sysctl(mib, 2, &bootTime, &size, nullptr, 0) == 0) {
            bootTime = bootTime;
        }
#endif
        
        auto bootTimePoint = std::chrono::system_clock::from_time_t(std::time_t(bootTime));
        auto now = std::chrono::system_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - bootTimePoint);
        
        return std::to_string(uptime.count()) + " seconds ago";
    }
    catch (const std::exception& e) {
        Logger::Error("GetBootTime failed: " + std::string(e.what()));
    }
    return "Unknown";
}

std::string OSInfo::GetMacModel() const {
    try {
        // 使用system_profiler获取型号信息
        FILE* pipe = popen("system_profiler SPHardwareDataType | grep \"Model Name\"", "r");
        if (!pipe) return "Unknown";
        
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            std::string line(buffer);
            size_t pos = line.find("Model Name:");
            if (pos != std::string::npos) {
                std::string model = line.substr(pos + 11);
                // 移除前后空格
                model.erase(0, model.find_first_not_of(" \t\n\r"));
                model.erase(model.find_last_not_of(" \t\n\r") + 1);
                pclose(pipe);
                return model;
            }
        }
        pclose(pipe);
    }
    catch (const std::exception& e) {
        Logger::Error("GetMacModel failed: " + std::string(e.what()));
    }
    return "Unknown";
}

std::string OSInfo::GetMacSerialNumber() const {
    try {
        // 使用system_profiler获取序列号
        FILE* pipe = popen("system_profiler SPHardwareDataType | grep \"Serial Number\"", "r");
        if (!pipe) return "Unknown";
        
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            std::string line(buffer);
            size_t pos = line.find("Serial Number:");
            if (pos != std::string::npos) {
                std::string serial = line.substr(pos + 14);
                // 移除前后空格
                serial.erase(0, serial.find_first_not_of(" \t\n\r"));
                serial.erase(serial.find_last_not_of(" \t\n\r") + 1);
                pclose(pipe);
                return serial;
            }
        }
        pclose(pipe);
    }
    catch (const std::exception& e) {
        Logger::Error("GetMacSerialNumber failed: " + std::string(e.what()));
    }
    return "Unknown";
}

std::string OSInfo::GetMacUUID() const {
    try {
        // 获取系统UUID
        FILE* pipe = popen("system_profiler SPHardwareDataType | grep \"Platform UUID\"", "r");
        if (!pipe) return "Unknown";
        
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            std::string line(buffer);
            size_t pos = line.find("Platform UUID:");
            if (pos != std::string::npos) {
                std::string uuid = line.substr(pos + 14);
                // 移除前后空格
                uuid.erase(0, uuid.find_first_not_of(" \t\n\r"));
                uuid.erase(uuid.find_last_not_of(" \t\n\r") + 1);
                pclose(pipe);
                return uuid;
            }
        }
        pclose(pipe);
    }
    catch (const std::exception& e) {
        Logger::Error("GetMacUUID failed: " + std::string(e.what()));
    }
    return "Unknown";
}

bool OSInfo::IsMacSIPEnabled() const {
    try {
        FILE* pipe = popen("csrutil status | grep \"System Integrity Protection status: enabled\"", "r");
        if (!pipe) return false;
        
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            pclose(pipe);
            return std::string(buffer).find("enabled") != std::string::npos;
        }
    }
    catch (const std::exception& e) {
        Logger::Error("IsMacSIPEnabled failed: " + std::string(e.what()));
    }
    return false;
}

bool OSInfo::IsMacSecureBootEnabled() const {
    try {
        FILE* pipe = popen("bless -r /System/Library/CoreServices/SystemRoot.dmg/efi/Apple/AppleBootPolicy", "r");
        if (!pipe) return false;
        
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            if (strstr(buffer, "SecureBoot") != nullptr) {
                pclose(pipe);
                return true;
            }
        }
        pclose(pipe);
    }
    catch (const std::exception& e) {
        Logger::Error("IsMacSecureBootEnabled failed: " + std::string(e.what()));
    }
    return false;
}

std::string OSInfo::GetMacSystemIntegrityProtectionStatus() const {
    try {
        FILE* pipe = popen("csrutil status", "r");
        if (!pipe) return "Unknown";
        
        char buffer[1024];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe)) {
            result += buffer;
        }
        pclose(pipe);
        
        // 解析SIP状态
        if (result.find("System Integrity Protection status: enabled") != std::string::npos) {
            return "Enabled";
        } else if (result.find("System Integrity Protection status: disabled") != std::string::npos) {
            return "Disabled";
        } else if (result.find("System Integrity Protection status: unknown") != std::string::npos) {
            return "Unknown";
        }
        
        return "Not Available";
    }
    catch (const std::exception& e) {
        Logger::Error("GetMacSystemIntegrityProtectionStatus failed: " + std::string(e.what()));
    }
    return "Unknown";
}

std::string OSInfo::GetMacSecureEnclaveVersion() const {
    try {
        FILE* pipe = popen("system_profiler SPiBridgeDataType | grep \"Secure Enclave\"", "r");
        if (!pipe) return "Unknown";
        
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            std::string line(buffer);
            size_t pos = line.find("Version:");
            if (pos != std::string::npos) {
                std::string version = line.substr(pos + 8);
                version.erase(0, version.find_first_not_of(" \t\n\r"));
                version.erase(version.find_last_not_of(" \t\n\r") + 1);
                pclose(pipe);
                return version;
            }
        }
        pclose(pipe);
    }
    catch (const std::exception& e) {
        Logger::Error("GetMacSecureEnclaveVersion failed: " + std::string(e.what()));
    }
    return "Unknown";
}

void OSInfo::GetMacHardwareInfo() {
    try {
        // 获取硬件信息
        FILE* pipe = popen("system_profiler SPHardwareDataType", "r");
        if (!pipe) return;
        
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            // 解析硬件信息
        }
        pclose(pipe);
    }
    catch (const std::exception& e) {
        Logger::Error("GetMacHardwareInfo failed: " + std::string(e.what()));
    }
}

void OSInfo::GetMacSoftwareInfo() {
    try {
        // 获取软件信息
        FILE* pipe = popen("system_profiler SPSoftwareDataType", "r");
        if (!pipe) return;
        
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            // 解析软件信息
        }
        pclose(pipe);
    }
    catch (const std::exception& e) {
        Logger::Error("GetMacSoftwareInfo failed: " + std::string(e.what()));
    }
}

void OSInfo::GetMacSecurityInfo() {
    try {
        // 检查安全设置
        FILE* pipe = popen("system_profiler SPSecurityDataType", "r");
        if (!pipe) return;
        
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            // 解析安全信息
        }
        pclose(pipe);
    }
    catch (const std::exception& e) {
        Logger::Error("GetMacSecurityInfo failed: " + std::string(e.what()));
    }
}
#endif

// Linux系统信息检测实现
#ifdef PLATFORM_LINUX
void OSInfo::DetectLinuxSystemInfo() {
    try {
        struct utsname uts;
        uname(&uts);
        
        // 获取操作系统版本
        osVersion = std::string(uts.sysname) + " " + GetLinuxKernelVersion();
        
        // 获取主机名
        hostname = std::string(uts.nodename);
        
        // 获取架构
        architecture = GetLinuxArchitecture();
        
        // 获取系统运行时间
        systemUptime = GetSystemUptime();
        
        // 获取启动时间
        bootTime = GetBootTime();
        
        // 获取硬件信息
        GetLinuxHardwareInfo();
        
        // 获取软件信息
        GetLinuxSoftwareInfo();
        
        // 获取安全信息
        GetLinuxSecurityInfo();
        
        Logger::Info("Linux system info detected: " + osVersion);
    }
    catch (const std::exception& e) {
        Logger::Error("DetectLinuxSystemInfo failed: " + std::string(e.what()));
    }
}

std::string OSInfo::GetLinuxKernelVersion() const {
    try {
        std::ifstream file("/proc/version");
        if (!file.is_open()) return "Unknown";
        
        std::string line;
        if (std::getline(file, line)) {
            file.close();
            // 解析版本行格式: "Linux version 5.15.0-generic"
            size_t pos = line.find(" ");
            if (pos != std::string::npos) {
                return line.substr(pos + 1);
            }
            return line;
        }
    }
    catch (const std::exception& e) {
        Logger::Error("GetLinuxKernelVersion failed: " + std::string(e.what()));
    }
    return "Unknown";
}

std::string OSInfo::GetLinuxArchitecture() const {
    try {
        std::ifstream file("/proc/cpuinfo");
        if (!file.is_open()) return "Unknown";
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("model name") != std::string::npos) {
                file.close();
                size_t pos = line.find(":");
                if (pos != std::string::npos) {
                    std::string arch = line.substr(pos + 2);
                    // 移除前后空格
                    arch.erase(0, arch.find_first_not_of(" \t "));
                    arch.erase(arch.find_last_not_of(" \t\n\r") + 1); // Trim trailing whitespace
                    // 标准化架构名称
                    if (arch == "x86_64") return "x86_64";
                    if (arch == "aarch64") return "aarch64";
                    if (arch == "i686") return "i686";
                    return arch;
                }
            }
        }
        file.close();
    }
    catch (const std::exception& e) {
        Logger::Error("GetLinuxArchitecture failed: " + std::string(e.what()));
    }
    return "Unknown";
}

std::string OSInfo::GetLinuxDistribution() const {
    try {
        std::ifstream file("/etc/os-release");
        if (!file.is_open()) return "Unknown";
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("PRETTY_NAME=") != std::string::npos) {
                file.close();
                size_t start = line.find("PRETTY_NAME=\"");
                if (start != std::string::npos) {
                    start += 13; // 跳过 PRETTY_NAME="
                    size_t end = line.find("\"", start);
                    if (end != std::string::npos) {
                        std::string distro = line.substr(start, end - start);
                        return distro;
                    }
                }
            }
        }
        file.close();
    }
    catch (const std::exception& e) {
        Logger::Error("GetLinuxDistribution failed: " + std::string(e.what()));
    }
    return "Unknown";
}

std::string OSInfo::GetLinuxDesktopEnvironment() const {
    try {
        const char* desktop = getenv("DESKTOP_SESSION");
        if (desktop) {
            return std::string(desktop);
        }
        
        // 检查常见的桌面环境
        if (system("which gnome-session", nullptr) == 0) return "GNOME";
        if (system("which kde-session", nullptr) == 0) return "KDE";
        if (system("which xfce4-session", nullptr) == 0) return "XFCE";
        
        return "Unknown";
    }
    catch (const std::exception& e) {
        Logger::Error("GetLinuxDesktopEnvironment failed: " + std::string(e.what()));
    }
    return "Unknown";
}

std::string OSInfo::GetLinuxPackageManager() const {
    try {
        if (system("which apt", nullptr) == 0) return "APT";
        if (system("which yum", nullptr) == 0) return "YUM";
        if (system("which dnf", nullptr) == 0) return "DNF";
        if (system("which pacman", nullptr) == 0) return "PACMAN";
        return "Unknown";
    }
    catch (const std::exception& e) {
        Logger::Error("GetLinuxPackageManager failed: " + std::string(e.what()));
    }
    return "Unknown";
}

void OSInfo::GetLinuxHardwareInfo() {
    try {
        // 获取CPU信息
        std::ifstream cpuinfo("/proc/cpuinfo");
        if (cpuinfo.is_open()) {
            cpuinfo.close();
        }
        
        // 获取内存信息
        std::ifstream meminfo("/proc/meminfo");
        if (meminfo.is_open()) {
            meminfo.close();
        }
        
        // 获取磁盘信息
        std::ifstream disks("/proc/diskstats");
        if (disks.is_open()) {
            disks.close();
        }
    }
    catch (const std::exception& e) {
        Logger::Error("GetLinuxHardwareInfo failed: " + std::string(e.what()));
    }
}

void OSInfo::GetLinuxSoftwareInfo() {
    try {
        // 获取已安装的软件包信息
        std::ifstream packages("/var/lib/dpkg/status", std::ifstream::in);
        if (packages.is_open()) {
            packages.close();
        }
        
        // 获取系统服务状态
        std::ifstream services("/proc/sys/fs/file", std::ifstream::in);
        if (services.is_open()) {
            services.close();
        }
    }
    catch (const std::exception& e) {
        Logger::Error("GetLinuxSoftwareInfo failed: " + std::string(e.what()));
    }
}

void OSInfo::GetLinuxSecurityInfo() {
    try {
        // 检查SELinux状态
        std::ifstream selinux_config("/etc/selinux/config");
        if (selinux_config.is_open()) {
            selinux_config.close();
        }
        
        // 检查防火墙状态
        std::ifstream firewall("sudo ufw status", std::ifstream::in);
        if (firewall.is_open()) {
            firewall.close();
        }
    }
    catch (const std::exception& e) {
        Logger::Error("GetLinuxSecurityInfo failed: " + std::string(e.what()));
    }
}
#endif

// Windows实现
#ifdef PLATFORM_WINDOWS
void OSInfo::DetectWindowsSystemInfo() {
    try {
        RTL_OSVERSIONINFOW osvi = { sizeof(osvi) };
        
        // 动态获取RtlGetVersion函数指针
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (ntdll) {
            RtlGetVersionPtr RtlGetVersion = (RtlGetVersionPtr)GetProcAddress(ntdll, "RtlGetVersion");
            if (RtlGetVersion && RtlGetVersion(&osvi) == STATUS_SUCCESS) {
                osVersion = WinUtils::WstringToString(
                    std::wstring(L"Windows ") + 
                    std::to_wstring(osvi.dwMajorVersion) + L"." + 
                    std::to_wstring(osvi.dwMinorVersion) + 
                    L" (Build " + std::to_wstring(osvi.dwBuildNumber) + L")"
                );
                majorVersion = osvi.dwMajorVersion;
                minorVersion = osvi.dwMinorVersion;
                buildNumber = osvi.dwBuildNumber;
            } else {
                osVersion = "Unknown OS Version";
            }
        } else {
            osVersion = "Unknown OS Version";
        }
        
        // 获取主机名
        char hostName[256];
        if (gethostname(hostName, sizeof(hostName)) == 0) {
            this->hostname = std::string(hostName);
        }
        
        // 获取架构
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        switch (si.wProcessorArchitecture) {
            case PROCESSOR_ARCHITECTURE_AMD64:
                architecture = "x86_64";
                break;
            case PROCESSOR_ARCHITECTURE_ARM64:
                architecture = "arm64";
                break;
            case PROCESSOR_ARCHITECTURE_INTEL:
                architecture = "x86";
                break;
            default:
                architecture = "Unknown";
        }
        
        // 获取系统运行时间
        systemUptime = std::to_string(GetTickCount64() / 1000) + " seconds";
        
        // 获取启动时间
        bootTime = GetBootTime();
        
        Logger::Info("Windows system info detected: " + osVersion);
    }
    catch (const std::exception& e) {
        Logger::Error("DetectWindowsSystemInfo failed: " + std::string(e.what()));
    }
}
#endif



