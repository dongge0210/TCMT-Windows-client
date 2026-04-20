#include "DiskInfo.h"
#include "../utils/Logger.h"

#ifdef TCMT_WINDOWS
// ======================== Windows Implementation ========================
#include "../utils/WinUtils.h"
#include "../utils/WmiManager.h"
#include <wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")

DiskInfo::DiskInfo() { QueryDrives(); }

void DiskInfo::QueryDrives() {
    drives.clear();
    DWORD driveMask = GetLogicalDrives();
    if (driveMask == 0) { Logger::Error("GetLogicalDrives 失败"); return; }
    for (int i = 0; i < 26; ++i) {
        if ((driveMask & (1 << i)) == 0) continue;
        char driveLetter = static_cast<char>('A' + i);
        if (driveLetter == 'A' || driveLetter == 'B') continue;
        std::wstring rootPath;
        rootPath.reserve(4);
        rootPath.push_back(static_cast<wchar_t>(L'A' + i));
        rootPath.append(L":\\");
        UINT driveType = GetDriveTypeW(rootPath.c_str());
        if (!(driveType == DRIVE_FIXED || driveType == DRIVE_REMOVABLE)) continue;
        ULARGE_INTEGER freeBytesAvailable{}, totalBytes{}, totalFreeBytes{};
        if (!GetDiskFreeSpaceExW(rootPath.c_str(), &freeBytesAvailable, &totalBytes, &totalFreeBytes)) continue;
        if (totalBytes.QuadPart == 0) continue;
        DriveInfo info{};
        info.letter = driveLetter;
        info.totalSize = totalBytes.QuadPart;
        info.freeSpace = totalFreeBytes.QuadPart;
        info.usedSpace = (totalBytes.QuadPart >= totalFreeBytes.QuadPart)
                         ? (totalBytes.QuadPart - totalFreeBytes.QuadPart) : 0ULL;
        wchar_t volumeName[MAX_PATH + 1] = {0};
        wchar_t fileSystemName[MAX_PATH + 1] = {0};
        DWORD fsFlags = 0;
        if (!GetVolumeInformationW(rootPath.c_str(), volumeName, MAX_PATH, nullptr, nullptr, &fsFlags, fileSystemName, MAX_PATH)) {
            info.label = "";
            info.fileSystem = "未知";
        } else {
            info.label = WinUtils::WstringToString(volumeName);
            if (info.label.empty()) info.label = "未命名";
            info.fileSystem = WinUtils::WstringToString(fileSystemName);
        }
        drives.push_back(std::move(info));
    }
    std::sort(drives.begin(), drives.end(), [](const DriveInfo& a, const DriveInfo& b) {
        return a.letter < b.letter;
    });
}

void DiskInfo::Refresh() { QueryDrives(); }
const std::vector<DriveInfo>& DiskInfo::GetDrives() const { return drives; }

std::vector<DiskData> DiskInfo::GetDisks() {
    std::vector<DiskData> disks;
    disks.reserve(drives.size());
    for (const auto& drive : drives) {
        DiskData d;
        d.letter = drive.letter;
        d.label = drive.label;
        d.fileSystem = drive.fileSystem;
        d.totalSize = drive.totalSize;
        d.usedSpace = drive.usedSpace;
        d.freeSpace = drive.freeSpace;
        disks.push_back(std::move(d));
    }
    return disks;
}

static bool ParseDiskPartition(const std::wstring& text, int& diskIndexOut) {
    size_t posDisk = text.find(L"Disk #");
    if (posDisk == std::wstring::npos) return false;
    posDisk += 6;
    if (posDisk >= text.size()) return false;
    int num = 0;
    bool any = false;
    while (posDisk < text.size() && iswdigit(text[posDisk])) {
        any = true;
        num = num * 10 + (text[posDisk] - L'0');
        ++posDisk;
    }
    if (!any) return false;
    diskIndexOut = num;
    return true;
}

