#include <iostream> 
#include <vector>

// ...existing code...

// Remove duplicate ManagedBridge definition
#pragma managed
ref class ManagedBridge {
public:
    static void InitializeSensorMonitor() {
        SensorMonitor::Initialize();
    }

    static void CleanupSensorMonitor() {
        SensorMonitor::Cleanup();
    }
};
#pragma unmanaged

// Refactor Initialize to avoid direct usage of managed types
void LibreHardwareMonitorBridge::Initialize() {
    if (!initialized) {
        try {
            // Call managed code through a wrapper function
            CallSensorMonitorInitialize();
            initialized = true;
        }
        catch (...) {
            std::cerr << "Error: Failed to initialize SensorMonitor." << std::endl;
        }
    }
}

// Refactor Cleanup to avoid direct usage of managed types
void LibreHardwareMonitorBridge::Cleanup() {
    if (initialized) {
        try {
            // Call managed code through a wrapper function
            CallSensorMonitorCleanup();
            initialized = false;
        }
        catch (...) {
            std::cerr << "Error: Failed to cleanup SensorMonitor." << std::endl;
        }
    }
}

// Consolidate GetTemperatures logic
std::vector<std::pair<std::string, double>> LibreHardwareMonitorBridge::GetTemperatures() {
    std::vector<std::pair<std::string, double>> results;

    try {
        // Call managed code through a wrapper function
        #pragma managed
        auto managedResults = SensorMonitor::GetTemperatures();
        #pragma unmanaged

        for each (auto entry in managedResults) {
            results.emplace_back(
                marshal_as<std::string>(entry->Item1),
                static_cast<double>(entry->Item2)
            );
        }
    }
    catch (...) {
        std::cerr << "Error: Failed to get temperatures." << std::endl;
    }

    return results;
}

// Remove redundant comments and debug messages
#pragma managed
bool SensorMonitor::IsCpuTemperature(Tuple<String^, float>^ t) {
    return t->Item1->StartsWith("CPU");
}

List<Tuple<String^, float>^>^ SensorMonitor::GetTemperatures() {
    List<Tuple<String^, float>^>^ results = gcnew List<Tuple<String^, float>^>();

    for each (IHardware^ hardware in computer->Hardware) {
        hardware->Update();

        for each (ISensor^ sensor in hardware->Sensors) {
            if (sensor->SensorType == SensorType::Temperature && sensor->Value.HasValue) {
                results->Add(gcnew Tuple<String^, float>(sensor->Name, safe_cast<float>(sensor->Value.Value)));
            }
        }
    }

    return results;
}
#pragma unmanaged

// ...existing code...
#include "LibreHardwareMonitorBridge.h"

// Separate C++/CLI part into its own compilation unit
#pragma managed
#include <msclr/marshal_cppstd.h>
#include <vcclr.h>

#using "F:\Win_x64-sysMonitor\src\third_party\LibreHardwareMonitor\bin\Debug\net8.0\LibreHardwareMonitorLib.dll"

using namespace System;
using namespace System::Collections::Generic;
using namespace msclr::interop;
using namespace LibreHardwareMonitor::Hardware;

// Add missing TryLoadLibrary method
static bool TryLoadLibrary() {
    try {
        System::Reflection::Assembly::LoadFrom("F:\\Win_x64-sysMonitor\\src\\third_party\\LibreHardwareMonitor\\bin\\Debug\\net8.0\\LibreHardwareMonitorLib.dll");
        return true;
    }
    catch (Exception^) {
        return false;
    }
}

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

