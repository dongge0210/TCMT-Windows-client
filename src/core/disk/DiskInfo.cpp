// DiskInfo.cpp
#include "DiskInfo.h"
#include "../Utils/Logger.h"

#ifdef PLATFORM_WINDOWS
    #include "../Utils/WinUtils.h"
    #include "../Utils/WmiManager.h"
    #include <wbemidl.h>
    #include <comdef.h>
    #include <shellapi.h>
    #include <algorithm>
    #pragma comment(lib, "wbemuuid.lib")
    #pragma comment(lib, "Shell32.lib")
#elif defined(PLATFORM_MACOS)
    #include <sys/sysctl.h>
    #include <sys/statvfs.h>
    #include <sys/mount.h>
    #include <sys/mount.h>
    #include <diskarbitration/diskarbitration.h>
    #include <IOKit/IOKitLib.h>
    #include <IOKit/storage/IOBlockStorageDevice.h>
    #include <CoreFoundation/CoreFoundation.h>
    #include <unistd.h>
    #include <fstream>
    #include <sstream>
    #include <algorithm>
#elif defined(PLATFORM_LINUX)
    #include <sys/statvfs.h>
    #include <fstream>
    #include <sstream>
    #include <algorithm>
    #include <glob.h>
    #include <dirent.h>
    #include <unistd.h>
#endif

DiskInfo::DiskInfo() : initialized(false) { 
    Logger::Debug("DiskInfo: 初始化磁盘信息");
    QueryDrives(); 
}

DiskInfo::~DiskInfo() {
    Logger::Debug("DiskInfo: 清理磁盘信息");
}

void DiskInfo::QueryDrives() {
    Logger::Debug("DiskInfo: 开始查询驱动器信息");
    drives.clear();
    
    try {
#ifdef PLATFORM_WINDOWS
        QueryWindowsDrives();
#elif defined(PLATFORM_MACOS)
        QueryMacDrives();
#elif defined(PLATFORM_LINUX)
        QueryLinuxDrives();
#endif
        initialized = true;
    }
    catch (const std::exception& e) {
        Logger::Error("DiskInfo查询失败: " + std::string(e.what()));
        initialized = false;
    }
}

// macOS磁盘查询实现
#ifdef PLATFORM_MACOS
void DiskInfo::QueryMacDrives() {
    Logger::Debug("DiskInfo: 查询macOS磁盘信息");
    
    struct statfs *mounts;
    int count = getmntinfo(&mounts, MNT_WAIT);
    if (count == 0) {
        Logger::Error("获取挂载点信息失败");
        return;
    }
    
    for (int i = 0; i < count; i++) {
        DriveInfo info;
        info.mountPoint = mounts[i].f_mntonname;
        info.devicePath = mounts[i].f_mntfromname;
        info.letter = 0; // macOS不使用驱动器字母
        
        // 过滤掉不需要的挂载点
        if (info.mountPoint.empty() || 
            info.mountPoint == "/dev" || 
            info.mountPoint.find("/net/") == 0 ||
            info.mountPoint.find("/home/") == 0) {
            continue;
        }
        
        // 获取磁盘空间信息
        struct statvfs fs;
        if (statvfs(info.mountPoint.c_str(), &fs) == 0) {
            info.totalSize = fs.f_blocks * fs.f_frsize;
            info.freeSpace = fs.f_bavail * fs.f_frsize;
            info.usedSpace = info.totalSize - info.freeSpace;
        }
        
        // 获取文件系统类型
        info.fileSystem = GetMacFileSystem(info.mountPoint);
        
        // 获取卷标
        info.label = GetMacDiskLabel(info.mountPoint);
        
        // 检查是否为可移动设备
        info.isRemovable = (info.mountPoint == "/Volumes" || 
                           info.devicePath.find("/dev/disk") == 0);
        
        // 获取磁盘属性
        GetMacDiskProperties(info.devicePath, info);
        
        drives.push_back(info);
        Logger::Debug("DiskInfo: 添加macOS磁盘 - 挂载点: " + info.mountPoint + 
                     ", 设备: " + info.devicePath + 
                     ", 文件系统: " + info.fileSystem);
    }
    
    Logger::Debug("DiskInfo: macOS磁盘查询完成，共找到 " + std::to_string(drives.size()) + " 个磁盘");
}

