#pragma once
#include <string>
#include <windows.h>

class OSInfo {
public:
    OSInfo();
    std::string GetVersion() const;
    void Initialize();
private:
    std::string osVersion;
    DWORD majorVersion;
    DWORD minorVersion;
    DWORD buildNumber;
};
