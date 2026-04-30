#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct BatteryInfo {
    std::string name;               // e.g. "Internal Battery"
    uint64_t designCapacity = 0;    // mWh
    uint64_t fullChargeCapacity = 0;// mWh
    uint64_t currentCapacity = 0;   // mWh
    uint32_t voltage = 0;           // mV
    uint32_t cycleCount = 0;
    double chargePercent = 0.0;     // 0-100
    double wearLevel = 0.0;         // 100 * (1 - full/design)
    bool isCharging = false;
    bool isAcConnected = false;
    uint32_t timeRemaining = 0;     // minutes, 0 if AC
};

struct PowerInfo {
    std::string powerPlan;          // active power plan name
    bool isAcPlugged = false;
    std::vector<BatteryInfo> batteries;
    double systemPowerWatts = 0.0;  // estimated total system power consumption

    void Detect();
};