void DiskInfo::GetMacDiskProperties(const std::string& devicePath, DriveInfo& info) const {
    try {
        // 检查是否为SSD
        info.isSSD = IsMacSSD(devicePath);
        
        // 确定接口类型
        if (devicePath.find("disk1") != std::string::npos) {
            info.interfaceType = "Internal";
        } else if (devicePath.find("disk2") != std::string::npos) {
            info.interfaceType = "External";
        } else {
            info.interfaceType = "Unknown";
        }
        
        // 检查APFS和加密状态
        info.isAPFS = (info.fileSystem == "apfs");
        info.isEncrypted = false; // 需要进一步检查
        
        // 获取性能信息
        GetMacDiskPerformance(devicePath, info);
    }
    catch (const std::exception& e) {
        Logger::Error("获取macOS磁盘属性失败: " + std::string(e.what()));
    }
}

bool DiskInfo::IsMacSSD(const std::string& devicePath) const {
    try {
        // 通过IOKit检查设备类型
        std::string command = "diskutil info " + devicePath + " | grep 'Solid State'";
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) return false;
        
        char buffer[128];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
        
        return result.find("Solid State") != std::string::npos;
    }
    catch (const std::exception& e) {
        Logger::Error("检查SSD状态失败: " + std::string(e.what()));
        return false;
    }
}

std::string DiskInfo::GetMacFileSystem(const std::string& mountPoint) const {
    struct statfs fs;
    if (statfs(mountPoint.c_str(), &fs) == 0) {
        if (strcmp(fs.f_fstypename, "apfs") == 0) return "apfs";
        if (strcmp(fs.f_fstypename, "hfs") == 0) return "hfs";
        if (strcmp(fs.f_fstypename, "exfat") == 0) return "exfat";
        if (strcmp(fs.f_fstypename, "ntfs") == 0) return "ntfs";
        if (strcmp(fs.f_fstypename, "ext4") == 0) return "ext4";
        return fs.f_fstypename;
    }
    return "Unknown";
}

std::string DiskInfo::GetMacDiskLabel(const std::string& mountPoint) const {
    try {
        std::string command = "diskutil info " + mountPoint + " | grep 'Volume Name'";
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) return "";
        
        char buffer[256];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
        
        // 解析卷标
        size_t pos = result.find(":");
        if (pos != std::string::npos) {
            std::string label = result.substr(pos + 1);
            // 移除前后空格
            label.erase(0, label.find_first_not_of(" \t\n\r"));
            label.erase(label.find_last_not_of(" \t\n\r") + 1);
            return label;
        }
    }
    catch (const std::exception& e) {
        Logger::Error("获取macOS磁盘卷标失败: " + std::string(e.what()));
    }
    
    return "";
}

void DiskInfo::GetMacDiskPerformance(const std::string& devicePath, DriveInfo& info) const {
    try {
        // 这里可以添加性能监控逻辑
        // 暂时留空，后续可以添加IO统计
    }
    catch (const std::exception& e) {
        Logger::Error("获取macOS磁盘性能信息失败: " + std::string(e.what()));
    }
}
#endif