ref class SensorMonitor {
private:
    static Computer^ computer = nullptr;
    static UpdateVisitor^ visitor = nullptr;

    static bool IsCpuTemperature(Tuple<String^, float>^ t) {
        return t->Item1->StartsWith("CPU");
    }

public:
    static void Initialize() {
        try {
            // 尝试加载库
            if (!TryLoadLibrary()) {
                throw gcnew System::IO::FileNotFoundException("无法找到 LibreHardwareMonitorLib.dll，请确保它已正确放置在 'lib' 目录中。");
            }
            
            if (computer != nullptr) {
                computer->Close();
                delete computer;
            }
            computer = gcnew Computer();
            if (!computer) {
                throw gcnew Exception("Failed to create Computer instance");
            }
            computer->IsCpuEnabled = true;
            computer->IsGpuEnabled = true;
            computer->IsMotherboardEnabled = true;
            computer->Open();

            visitor = gcnew UpdateVisitor();
            if (!visitor) {
                throw gcnew Exception("Failed to create UpdateVisitor instance");
            }
            computer->Accept(visitor);
        }
        catch (Exception^ ex) {
            System::Diagnostics::Debug::WriteLine("Failed to initialize: " + ex->Message);
            throw; // 重新抛出异常以便跟踪
        }
    }

    static void Cleanup() {
        if (computer != nullptr) {
            try {
                computer->Close();
                delete computer;
                computer = nullptr;
                if (visitor != nullptr) {
                    delete visitor;
                    visitor = nullptr;
                }
            }
            catch (Exception^ ex) {
                System::Diagnostics::Debug::WriteLine(ex->Message);
            }
        }
    }

    static List<Tuple<String^, float>^>^ GetTemperatures() {
        List<Tuple<String^, float>^>^ results = gcnew List<Tuple<String^, float>^>();
        float cpuTotalTemp = 0;
        int cpuCoreCount = 0;

        try {
            if (computer == nullptr) {
                return results;
            }

            computer->Accept(visitor);

            for each (IHardware^ hardware in computer->Hardware) {
                hardware->Update();

                if (hardware->HardwareType == HardwareType::Cpu) {
                    bool foundPackageTemp = false;

                    for each (ISensor^ sensor in hardware->Sensors) {
                        if (sensor->SensorType == SensorType::Temperature) {
                            // 修改 Nullable 值的处理
                            if (sensor->Value.HasValue) {
                                float temp = safe_cast<float>(sensor->Value.Value);
                                String^ sensorName = sensor->Name;

                                if (sensorName->Contains("Package")) {
                                    results->Add(gcnew Tuple<String^, float>(
                                        "CPU Package",
                                        temp
                                    ));
                                    foundPackageTemp = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (!foundPackageTemp) {
                        cpuTotalTemp = 0;
                        cpuCoreCount = 0;

                        for each (ISensor^ sensor in hardware->Sensors) {
                            if (sensor->SensorType == SensorType::Temperature &&
                                sensor->Value.HasValue &&
                                sensor->Name->Contains("Core")) {
                                cpuTotalTemp += safe_cast<float>(sensor->Value.Value);
                                cpuCoreCount++;
                            }
                        }

                        if (cpuCoreCount > 0) {
                            results->Add(gcnew Tuple<String^, float>(
                                "CPU Average Core",
                                cpuTotalTemp / cpuCoreCount
                            ));
                        }
                    }
                }
                else if (hardware->HardwareType == HardwareType::GpuNvidia || hardware->HardwareType == HardwareType::GpuAmd) {
                    for each (ISensor^ sensor in hardware->Sensors) {
                        if (sensor->SensorType == SensorType::Temperature &&
                            sensor->Value.HasValue) {
                            results->Add(gcnew Tuple<String^, float>(
                                "GPU Core - " + hardware->Name,
                                safe_cast<float>(sensor->Value.Value)
                            ));
                            break;
                        }
                    }
                }
            }

            // 使用静态方法替代 Lambda
            if (!results->Exists(gcnew Predicate<Tuple<String^, float>^>(IsCpuTemperature))) {
                for each (IHardware^ hardware in computer->Hardware) {
                    if (hardware->HardwareType == HardwareType::Motherboard) {
                        for each (ISensor^ sensor in hardware->Sensors) {
                            if (sensor->SensorType == SensorType::Temperature &&
                                sensor->Value.HasValue &&
                                sensor->Name->Contains("CPU")) {
                                results->Add(gcnew Tuple<String^, float>(
                                    "CPU Temperature",
                                    safe_cast<float>(sensor->Value.Value)
                                ));
                                break;
                            }
                        }
                    }
                }
            }
        }
        catch (Exception^ ex) {
            System::Diagnostics::Debug::WriteLine("Error getting temperatures: " + ex->Message);
        }

        return results;
    }
};

// Remove duplicate SensorMonitorWrapper definition
#pragma managed
void CallSensorMonitorInitialize() {
    SensorMonitor::Initialize();
}

void CallSensorMonitorCleanup() {
    SensorMonitor::Cleanup();
}
#pragma unmanaged

// Add managed-to-native bridge functions
#pragma managed
ref class ManagedBridge {
public:
    static void InitializeSensorMonitor() {
        SensorMonitor::Initialize();
    }

    static void CleanupSensorMonitor() {
        SensorMonitor::Cleanup();
    }
};
#pragma unmanaged

// Add a gcroot-based native-to-managed bridge
#pragma unmanaged
#include <msclr/gcroot.h>
msclr::gcroot<ManagedBridge^> managedBridgeInstance;

bool LibreHardwareMonitorBridge::initialized = false;

// Refactor Initialize to avoid direct usage of managed types
void LibreHardwareMonitorBridge::Initialize() {
    if (!initialized) {
        try {
            // Call managed code through a wrapper function
            CallSensorMonitorInitialize();
            initialized = true;
        }
        catch (...) {
            std::cerr << "Error: Failed to initialize SensorMonitor." << std::endl;
        }
    }
}

// Refactor Cleanup to avoid direct usage of managed types
void LibreHardwareMonitorBridge::Cleanup() {
    if (initialized) {
        try {
            // Call managed code through a wrapper function
            CallSensorMonitorCleanup();
            initialized = false;
        }
        catch (...) {
            std::cerr << "Error: Failed to cleanup SensorMonitor." << std::endl;
        }
    }
}

// Consolidate GetTemperatures logic
std::vector<std::pair<std::string, double>> LibreHardwareMonitorBridge::GetTemperatures() {
    std::vector<std::pair<std::string, double>> results;

    try {
        // Call managed code through a wrapper function
        #pragma managed
        auto managedResults = SensorMonitor::GetTemperatures();
        #pragma unmanaged

        for each (auto entry in managedResults) {
            results.emplace_back(
                marshal_as<std::string>(entry->Item1),
                static_cast<double>(entry->Item2)
            );
        }
    }
    catch (...) {
        std::cerr << "Error: Failed to get temperatures." << std::endl;
    }

    return results;
}

// Remove redundant comments and debug messages
#pragma managed
bool SensorMonitor::IsCpuTemperature(Tuple<String^, float>^ t) {
    return t->Item1->StartsWith("CPU");
}

List<Tuple<String^, float>^>^ SensorMonitor::GetTemperatures() {
    List<Tuple<String^, float>^>^ results = gcnew List<Tuple<String^, float>^>();

    for each (IHardware^ hardware in computer->Hardware) {
        hardware->Update();

        for each (ISensor^ sensor in hardware->Sensors) {
            if (sensor->SensorType == SensorType::Temperature && sensor->Value.HasValue) {
                results->Add(gcnew Tuple<String^, float>(sensor->Name, safe_cast<float>(sensor->Value.Value)));
            }
        }
    }

    return results;
}
#pragma unmanaged
