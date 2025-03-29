#include "LibreHardwareMonitorBridge.h"
#include <msclr/marshal_cppstd.h>
#include <utility>

#using "F:\Win_x64-sysMonitor\src\third_party\LibreHardwareMonitor\LibreHardwareMonitorLib\obj\Debug\net8.0\LibreHardwareMonitorLib.dll"

using namespace LibreHardwareMonitor::Hardware;
using namespace System;
using namespace System::Collections::Generic;
using namespace msclr::interop;

ref class SensorMonitor {
private:
    static Computer^ computer = gcnew Computer();

public:
    static void Initialize() {
        computer->IsCpuEnabled = true;
        computer->IsGpuEnabled = true;
        computer->Open();
    }

    static List<Tuple<String^, float>^>^ GetTemperatures() {
        List<Tuple<String^, float>^>^ results = (gcnew List<Tuple<String^, float>^>);
        Dictionary<String^, List<float>^>^ tempGroups = (gcnew Dictionary<String^, List<float>^>);

        for each (IHardware ^ hardware in computer->Hardware) {
            hardware->Update();
            for each (ISensor ^ sensor in hardware->Sensors) {
                if (sensor->SensorType == SensorType::Temperature && sensor->Value.HasValue) {
                    String^ key;
                    float value = safe_cast<float>(sensor->Value.Value);

                    // 根据传感器名称进行分组
                    if (sensor->Name->Contains("CPU Core")) {
                        key = "CPU Cores";
                    }
                    else if (sensor->Name->Contains("CPU Package")) {
                        key = "CPU Package";
                    }
                    else if (sensor->Name->Contains("GPU")) {
                        key = sensor->Name;
                    }
                    else if (sensor->Name->Contains("Distance to TjMax")) {
                        continue; // 跳过 TjMax 相关的温度
                    }
                    else {
                        key = sensor->Name;
                    }

                    // 添加到对应的组
                    if (!tempGroups->ContainsKey(key)) {
                        tempGroups->Add(key, gcnew List<float>());
                    }
                    tempGroups[key]->Add(value);
                }
            }
        }

        // 计算每组的平均温度
        for each (System::Collections::Generic::KeyValuePair<String^, List<float>^> pair in tempGroups) {
            float average = 0;
            for each (float temp in pair.Value) {
                average += temp;
            }
            average /= pair.Value->Count;

            results->Add(gcnew Tuple<String^, float>(
                pair.Key,
                average
            ));
        }

        return results;
    }
};

bool LibreHardwareMonitorBridge::initialized = false;

void LibreHardwareMonitorBridge::Initialize() {
    if (!initialized) {
        SensorMonitor::Initialize();
        initialized = true;
    }
}

std::vector<std::pair<std::string, float>> LibreHardwareMonitorBridge::GetTemperatures() {
    std::vector<std::pair<std::string, float>> results;

    auto managedResults = SensorMonitor::GetTemperatures();
    for each(Tuple<String^, float> ^ entry in managedResults) {
        std::string name = marshal_as<std::string>(entry->Item1);
        results.emplace_back(std::make_pair(name, entry->Item2));
    }

    return results;
}