// Windows实现（保持原有逻辑）
#ifdef PLATFORM_WINDOWS
void DiskInfo::QueryWindowsDrives() {
    Logger::Debug("DiskInfo: 查询Windows磁盘信息");
    
    // 获取逻辑驱动器信息
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; i++) {
        if (drives & (1 << i)) {
            char driveLetter = 'A' + i;
            std::string rootPath = std::string(1, driveLetter) + ":\\";
            
            DriveInfo info;
            info.letter = driveLetter;
            info.mountPoint = rootPath;
            info.devicePath = "\\\\.\\\\PhysicalDrive" + std::to_string(i);
            
            // 获取磁盘空间
            ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
            if (GetDiskFreeSpaceExA(rootPath.c_str(), &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
                info.totalSize = totalBytes.QuadPart;
                info.freeSpace = freeBytesAvailable.QuadPart;
                info.usedSpace = info.totalSize - info.freeSpace;
            }
            
            // 获取卷标和文件系统
            char volumeName[MAX_PATH + 1] = {0};
            char fileSystemName[MAX_PATH + 1] = {0};
            DWORD volumeSerialNumber = 0;
            DWORD maximumComponentLength = 0;
            DWORD fileSystemFlags = 0;
            
            if (GetVolumeInformationA(rootPath.c_str(), volumeName, MAX_PATH + 1,
                                     &volumeSerialNumber, &maximumComponentLength,
                                     &fileSystemFlags, fileSystemName, MAX_PATH + 1)) {
                info.label = volumeName;
                info.fileSystem = fileSystemName;
            }
            
            // 检查是否为可移动设备
            UINT driveType = GetDriveTypeA(rootPath.c_str());
            info.isRemovable = (driveType == DRIVE_REMOVABLE || driveType == DRIVE_CDROM);
            
            // 检查是否为SSD（简化实现）
            info.isSSD = false;
            info.interfaceType = "Unknown";
            
            drives.push_back(info);
            Logger::Debug("DiskInfo: 添加Windows磁盘 - 驱动器 " + std::string(1, driveLetter) + ": " + info.label);
        }
    }
    
    Logger::Debug("DiskInfo: Windows磁盘查询完成，共找到 " + std::to_string(drives.size()) + " 个磁盘");
}
#endif

// Linux磁盘查询实现
#ifdef PLATFORM_LINUX
void DiskInfo::QueryLinuxDrives() {
    Logger::Debug("DiskInfo: 查询Linux磁盘信息");
    
    // 读取 /proc/mounts 获取挂载点信息
    std::ifstream mounts("/proc/mounts");
    if (!mounts.is_open()) {
        Logger::Error("无法打开 /proc/mounts");
        return;
    }
    
    std::string line;
    while (std::getline(mounts, line)) {
        std::istringstream iss(line);
        std::string device, mountPoint, fsType, options;
        int dump, pass;
        
        if (!(iss >> device >> mountPoint >> fsType >> options >> dump >> pass)) {
            continue;
        }
        
        // 过滤掉不需要的挂载点
        if (mountPoint.empty() || 
            mountPoint == "/proc" || 
            mountPoint == "/sys" || 
            mountPoint == "/dev" ||
            mountPoint.find("/proc/") == 0 ||
            mountPoint.find("/sys/") == 0) {
            continue;
        }
        
        DriveInfo info;
        info.devicePath = device;
        info.mountPoint = mountPoint;
        info.fileSystem = fsType;
        info.letter = 0; // Linux不使用驱动器字母
        
        // 获取磁盘空间信息
        struct statvfs fs;
        if (statvfs(mountPoint.c_str(), &fs) == 0) {
            info.totalSize = fs.f_blocks * fs.f_frsize;
            info.freeSpace = fs.f_bavail * fs.f_frsize;
            info.usedSpace = info.totalSize - info.freeSpace;
        }
        
        // 获取卷标
        info.label = GetLinuxDiskLabel(device);
        
        // 检查是否为可移动设备
        info.isRemovable = (device.find("/dev/sd") == 0 || device.find("/dev/nvme") == 0);
        
        // 检查是否为SSD
        info.isSSD = IsLinuxSSD(device);
        
        // 确定接口类型
        if (device.find("/dev/nvme") == 0) {
            info.interfaceType = "NVMe";
        } else if (device.find("/dev/sd") == 0) {
            info.interfaceType = "SATA";
        } else if (device.find("/dev/hd") == 0) {
            info.interfaceType = "IDE";
        } else {
            info.interfaceType = "Unknown";
        }
        
        drives.push_back(info);
        Logger::Debug("DiskInfo: 添加Linux磁盘 - 挂载点: " + info.mountPoint + 
                     ", 设备: " + info.devicePath + 
                     ", 文件系统: " + info.fileSystem);
    }
    
    mounts.close();
    Logger::Debug("DiskInfo: Linux磁盘查询完成，共找到 " + std::to_string(drives.size()) + " 个磁盘");
}

