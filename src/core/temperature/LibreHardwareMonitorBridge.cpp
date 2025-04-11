#include "LibreHardwareMonitorBridge.h"
#include <msclr/marshal_cppstd.h>
#include <vcclr.h>
#include <iostream>

// Ensure .NET 8.0 compatibility
#using "F:\\Win_x64-sysMonitor\\src\\third_party\\LibreHardwareMonitor\\bin\\Debug\\net8.0\\LibreHardwareMonitorLib.dll"

using namespace LibreHardwareMonitor::Hardware;
using namespace System;
using namespace System::Collections::Generic;
using namespace msclr::interop;

// Define UpdateVisitor fully in the implementation file
ref class UpdateVisitor : public IVisitor {
public:
    virtual void VisitComputer(IComputer^ computer) {
        computer->Traverse(this);
    }
    virtual void VisitHardware(IHardware^ hardware) {
        hardware->Update();
        hardware->Traverse(this);
    }
    virtual void VisitSensor(ISensor^ sensor) {}
    virtual void VisitParameter(IParameter^ parameter) {}
};

// Define static members
bool LibreHardwareMonitorBridge::initialized = false;
gcroot<Computer^> LibreHardwareMonitorBridge::computer;
gcroot<UpdateVisitor^> LibreHardwareMonitorBridge::visitor;

void LibreHardwareMonitorBridge::Initialize() {
    if (initialized) return;
    computer = gcnew Computer();
    computer->IsCpuEnabled = true;
    computer->Open();
    visitor = gcnew UpdateVisitor();
    initialized = true;
}

void LibreHardwareMonitorBridge::Cleanup() {
    if (!initialized) return;
    computer->Close();
    computer = nullptr; // Replace computer.reset() with computer = nullptr
    visitor = nullptr;  // Replace visitor.reset() with visitor = nullptr
    initialized = false;
}

std::vector<std::pair<std::string, double>> LibreHardwareMonitorBridge::GetTemperatures() {
    std::vector<std::pair<std::string, double>> temps;
    if (!initialized) return temps;

    computer->Accept(visitor);
    // Collect CPU and GPU temperature sensors
    for each (IHardware^ hardware in computer->Hardware) {
        hardware->Update();
        if (hardware->HardwareType == HardwareType::Cpu ||
            hardware->HardwareType == HardwareType::GpuNvidia ||
            hardware->HardwareType == HardwareType::GpuAmd) {
            for each (ISensor^ sensor in hardware->Sensors) {
                if (sensor->SensorType == SensorType::Temperature && sensor->Value.HasValue) {
                    std::string name = msclr::interop::marshal_as<std::string>(sensor->Name);
                    temps.push_back({ name, sensor->Value.Value });
                }
            }
        }
    }
    return temps;
}