void DiskInfo::CollectPhysicalDisks(WmiManager& wmi, const std::vector<DiskData>& logicalDisks, SystemInfo& sysInfo) {
    IWbemServices* svc = wmi.GetWmiService();
    if (!svc) { Logger::Warn("WMI 服务无效，跳过物理磁盘枚举"); return; }
    std::map<int, std::vector<char>> physicalIndexToLetters;
    std::map<char, int> letterToDiskIndex;
    IEnumWbemClassObject* pEnum = nullptr;

    pEnum = nullptr;
    HRESULT hr = svc->ExecQuery(bstr_t(L"WQL"),
        bstr_t(L"SELECT * FROM Win32_LogicalDiskToPartition"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnum);
    if (SUCCEEDED(hr) && pEnum) {
        IWbemClassObject* obj = nullptr;
        ULONG ret = 0;
        while (pEnum->Next(WBEM_INFINITE, 1, &obj, &ret) == S_OK) {
            VARIANT ant, dep;
            VariantInit(&ant); VariantInit(&dep);
            if (SUCCEEDED(obj->Get(L"Antecedent", 0, &ant, 0, 0)) && ant.vt == VT_BSTR &&
                SUCCEEDED(obj->Get(L"Dependent", 0, &dep, 0, 0)) && dep.vt == VT_BSTR) {
                int diskIdx = -1;
                if (ParseDiskPartition(dep.bstrVal, diskIdx)) {
                    std::wstring depStr = dep.bstrVal;
                    size_t pos = depStr.find(L"DeviceID=\"");
                    if (pos != std::wstring::npos) {
                        pos += 10;
                        if (pos < depStr.size()) {
                            wchar_t letterW = depStr[pos];
                            if (letterW && letterW != L'"') {
                                letterToDiskIndex[static_cast<char>(::toupper(letterW))] = diskIdx;
                            }
                        }
                    }
                }
            }
            VariantClear(&ant); VariantClear(&dep);
            obj->Release();
        }
        pEnum->Release();
    }
    for (auto& kv : letterToDiskIndex)
        physicalIndexToLetters[kv.second].push_back(kv.first);

    std::map<int, PhysicalDiskSmartData> tempDisks;
    pEnum = nullptr;
    hr = svc->ExecQuery(bstr_t(L"WQL"),
        bstr_t(L"SELECT Index,Model,SerialNumber,InterfaceType,Size,MediaType FROM Win32_DiskDrive"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnum);
    if (SUCCEEDED(hr) && pEnum) {
        IWbemClassObject* obj = nullptr;
        ULONG ret = 0;
        while (pEnum->Next(WBEM_INFINITE, 1, &obj, &ret) == S_OK) {
            VARIANT vIndex, vModel, vSerial, vIface, vSize, vMedia;
            VariantInit(&vIndex); VariantInit(&vModel); VariantInit(&vSerial);
            VariantInit(&vIface); VariantInit(&vSize); VariantInit(&vMedia);
            if (SUCCEEDED(obj->Get(L"Index", 0, &vIndex, 0, 0))
                && (vIndex.vt == VT_I4 || vIndex.vt == VT_UI4)) {
                int idx = (vIndex.vt == VT_I4) ? vIndex.intVal : static_cast<int>(vIndex.uintVal);
                PhysicalDiskSmartData data{};
                if (SUCCEEDED(obj->Get(L"Model", 0, &vModel, 0, 0)) && vModel.vt == VT_BSTR)
                    wcsncpy_s(data.model, vModel.bstrVal, _TRUNCATE);
                if (SUCCEEDED(obj->Get(L"SerialNumber", 0, &vSerial, 0, 0)) && vSerial.vt == VT_BSTR)
                    wcsncpy_s(data.serialNumber, vSerial.bstrVal, _TRUNCATE);
                if (SUCCEEDED(obj->Get(L"InterfaceType", 0, &vIface, 0, 0)) && vIface.vt == VT_BSTR)
                    wcsncpy_s(data.interfaceType, vIface.bstrVal, _TRUNCATE);
                if (SUCCEEDED(obj->Get(L"Size", 0, &vSize, 0, 0))) {
                    if (vSize.vt == VT_UI8) data.capacity = vSize.ullVal;
                    else if (vSize.vt == VT_BSTR) data.capacity = _wcstoui64(vSize.bstrVal, nullptr, 10);
                }
                if (SUCCEEDED(obj->Get(L"MediaType", 0, &vMedia, 0, 0)) && vMedia.vt == VT_BSTR) {
                    std::wstring media = vMedia.bstrVal;
                    if (media.find(L"SSD") != std::wstring::npos || media.find(L"Solid State") != std::wstring::npos)
                        wcsncpy_s(data.diskType, L"SSD", _TRUNCATE);
                    else
                        wcsncpy_s(data.diskType, L"HDD", _TRUNCATE);
                } else {
                    wcsncpy_s(data.diskType, L"未知", _TRUNCATE);
                }
                data.smartSupported = false;
                data.smartEnabled = false;
                data.healthPercentage = 0;
                data.temperature = 0.0;
                data.logicalDriveCount = 0;
                tempDisks[idx] = data;
            }
            VariantClear(&vIndex); VariantClear(&vModel); VariantClear(&vSerial);
            VariantClear(&vIface); VariantClear(&vSize); VariantClear(&vMedia);
            obj->Release();
        }
        pEnum->Release();
    }
    for (auto& kv : physicalIndexToLetters) {
        int diskIdx = kv.first;
        auto it = tempDisks.find(diskIdx);
        if (it == tempDisks.end()) continue;
        auto& pd = it->second;
        int count = 0;
        for (char L : kv.second) {
            if (count >= 8) break;
            pd.logicalDriveLetters[count++] = L;
        }
        pd.logicalDriveCount = count;
    }
    sysInfo.physicalDisks.clear();
    for (auto& kv : tempDisks) {
        sysInfo.physicalDisks.push_back(kv.second);
        if (sysInfo.physicalDisks.size() >= 8) break;
    }
    Logger::Debug("物理磁盘枚举完成: " + std::to_string(sysInfo.physicalDisks.size()) + " 个");
}

#elif defined(TCMT_MACOS)
// ======================== macOS Implementation ========================
#include <sys/mount.h>
#include <mach/mach.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

#include <dirent.h>
#include <algorithm>
#include <set>

static std::string GetFsType(const std::string& mountpoint) {
    struct statfs fs;
    if (statfs(mountpoint.c_str(), &fs) == 0) {
        return std::string(fs.f_fstypename);
    }
    return "unknown";
}

// Get disk IOKit info
static bool GetIOKitDiskInfo(const std::string& bsdName,
                              uint64_t& capacity,
                              std::string& model,
                              std::string& mediaType) {
    io_iterator_t iter = 0;
    io_registry_entry_t parent = 0;
    CFMutableDictionaryRef matching = IOServiceMatching("IONVMeController");
    if (!matching) matching = IOServiceMatching("IOATABlockStorageDevice");
    if (!matching) return false;

    kern_return_t kr = IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iter);
    if (kr != KERN_SUCCESS) return false;

    while ((parent = IOIteratorNext(iter)) != 0) {
        CFTypeRef bsdRef = IORegistryEntryCreateCFProperty(parent, CFSTR("BSD Name"), kCFAllocatorDefault, 0);
        if (!bsdRef) { IOObjectRelease(parent); continue; }
        CFStringRef bsdNameRef = (CFStringRef)bsdRef;
        char bsdNameBuf[64] = {0};
        CFStringGetCString(bsdNameRef, bsdNameBuf, sizeof(bsdNameBuf), kCFStringEncodingUTF8);
        CFRelease(bsdRef);

        if (bsdName == bsdNameBuf) {
            // Capacity
            CFTypeRef capRef = IORegistryEntryCreateCFProperty(parent, CFSTR("Size"), kCFAllocatorDefault, 0);
            if (capRef) {
                if (CFNumberGetValue((CFNumberRef)capRef, kCFNumberSInt64Type, &capacity))
                    capacity = 0;
                CFRelease(capRef);
            } else {
                capacity = 0;
            }
            // Model
            CFTypeRef modelRef = IORegistryEntryCreateCFProperty(parent, CFSTR("Model"), kCFAllocatorDefault, 0);
            if (modelRef) {
                char modelBuf[128] = {0};
                CFStringGetCString((CFStringRef)modelRef, modelBuf, sizeof(modelBuf), kCFStringEncodingUTF8);
                model = modelBuf;
                CFRelease(modelRef);
            }
            // Media type
            CFTypeRef mediaRef = IORegistryEntryCreateCFProperty(parent, CFSTR("Medium Type"), kCFAllocatorDefault, 0);
            if (mediaRef) {
                char mediaBuf[64] = {0};
                CFStringGetCString((CFStringRef)mediaRef, mediaBuf, sizeof(mediaBuf), kCFStringEncodingUTF8);
                mediaType = mediaBuf;
                CFRelease(mediaRef);
            }
            IOObjectRelease(parent);
            IOObjectRelease(iter);
            return true;
        }
        IOObjectRelease(parent);
    }
    IOObjectRelease(iter);
    return false;
}

DiskInfo::DiskInfo() { QueryDrives(); }

void DiskInfo::QueryDrives() {
    drives.clear();

    // Enumerate mounted volumes via statfs
    int mountCount = getfsstat(nullptr, 0, MNT_NOWAIT);
    if (mountCount <= 0) {
        Logger::Error("DiskInfo: getfsstat failed");
        return;
    }

    std::vector<struct statfs> mounts(mountCount);
    getfsstat(mounts.data(), (int)(mounts.size() * sizeof(struct statfs)), MNT_NOWAIT);

    // Track physical disks to deduplicate APFS volumes on same device
    std::set<std::string> seenPhysicalDisks;

    for (const auto& fs : mounts) {
        std::string mountpoint(fs.f_mntonname);
        std::string fstype(fs.f_fstypename);
        std::string devname(fs.f_mntfromname);

        // Skip non-physical volumes and macOS system snapshots
        if (fstype == "autofs" || fstype == "devfs" || fstype == "fdesc"
            || fstype == "procfs" || fstype == "devpts" || fstype == "overlay"
            || fstype == "nullfs" || fstype == "simfs"
            || mountpoint == "/System/Volumes/VM"
            || mountpoint == "/System/Volumes/Preboot"
            || mountpoint == "/System/Volumes/Update"
            || mountpoint == "/System/Volumes/xarts"
            || mountpoint == "/System/Volumes/iSCPreboot"
            || mountpoint == "/System/Volumes/Hardware"
            || mountpoint.find("/System/Volumes/Update/") == 0
            || mountpoint.find("/private/var/folders/") == 0) {
            continue;
        }

        // Get BSD name (e.g. disk0s1)
        std::string bsdName = devname;
        if (bsdName.find("/dev/") == 0)
            bsdName = bsdName.substr(5);

        // Get capacity and free space
        uint64_t total = (uint64_t)fs.f_blocks * (uint64_t)fs.f_bsize;
        uint64_t free = (uint64_t)fs.f_bfree * (uint64_t)fs.f_bsize;
        uint64_t used = total - free;

        // Only include significant volumes (skip small system volumes)
        if (total < 100 * 1024 * 1024) continue; // skip < 100MB

        // Skip boot partition
        if (bsdName.find("s1") != std::string::npos && mountpoint == "/") {
            // Root volume
        }

        DriveInfo info{};
        info.totalSize = total;
        info.freeSpace = free;
        info.usedSpace = used;
        info.fileSystem = fstype;

        // Volume name from last path component
        size_t lastSlash = mountpoint.find_last_of('/');
        info.label = (lastSlash != std::string::npos && lastSlash + 1 < mountpoint.size())
                     ? mountpoint.substr(lastSlash + 1) : mountpoint;
        if (info.label.empty()) info.label = "Untitled";

        // Derive letter: use '/' for root, 'A'+index for others
        if (mountpoint == "/") {
            info.letter = '/'; // root indicator
        } else {
            info.letter = 0; // no drive letter on macOS
        }

        // Deduplicate: extract physical disk name (e.g. "disk3s1s1" -> "disk3")
        // Skip if we've already seen this physical disk
        std::string physicalDisk = bsdName;
        // Remove partition suffixes: keep only up to first 's' after "diskN"
        auto diskPrefixEnd = physicalDisk.find('s', 4); // skip "disk"
        if (diskPrefixEnd != std::string::npos) {
            physicalDisk = physicalDisk.substr(0, diskPrefixEnd);
        }
        if (!seenPhysicalDisks.insert(physicalDisk).second) {
            // Already have a volume from this physical disk
            Logger::Debug("DiskInfo: skipping duplicate APFS volume " + mountpoint
                        + " on " + physicalDisk);
            continue;
        }

        drives.push_back(std::move(info));
        Logger::Debug("DiskInfo: found volume " + mountpoint
                    + " (" + fstype + ") size=" + std::to_string(total));
    }
}

void DiskInfo::Refresh() { QueryDrives(); }
const std::vector<DriveInfo>& DiskInfo::GetDrives() const { return drives; }

std::vector<DiskData> DiskInfo::GetDisks() {
    std::vector<DiskData> disks;
    disks.reserve(drives.size());
    for (const auto& drive : drives) {
        DiskData d;
        d.letter = drive.letter;
        d.label = drive.label;
        d.fileSystem = drive.fileSystem;
        d.totalSize = drive.totalSize;
        d.usedSpace = drive.usedSpace;
        d.freeSpace = drive.freeSpace;
        disks.push_back(std::move(d));
    }
    return disks;
}

// macOS: SMART data not available without root/special drivers
void DiskInfo::CollectSmartData(SystemInfo& sysInfo) {
    // No-op on macOS: SMART requires root privileges and IOKit SMART interface
    Logger::Debug("DiskInfo: SMART data collection skipped on macOS");
}

#else
#error "Unsupported platform"
#endif