bool DiskInfo::IsLinuxSSD(const std::string& devicePath) const {
    try {
        // 通过 /sys/block 检查设备类型
        std::string deviceName = devicePath;
        size_t pos = deviceName.find_last_of('/');
        if (pos != std::string::npos) {
            deviceName = deviceName.substr(pos + 1);
        }
        
        // 处理分区（如 sda1 -> sda）
        if (deviceName.length() > 0 && std::isdigit(deviceName.back())) {
            while (!deviceName.empty() && std::isdigit(deviceName.back())) {
                deviceName.pop_back();
            }
        }
        
        std::string queuePath = "/sys/block/" + deviceName + "/queue/rotational";
        std::ifstream file(queuePath);
        if (file.is_open()) {
            int rotational;
            file >> rotational;
            file.close();
            return rotational == 0; // 0 表示SSD，1 表示HDD
        }
    }
    catch (const std::exception& e) {
        Logger::Error("检查Linux SSD状态失败: " + std::string(e.what()));
    }
    
    return false;
}

std::string DiskInfo::GetLinuxFileSystem(const std::string& mountPoint) const {
    struct statfs fs;
    if (statfs(mountPoint.c_str(), &fs) == 0) {
        // 转换文件系统类型
        switch (fs.f_type) {
            case 0xEF53: return "ext4";
            case 0xEF51: return "ext3";
            case 0xEF52: return "ext2";
            case 0x01021994: return "tmpfs";
            case 0x9fa0: return "proc";
            case 0x62656572: return "btrfs";
            case 0x58465342: return "xfs";
            case 0x65735546: return "fuse";
            case 0x65735543: return "cifs";
            case 0x517B: return "reiserfs";
            case 0x012FF7B7: return "coh";
            case 0x199C: return "udf";
            case 0x137D: return "devpts";
            case 0x9fa2: return "nfs";
            case 0x5346544E: return "ntfs";
            case 0x4d44: return "msdos";
            case 0x4000: return "vfat";
            case 0x4244: return "hfs";
            case 0x482b: return "hfsplus";
            case 0x65735541: return "aufs";
            case 0x09041934: return "pstorefs";
            case 0x73636673: return "securityfs";
            case 0x6573556d: return "mqueue";
            case 0x65735574: return "tmpfs";
            case 0x65735563: return "cgroup";
            case 0x61676673: return "autofs";
            case 0x696e6f73: return "inodefs";
            case 0x74646174: return "data";
            case 0x62666973: return "bpf";
            case 0xcafe4a11: return "smackfs";
            case 0x53464846: return "hostfs";
            case 0x65735564: return "dax";
            case 0x43414d53: return "ecryptfs";
            case 0xf97cff8c: return "overlayfs";
            case 0x6573554f: return "fuseblk";
            case 0x65735570: return "pipefs";
            case 0x65735563: return "cgroup2";
            case 0x73616368: return "cachefiles";
            case 0x65735573: return "sockfs";
            case 0x65735575: return "userfaultfd";
            case 0x65735572: return "ramfs";
            case 0x65735568: return "hugetlbfs";
            case 0x6573556d: return "mqueue";
            case 0x65735564: return "dax";
            case 0x65735563: return "cgroup";
            case 0x65735574: return "tmpfs";
            case 0x6573556c: return "lxcfs";
            case 0x65735570: return "pstore";
            case 0x6573556d: return "mqueue";
            case 0x65735573: return "sockfs";
            case 0x65735566: return "function";
            default: return "Unknown";
        }
    }
    return "Unknown";
}

