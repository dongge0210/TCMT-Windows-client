#pragma once
// No changes needed here
// ...existing code...
#include <string>
#include <vector>
#include <utility>
#include <msclr/marshal_cppstd.h>
#include <vcclr.h>

// Ensure .NET 8.0 compatibility
#using <mscorlib.dll>
#using <System.dll>
#using <System.Core.dll>
#using <System.Runtime.dll>
#using "F:\\Win_x64-sysMonitor\\src\\third_party\\LibreHardwareMonitor\\bin\\Debug\\net8.0\\LibreHardwareMonitorLib.dll"

using namespace System;
using namespace System::Collections::Generic;
using namespace LibreHardwareMonitor::Hardware;

// Forward declare ref classes so they're defined before use
ref class UpdateVisitor;

class LibreHardwareMonitorBridge {
public:
    static void Initialize();
    static void Cleanup();
    static std::vector<std::pair<std::string, double>> GetTemperatures();

private:
    static bool initialized;
    static gcroot<Computer^> computer;
    static gcroot<UpdateVisitor^> visitor; // Updated to use concrete implementation

    static bool IsCpuTemperature(Tuple<String^, double>^ t);
};
