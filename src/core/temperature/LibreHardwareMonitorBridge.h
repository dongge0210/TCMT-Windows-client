#pragma once

#include <string>
#include <vector>
#include <utility>
#include <vcclr.h> // 必须包含以使用 gcroot

#using "F:\Win_x64-10.lastest-sysMonitor\src\third_party\LibreHardwareMonitor\bin\Debug\net8.0\LibreHardwareMonitorLib.dll"


// 添加对 .NET 类型的前向声明
namespace LibreHardwareMonitor {
    namespace Hardware {
        ref class Computer; // 前向声明 Computer
    }
}

// 定义 UpdateVisitor 类
ref class UpdateVisitor : public LibreHardwareMonitor::Hardware::IVisitor {
public:
    virtual void VisitComputer(LibreHardwareMonitor::Hardware::IComputer^ computer) {
        computer->Traverse(this);
    }
    virtual void VisitHardware(LibreHardwareMonitor::Hardware::IHardware^ hardware) {
        hardware->Update();
        hardware->Traverse(this);
    }
    virtual void VisitSensor(LibreHardwareMonitor::Hardware::ISensor^ sensor) {}
    virtual void VisitParameter(LibreHardwareMonitor::Hardware::IParameter^ parameter) {}
};

class LibreHardwareMonitorBridge {
public:
    static void Initialize();
    static void Cleanup();
    static std::vector<std::pair<std::string, double>> GetTemperatures();

private:
    static bool initialized;
    static gcroot<LibreHardwareMonitor::Hardware::Computer^> computer; // 使用 gcroot 包装 Computer
    static gcroot<UpdateVisitor^> visitor; // 使用 gcroot 包装 UpdateVisitor
};
