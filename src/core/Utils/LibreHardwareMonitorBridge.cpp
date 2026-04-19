#include "LibreHardwareMonitorBridge.h"
#include "Logger.h"
#include <msclr/marshal_cppstd.h>
#include <iostream>
#include <windows.h>

// 不需要重复#using，已在头文件中包含

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

std::vector<PhysicalDiskSmartData> LibreHardwareMonitorBridge::GetPhysicalDisks() {
    std::vector<PhysicalDiskSmartData> disks;
    if (!initialized) return disks;

    try {
        computer->Accept(visitor);
        
        for each (IHardware ^ hardware in computer->Hardware) {
            if (hardware->HardwareType == HardwareType::Storage) {
                hardware->Update();
                
                PhysicalDiskSmartData diskData = {};
                diskData.smartSupported = false;
                diskData.smartEnabled = false;
                diskData.healthPercentage = 0;
                diskData.temperature = 0.0;
                diskData.attributeCount = 0;
                
                // 获取磁盘基本信息
                std::string hwName = marshal_as<std::string>(hardware->Name);
                wchar_t nameBuffer[128] = {};
                mbstowcs_s(nullptr, nameBuffer, hwName.c_str(), _TRUNCATE);
                wcsncpy_s(diskData.model, nameBuffer, _TRUNCATE);
                
                // 遍历传感器获取 SMART 数据
                for each (ISensor ^ sensor in hardware->Sensors) {
                    if (!sensor->Value.HasValue) continue;
                    
                    double value = sensor->Value.Value;
                    
                    // 根据传感器类型填充数据
                    switch (sensor->SensorType) {
                        case SensorType::Temperature:
                            diskData.temperature = value;
                            diskData.smartSupported = true;
                            diskData.smartEnabled = true;
                            break;
                        case SensorType::Level:
                            // 健康百分比或可用空间
                            if (sensor->Name && sensor->Name->Contains("Life")) {
                                diskData.healthPercentage = static_cast<uint8_t>(value);
                            }
                            break;
                        case SensorType::Data:
                            // 写入/读取数据量
                            if (sensor->Name && sensor->Name->Contains("Written")) {
                                diskData.totalBytesWritten = static_cast<uint64_t>(value);
                            } else if (sensor->Name && sensor->Name->Contains("Read")) {
                                diskData.totalBytesRead = static_cast<uint64_t>(value);
                            }
                            break;
                        case SensorType::Throughput:
                            // 吞吐量
                            break;
                        default:
                            break;
                    }
                }
                
                // 简单设置接口类型为未知（实际需要从硬件属性获取）
                wcsncpy_s(diskData.interfaceType, L"Unknown", _TRUNCATE);
                wcsncpy_s(diskData.diskType, L"Unknown", _TRUNCATE);
                
                // 如果支持 SMART 但没有健康数据，默认 100%
                if (diskData.smartSupported && diskData.healthPercentage == 0) {
                    diskData.healthPercentage = 100;
                }
                
                disks.push_back(diskData);
            }
        }
    }
    catch (System::Exception^ ex) {
        std::string errorMsg = msclr::interop::marshal_as<std::string>(ex->Message);
        Logger::Warn("获取磁盘 SMART 数据失败: " + errorMsg);
    }
    
    return disks;
}
