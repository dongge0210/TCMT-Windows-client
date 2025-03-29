// DiskInfo.h
#pragma once
#include <string>
#include <vector>
#include <windows.h>

struct DriveInfo {
    char letter;
    uint64_t totalSize;
    uint64_t freeSpace;
    uint64_t usedSpace;
    std::wstring label;
    std::wstring fileSystem;
};

class DiskInfo {
public:
    DiskInfo();
    const std::vector<DriveInfo>& GetDrives() const;
    void Refresh();

private:
    void QueryDrives();
    std::vector<DriveInfo> drives;
};

