#pragma once
#include <string>

struct BoardInfo {
    std::string manufacturer;   // e.g. "ASUSTeK COMPUTER INC."
    std::string product;        // e.g. "ROG STRIX B550-F GAMING"
    std::string serialNumber;   // e.g. "210876543211234"
    std::string version;        // e.g. "Rev X.0x"
    std::string biosVendor;     // e.g. "American Megatrends Inc."
    std::string biosVersion;    // e.g. "2806"
    std::string biosDate;       // e.g. "2024-01-15"
    std::string systemUuid;     // e.g. "550e8400-e29b-41d4-a716-446655440000"
    std::string chassisType;    // e.g. "Desktop", "Notebook"

    void Detect(); // fill all fields
};