std::string DiskInfo::GetLinuxDiskLabel(const std::string& devicePath) const {
    try {
        // 尝试从 /dev/disk/by-label 获取卷标
        std::string deviceName = devicePath;
        size_t pos = deviceName.find_last_of('/');
        if (pos != std::string::npos) {
            deviceName = deviceName.substr(pos + 1);
        }
        
        std::string labelPath = "/dev/disk/by-label";
        if (access(labelPath.c_str(), F_OK) == 0) {
            DIR* dir = opendir(labelPath.c_str());
            if (dir) {
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr) {
                    if (entry->d_name[0] != '.') {
                        char target[PATH_MAX];
                        std::string fullPath = labelPath + "/" + entry->d_name;
                        ssize_t len = readlink(fullPath.c_str(), target, sizeof(target) - 1);
                        if (len != -1) {
                            target[len] = '\0';
                            std::string targetPath(target);
                            pos = targetPath.find_last_of('/');
                            if (pos != std::string::npos) {
                                std::string targetName = targetPath.substr(pos + 1);
                                if (targetName == deviceName) {
                                    closedir(dir);
                                    return std::string(entry->d_name);
                                }
                            }
                        }
                    }
                }
                closedir(dir);
            }
        }
    }
    catch (const std::exception& e) {
        Logger::Error("获取Linux磁盘卷标失败: " + std::string(e.what()));
    }
    
    return "";
}
#endif

void DiskInfo::Refresh() { 
    Logger::Debug("DiskInfo: 刷新磁盘信息");
    QueryDrives(); 
}
const std::vector<DriveInfo>& DiskInfo::GetDrives() const { return drives; }

std::vector<DiskData> DiskInfo::GetDisks() {
    Logger::Debug("DiskInfo: 获取磁盘数据，共 " + std::to_string(drives.size()) + " 个驱动器");
    std::vector<DiskData> disks; disks.reserve(drives.size());
    for (const auto& drive : drives) { 
        DiskData d; 
#ifdef PLATFORM_WINDOWS
        d.letter = drive.letter; 
        d.label = drive.label; 
        d.fileSystem = drive.fileSystem; 
#else
        // macOS/Linux使用挂载点作为标识
        d.letter = 0; // 不使用驱动器字母
        d.label = drive.label;
        d.fileSystem = drive.fileSystem;
        d.mountPoint = drive.mountPoint;
        d.devicePath = drive.devicePath;
#endif
        d.totalSize = drive.totalSize; 
        d.freeSpace = drive.freeSpace; 
        d.usedSpace = drive.usedSpace; 
        disks.push_back(std::move(d)); 
        
#ifdef PLATFORM_WINDOWS
        Logger::Debug("DiskInfo: 添加磁盘数据 - 驱动器 " + std::string(1, d.letter) + ": " + d.label);
#else
        Logger::Debug("DiskInfo: 添加磁盘数据 - 挂载点 " + d.mountPoint + ": " + d.label);
#endif
    }
    Logger::Debug("DiskInfo: 磁盘数据获取完成，共 " + std::to_string(disks.size()) + " 个磁盘");
    return disks;
}

// ---------------- 物理磁盘 + 逻辑盘符映射实现合并 ----------------
static bool ParseDiskPartition(const std::wstring& text, int& diskIndexOut) {
    size_t posDisk = text.find(L"Disk #"); if (posDisk==std::wstring::npos) return false; posDisk += 6; if (posDisk>=text.size()) return false; int num=0; bool any=false; while (posDisk<text.size() && iswdigit(text[posDisk])) { any=true; num = num*10 + (text[posDisk]-L'0'); ++posDisk; } if(!any) return false; diskIndexOut = num; return true; }

