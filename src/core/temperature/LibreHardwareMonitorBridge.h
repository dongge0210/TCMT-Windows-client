#include <string>
#include <vector>
#include <utility>

#pragma managed
void CallSensorMonitorInitialize() {
    SensorMonitor::Initialize();
}
#pragma unmanaged

class LibreHardwareMonitorBridge {
public:
    static void Initialize();
    static void Cleanup();
    static std::vector<std::pair<std::string, double>> GetTemperatures();

private:
    static bool initialized;
};

