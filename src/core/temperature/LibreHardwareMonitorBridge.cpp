#include "LibreHardwareMonitorBridge.h"
#include "Logger.h"
#include <msclr/marshal_cppstd.h>
#include <iostream>

// 确保 .NET 8.0 兼容性
#using "F:/Win_x64-10.lastest-sysMonitor/src/third_party/LibreHardwareMonitor-0.9.4/bin/Debug/net8.0/LibreHardwareMonitorLib.dll"

using namespace LibreHardwareMonitor::Hardware;
using namespace System;
using namespace System::Collections::Generic;
using namespace msclr::interop;

// 定义静态成员
bool LibreHardwareMonitorBridge::initialized = false;
gcroot<Computer^> LibreHardwareMonitorBridge::computer;
gcroot<UpdateVisitor^> LibreHardwareMonitorBridge::visitor;

void LibreHardwareMonitorBridge::Initialize() {
    try {
        if (initialized) return;
        computer = gcnew Computer();
        computer->IsCpuEnabled = true;
        computer->Open();
        visitor = gcnew UpdateVisitor();
        initialized = true;
    }
    catch (System::IO::FileNotFoundException^ ex) {
        // 使用 marshal_as 将 .NET 字符串转换为 std::string
        std::string errorMsg = msclr::interop::marshal_as<std::string>(ex->Message);
        Logger::Error("LibreHardwareMonitor 初始化失败: " + errorMsg);
    }
    catch (System::Exception^ ex) {
        std::string errorMsg = msclr::interop::marshal_as<std::string>(ex->Message);
        Logger::Error("LibreHardwareMonitor 初始化异常: " + errorMsg);
    }
}

void LibreHardwareMonitorBridge::Cleanup() {
    if (!initialized) return;
    computer->Close();
    computer = nullptr;
    visitor = nullptr;
    initialized = false;
}

std::vector<std::pair<std::string, double>> LibreHardwareMonitorBridge::GetTemperatures() {
    std::vector<std::pair<std::string, double>> temps;
    if (!initialized) return temps;

    computer->Accept(visitor);
    for each (IHardware ^ hardware in computer->Hardware) {
        hardware->Update();
        if (hardware->HardwareType == HardwareType::Cpu ||
            hardware->HardwareType == HardwareType::GpuNvidia ||
            hardware->HardwareType == HardwareType::GpuAmd) {
            for each (ISensor ^ sensor in hardware->Sensors) {
                if (sensor->SensorType == SensorType::Temperature && sensor->Value.HasValue) {
                    std::string name = marshal_as<std::string>(sensor->Name);
                    temps.push_back({ name, sensor->Value.Value });
                }
            }
        }
    }
    return temps;
}