void DiskInfo::CollectPhysicalDisks(WmiManager& wmi, const std::vector<DiskData>& logicalDisks, SystemInfo& sysInfo) {
    Logger::Debug("DiskInfo: 开始收集物理磁盘信息");
    IWbemServices* svc = wmi.GetWmiService(); if (!svc) { Logger::Warn("WMI 服务无效，跳过物理磁盘枚举"); return; }
    std::map<int,std::vector<char>> physicalIndexToLetters;
    // 1. Win32_DiskDriveToDiskPartition
    {
        Logger::Debug("DiskInfo: 查询 Win32_DiskDriveToDiskPartition");
        IEnumWbemClassObject* pEnum=nullptr; HRESULT hr=svc->ExecQuery(bstr_t(L"WQL"), bstr_t(L"SELECT * FROM Win32_DiskDriveToDiskPartition"), WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,&pEnum);
        if (SUCCEEDED(hr)&&pEnum){ IWbemClassObject* obj=nullptr; ULONG ret=0; int mappingCount = 0; while(pEnum->Next(WBEM_INFINITE,1,&obj,&ret)==S_OK){ VARIANT ant; VARIANT dep; VariantInit(&ant); VariantInit(&dep); if (SUCCEEDED(obj->Get(L"Antecedent",0,&ant,0,0)) && ant.vt==VT_BSTR && SUCCEEDED(obj->Get(L"Dependent",0,&dep,0,0)) && dep.vt==VT_BSTR){ int diskIdx=-1; if (ParseDiskPartition(dep.bstrVal,diskIdx)){ /* mapping captured via next query */ mappingCount++; } } VariantClear(&ant); VariantClear(&dep); obj->Release(); } pEnum->Release(); Logger::Debug("DiskInfo: Win32_DiskDriveToDiskPartition 查询完成，找到 " + std::to_string(mappingCount) + " 个映射"); } else { Logger::Warn("查询 Win32_DiskDriveToDiskPartition 失败"); }
    }
    // 2. Win32_LogicalDiskToPartition 建立盘符 -> 物理索引
    std::map<char,int> letterToDiskIndex; {
        Logger::Debug("DiskInfo: 查询 Win32_LogicalDiskToPartition");
        IEnumWbemClassObject* pEnum=nullptr; HRESULT hr=svc->ExecQuery(bstr_t(L"WQL"), bstr_t(L"SELECT * FROM Win32_LogicalDiskToPartition"), WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,&pEnum);
        if (SUCCEEDED(hr)&&pEnum){ IWbemClassObject* obj=nullptr; ULONG ret=0; int mappingCount = 0; while(pEnum->Next(WBEM_INFINITE,1,&obj,&ret)==S_OK){ VARIANT ant; VARIANT dep; VariantInit(&ant); VariantInit(&dep); if (SUCCEEDED(obj->Get(L"Antecedent",0,&ant,0,0)) && ant.vt==VT_BSTR && SUCCEEDED(obj->Get(L"Dependent",0,&dep,0,0)) && dep.vt==VT_BSTR){ int diskIdx=-1; if (ParseDiskPartition(ant.bstrVal,diskIdx)){ std::wstring depStr = dep.bstrVal; size_t pos = depStr.find(L"DeviceID=\""); if (pos!=std::wstring::npos){ pos+=10; if (pos<depStr.size()){ wchar_t letterW = depStr[pos]; if(letterW && letterW!=L'"'){ letterToDiskIndex[ static_cast<char>(::toupper(letterW)) ] = diskIdx; mappingCount++; } } } } } VariantClear(&ant); VariantClear(&dep); obj->Release(); } pEnum->Release(); Logger::Debug("DiskInfo: Win32_LogicalDiskToPartition 查询完成，找到 " + std::to_string(mappingCount) + " 个映射"); } else { Logger::Warn("查询 Win32_LogicalDiskToPartition 失败"); } }
    for (auto& kv: letterToDiskIndex) physicalIndexToLetters[kv.second].push_back(kv.first);
    Logger::Debug("DiskInfo: 盘符到物理磁盘索引映射完成，共 " + std::to_string(letterToDiskIndex.size()) + " 个映射");
    // 3. Win32_DiskDrive 基本信息
    std::map<int,PhysicalDiskSmartData> tempDisks; {
        Logger::Debug("DiskInfo: 查询 Win32_DiskDrive");
        IEnumWbemClassObject* pEnum=nullptr; HRESULT hr=svc->ExecQuery(bstr_t(L"WQL"), bstr_t(L"SELECT Index,Model,SerialNumber,InterfaceType,Size,MediaType FROM Win32_DiskDrive"), WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,&pEnum);
        if (SUCCEEDED(hr)&&pEnum){ IWbemClassObject* obj=nullptr; ULONG ret=0; int diskCount = 0; while(pEnum->Next(WBEM_INFINITE,1,&obj,&ret)==S_OK){ VARIANT vIndex,vModel,vSerial,vIface,vSize,vMedia; VariantInit(&vIndex);VariantInit(&vModel);VariantInit(&vSerial);VariantInit(&vIface);VariantInit(&vSize);VariantInit(&vMedia); if (SUCCEEDED(obj->Get(L"Index",0,&vIndex,0,0)) && (vIndex.vt==VT_I4||vIndex.vt==VT_UI4)){ int idx=(vIndex.vt==VT_I4)?vIndex.intVal:static_cast<int>(vIndex.uintVal); PhysicalDiskSmartData data{}; if (SUCCEEDED(obj->Get(L"Model",0,&vModel,0,0))&&vModel.vt==VT_BSTR) wcsncpy_s(data.model,vModel.bstrVal,_TRUNCATE); if (SUCCEEDED(obj->Get(L"SerialNumber",0,&vSerial,0,0))&&vSerial.vt==VT_BSTR) wcsncpy_s(data.serialNumber,vSerial.bstrVal,_TRUNCATE); if (SUCCEEDED(obj->Get(L"InterfaceType",0,&vIface,0,0))&&vIface.vt==VT_BSTR) wcsncpy_s(data.interfaceType,vIface.bstrVal,_TRUNCATE); if (SUCCEEDED(obj->Get(L"Size",0,&vSize,0,0))){ if (vSize.vt==VT_UI8) data.capacity=vSize.ullVal; else if (vSize.vt==VT_BSTR) data.capacity=_wcstoui64(vSize.bstrVal,nullptr,10);} if (SUCCEEDED(obj->Get(L"MediaType",0,&vMedia,0,0))&&vMedia.vt==VT_BSTR){ std::wstring media=vMedia.bstrVal; if (media.find(L"SSD")!=std::wstring::npos||media.find(L"Solid State")!=std::wstring::npos) wcsncpy_s(data.diskType,L"SSD",_TRUNCATE); else wcsncpy_s(data.diskType,L"HDD",_TRUNCATE);} else wcsncpy_s(data.diskType,L"未知",_TRUNCATE); data.smartSupported=false; data.smartEnabled=false; data.healthPercentage=0; data.temperature=0.0; data.logicalDriveCount=0; tempDisks[idx]=data; diskCount++; Logger::Debug(L"DiskInfo: 添加物理磁盘 - 索引: " + std::to_wstring(idx) + L", 型号: " + data.model); } VariantClear(&vIndex);VariantClear(&vModel);VariantClear(&vSerial);VariantClear(&vIface);VariantClear(&vSize);VariantClear(&vMedia); obj->Release(); } pEnum->Release(); Logger::Debug("DiskInfo: Win32_DiskDrive 查询完成，找到 " + std::to_string(diskCount) + " 个物理磁盘"); } else { Logger::Warn("查询 Win32_DiskDrive 失败"); } }
    // 4. 填充盘符
    int letterMappingCount = 0;
    for (auto& kv: physicalIndexToLetters){ int diskIdx=kv.first; auto it=tempDisks.find(diskIdx); if(it==tempDisks.end()) continue; auto& pd=it->second; int count=0; for(char L: kv.second){ if(count>=8) break; pd.logicalDriveLetters[count++]=L; } pd.logicalDriveCount=count; letterMappingCount += count; Logger::Debug(L"DiskInfo: 物理磁盘 " + std::to_wstring(diskIdx) + L" 关联了 " + std::to_wstring(count) + L" 个逻辑驱动器"); }
    Logger::Debug("DiskInfo: 物理磁盘到逻辑驱动器映射完成，共 " + std::to_string(letterMappingCount) + " 个映射");
    // 5. 写入 SystemInfo
    sysInfo.physicalDisks.clear(); for (auto& kv: tempDisks){ sysInfo.physicalDisks.push_back(kv.second); if (sysInfo.physicalDisks.size()>=8) break; }
    Logger::Debug(L"物理磁盘枚举完成: " + std::to_wstring(sysInfo.physicalDisks.size()) + L" 个");
}