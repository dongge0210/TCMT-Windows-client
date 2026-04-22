#pragma once
#include <string>
#include <vector>
#include <utility>

// Forward declaration, avoid including GPU header in header file
class GpuInfo;

class TemperatureWrapper {
public:
    static void Initialize();
    static void Cleanup();
    static std::vector<std::pair<std::string, double> > GetTemperatures();
    static bool IsInitialized();

private:
    static bool initialized;
};
