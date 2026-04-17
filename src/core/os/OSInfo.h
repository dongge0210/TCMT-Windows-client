#pragma once
#include <string>

#ifdef TCMT_WINDOWS
#include <windows.h>
#elif defined(TCMT_MACOS)
// macOS headers - no special includes needed for OSInfo
#endif

class OSInfo {
public:
    OSInfo();
    std::string GetVersion() const;
    void Initialize();
private:
    std::string osVersion;
#ifdef TCMT_WINDOWS
    DWORD majorVersion;
    DWORD minorVersion;
    DWORD buildNumber;
#endif
};
