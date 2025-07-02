// DiskInfo.cpp
#include "DiskInfo.h"
#include "../Utils/WinUtils.h"

DiskInfo::DiskInfo() {
    QueryDrives();
}

void DiskInfo::QueryDrives() {
    drives.clear();
    for (char drive = 'A'; drive <= 'Z'; drive++) {
        std::wstring rootPath = std::wstring(1, drive) + L":\\";
        UINT driveType = GetDriveTypeW(rootPath.c_str());

        if (driveType == DRIVE_FIXED) {
            DriveInfo info;
            info.letter = drive;

            ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
            if (GetDiskFreeSpaceExW(rootPath.c_str(),
                &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {

                info.totalSize = totalBytes.QuadPart;
                info.freeSpace = totalFreeBytes.QuadPart;
                info.usedSpace = totalBytes.QuadPart - totalFreeBytes.QuadPart;

                // 获取卷标
                wchar_t volumeName[MAX_PATH + 1] = { 0 };
                wchar_t fileSystemName[MAX_PATH + 1] = { 0 };
                GetVolumeInformationW(rootPath.c_str(),
                    volumeName, MAX_PATH,
                    NULL, NULL, NULL,
                    fileSystemName, MAX_PATH);

                info.label = volumeName;
                info.fileSystem = fileSystemName;

                drives.push_back(info);
            }
        }
    }
}

void DiskInfo::Refresh() {
    QueryDrives();
}

const std::vector<DriveInfo>& DiskInfo::GetDrives() const {
    return drives;
}

std::vector<DiskData> DiskInfo::GetDisks() {
    std::vector<DiskData> disks;
    for (const auto& drive : drives) {
        DiskData data;
        data.letter = drive.letter;
        data.totalSize = drive.totalSize;
        data.freeSpace = drive.freeSpace;
        data.usedSpace = drive.usedSpace;
        data.label = WinUtils::WstringToString(drive.label);
        data.fileSystem = WinUtils::WstringToString(drive.fileSystem);
        disks.push_back(data);
    }
    return disks;
}