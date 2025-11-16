#include "DiagnosticsPipeClient.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <algorithm>
// Use nlohmann::json from the bundled single_include in CPP-parsers
#include <nlohmann/json.hpp>

#ifdef PLATFORM_WINDOWS
    #include "../common/WindowsCompat.h"
#else
    #include "../core/common/CrossPlatformCompat.h"
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <errno.h>
    #include <cstring>
    #define INVALID_HANDLE_VALUE -1
    #define GENERIC_READ 0x80000000L
    #define OPEN_EXISTING 0
    #define FILE_ATTRIBUTE_NORMAL 0x80
    #define ERROR_PIPE_BUSY 231
    #define ERROR_FILE_NOT_FOUND 2
    #define ERROR_BROKEN_PIPE 109
    #define GetLastError() errno
    #define ReadFile(handle, buffer, size, bytesRead, overlapped) \
        (*(bytesRead) = read(handle, buffer, size), (*(bytesRead) >= 0))
    #define CloseHandle(handle) close(handle)
#endif

const std::string DiagnosticsPipeClient::PIPE_NAME = "\\\\.\\pipe\\SysMonDiag";

DiagnosticsPipeClient::DiagnosticsPipeClient() 
    : hPipe(INVALID_HANDLE_VALUE), isRunning(false), isConnected(false) {
}

DiagnosticsPipeClient::~DiagnosticsPipeClient() {
    Stop();
}

bool DiagnosticsPipeClient::Start() {
    if (isRunning) {
        return true;
    }
    
    lastError.clear();
    isRunning = true;
    
    try {
        workerThread = std::thread(&DiagnosticsPipeClient::WorkerThread, this);
        return true;
    }
    catch (const std::exception& e) {
        lastError = "Failed to start diagnostic pipe client: " + std::string(e.what());
        isRunning = false;
        return false;
    }
}

void DiagnosticsPipeClient::Stop() {
    if (!isRunning) {
        return;
    }
    
    isRunning = false;
    
    if (workerThread.joinable()) {
        workerThread.join();
    }
    
    Cleanup();
}

void DiagnosticsPipeClient::WorkerThread() {
    while (isRunning) {
        try {
            if (!isConnected) {
                if (ConnectToPipe()) {
                    isConnected = true;
                    std::cout << "Diagnostic pipe connected successfully" << std::endl;
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    continue;
                }
            }
            
            if (!ReadFromPipe()) {
                isConnected = false;
                Cleanup();
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                continue;
            }
        }
        catch (const std::exception& e) {
            HandleError("Worker thread exception: " + std::string(e.what()));
            isConnected = false;
            Cleanup();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
}

bool DiagnosticsPipeClient::ConnectToPipe() {
#ifdef PLATFORM_WINDOWS
    if (!WaitForPipeAvailable(5000)) {
        lastError = "Pipe wait timeout";
        return false;
    }
    
    hPipe = CreateFileA(
        PIPE_NAME.c_str(),       
        GENERIC_READ,           
        0,                      
        nullptr,                  
        OPEN_EXISTING,          
        FILE_ATTRIBUTE_NORMAL,    
        nullptr                   
    );
    
    if (hPipe == INVALID_HANDLE_VALUE) {
        DWORD error = ::GetLastError();
        if (error != ERROR_PIPE_BUSY) {
            std::ostringstream oss;
            oss << "Cannot connect to diagnostic pipe, error code: " << error;
            lastError = oss.str();
            return false;
        }
        
        if (!WaitNamedPipeA(PIPE_NAME.c_str(), 2000)) {
            lastError = "Pipe wait timeout";
            return false;
        }
        
        hPipe = CreateFileA(
            PIPE_NAME.c_str(),
            GENERIC_READ,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        
        if (hPipe == INVALID_HANDLE_VALUE) {
            std::ostringstream oss;
            oss << "Failed to reconnect pipe, error code: " << ::GetLastError();
            lastError = oss.str();
            return false;
        }
    }
    
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(
        hPipe,   
        &mode,    
        nullptr, 
        nullptr 
    )) {
        std::ostringstream oss;
        oss << "Failed to set pipe mode, error code: " << ::GetLastError();
        lastError = oss.str();
        CloseHandle(hPipe);
        hPipe = INVALID_HANDLE_VALUE;
        return false;
    }
    
    return true;
#else
    // Non-Windows platforms: use Unix domain socket or FIFO
    std::string pipePath = "/tmp/SysMonDiag";
    
    hPipe = open(pipePath.c_str(), O_RDONLY | O_NONBLOCK);
    if (hPipe == INVALID_HANDLE_VALUE) {
        lastError = "Cannot connect to diagnostic pipe (FIFO not found)";
        return false;
    }
    
    return true;
#endif
}

bool DiagnosticsPipeClient::ReadFromPipe() {
    if (hPipe == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    constexpr int BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    
#ifdef PLATFORM_WINDOWS
    DWORD bytesRead;
    std::string jsonData;

    while (true) {
        bool success = ReadFile(
            hPipe,       
            buffer,      
            BUFFER_SIZE, 
            &bytesRead, 
            nullptr      
        );
        
        if (!success) {
            DWORD error = ::GetLastError();
            if (error == ERROR_BROKEN_PIPE) {
                lastError = "Pipe disconnected";
            } else {
                std::ostringstream oss;
                oss << "Failed to read pipe data, error code: " << error;
                lastError = oss.str();
            }
            return false;
        }
        
        if (bytesRead == 0) {
            break;
        }
        
        jsonData.append(buffer, bytesRead);
        
        if (bytesRead < BUFFER_SIZE) {
            break;
        }
    }
    
    if (!jsonData.empty()) {
        DiagnosticsPipeSnapshot snapshot;
        if (ParseDiagnosticData(jsonData, snapshot)) {
            if (onSnapshotReceived) {
                onSnapshotReceived(snapshot);
            }
        } else {
            lastError = "Failed to parse diagnostic data";
            return false;
        }
    }
    
    return true;
#else
    ssize_t bytesRead;
    std::string jsonData;

    while (true) {
        bytesRead = read(hPipe, buffer, BUFFER_SIZE);
        
        if (bytesRead < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available right now, try again later
                return true;
            } else {
                lastError = "Failed to read pipe data: " + std::string(strerror(errno));
                return false;
            }
        }
        
        if (bytesRead == 0) {
            break;
        }
        
        jsonData.append(buffer, bytesRead);
        
        if (bytesRead < BUFFER_SIZE) {
            break;
        }
    }
    
    if (!jsonData.empty()) {
        DiagnosticsPipeSnapshot snapshot;
        if (ParseDiagnosticData(jsonData, snapshot)) {
            if (onSnapshotReceived) {
                onSnapshotReceived(snapshot);
            }
        } else {
            lastError = "Failed to parse diagnostic data";
            return false;
        }
    }
    
    return true;
#endif
}

bool DiagnosticsPipeClient::ParseDiagnosticData(const std::string& jsonData, DiagnosticsPipeSnapshot& snapshot) {
    try {
        auto j = nlohmann::json::parse(jsonData);

        if (j.contains("timestamp")) {
            snapshot.timestamp = j["timestamp"].get<uint32_t>();
        }

        if (j.contains("writeSequence")) {
            snapshot.writeSequence = j["writeSequence"].get<uint32_t>();
        }

        if (j.contains("abiVersion")) {
            snapshot.abiVersion = j["abiVersion"].get<uint32_t>();
        }

        if (j.contains("offsets") && j["offsets"].is_object()) {
            auto o = j["offsets"];
            snapshot.offsets.tempSensors = o.value("tempSensors", 0u);
            snapshot.offsets.tempSensorCount = o.value("tempSensorCount", 0u);
            snapshot.offsets.smartDisks = o.value("smartDisks", 0u);
            snapshot.offsets.smartDiskCount = o.value("smartDiskCount", 0u);
            snapshot.offsets.futureReserved = o.value("futureReserved", 0u);
            snapshot.offsets.sharedmemHash = o.value("sharedmemHash", 0u);
            snapshot.offsets.extensionPad = o.value("extensionPad", 0u);
        }

        if (j.contains("logs") && j["logs"].is_array()) {
            for (const auto& it : j["logs"]) {
                if (it.is_string()) snapshot.logs.push_back(it.get<std::string>());
                else snapshot.logs.push_back(it.dump());
            }
        }

        return true;
    }
    catch (const std::exception& e) {
        lastError = "Diagnostic data parsing exception: " + std::string(e.what());
        return false;
    }
}

bool DiagnosticsPipeClient::ParseLogEntries(const std::string& logsArray, DiagnosticsPipeSnapshot& snapshot) {
    try {
        // Use nlohmann::json to parse an array of logs robustly
        auto arr = nlohmann::json::parse(logsArray);
        if (!arr.is_array()) return false;

        for (const auto& it : arr) {
            if (it.is_string()) snapshot.logs.push_back(it.get<std::string>());
            else snapshot.logs.push_back(it.dump());
        }

        return true;
    }
    catch (const std::exception& e) {
        lastError = "Log entries parsing exception: " + std::string(e.what());
        return false;
    }
}

bool DiagnosticsPipeClient::WaitForPipeAvailable(int timeoutMs) {
    auto startTime = std::chrono::steady_clock::now();
    auto timeoutDuration = std::chrono::milliseconds(timeoutMs);
    
#ifdef PLATFORM_WINDOWS
    while (true) {
        if (WaitNamedPipeA(PIPE_NAME.c_str(), 0)) {
            return true;
        }
        
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed >= timeoutDuration) {
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
#else
    std::string pipePath = "/tmp/SysMonDiag";
    
    while (true) {
        struct stat st;
        if (stat(pipePath.c_str(), &st) == 0 && S_ISFIFO(st.st_mode)) {
            return true;
        }
        
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed >= timeoutDuration) {
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
#endif
}

void DiagnosticsPipeClient::HandleError(const std::string& errorMessage) {
    lastError = errorMessage;
    if (onError) {
        onError(errorMessage);
    }
}

void DiagnosticsPipeClient::Cleanup() {
    if (hPipe != INVALID_HANDLE_VALUE) {
#ifdef PLATFORM_WINDOWS
        CloseHandle(hPipe);
#else
        close(hPipe);
#endif
        hPipe = INVALID_HANDLE_VALUE;
    }
    isConnected = false;
}

bool DiagnosticsPipeClient::IsConnected() const {
    return isConnected;
}

std::string DiagnosticsPipeClient::GetLastErrorMessage() const {
    return lastError;
}

void DiagnosticsPipeClient::SetSnapshotCallback(SnapshotCallback callback) {
    onSnapshotReceived = callback;
}

void DiagnosticsPipeClient::SetErrorCallback(ErrorCallback callback) {
    onError = callback;
}

std::string DiagnosticsPipeClient::ExtractJsonValue(const std::string& json, const std::string& key) {
    try {
        // Build search pattern: "key":value
        std::string searchPattern = "\"" + key + "\":";
        size_t pos = json.find(searchPattern);
        
        if (pos == std::string::npos) {
            return "";
        }
        
        // Skip key and colon
        pos += searchPattern.length();
        
        // Skip spaces
        while (pos < json.length() && json[pos] == ' ') {
            ++pos;
        }
        
        // If string value (starts with quote)
        if (pos < json.length() && json[pos] == '"') {
            ++pos; // Skip opening quote
            size_t endPos = json.find('"', pos);
            if (endPos == std::string::npos) {
                return "";
            }
            return json.substr(pos, endPos - pos);
        }
        
        // If numeric or boolean value
        size_t endPos = json.find_first_of(",}", pos);
        if (endPos == std::string::npos) {
            return "";
        }
        
        std::string value = json.substr(pos, endPos - pos);
        
        // Remove leading and trailing spaces
        value.erase(0, value.find_first_not_of(" \t\n\r"));
        value.erase(value.find_last_not_of(" \t\n\r") + 1);
        
        return value;
    }
    catch (const std::exception&) {
        return "";
    }
}

std::string DiagnosticsPipeClient::ExtractJsonArray(const std::string& json, const std::string& arrayKey) {
    try {
        // Build search pattern: "arrayKey":[
        std::string searchPattern = "\"" + arrayKey + "\":[";
        size_t pos = json.find(searchPattern);
        
        if (pos == std::string::npos) {
            return "";
        }
        
        // Skip key, colon and opening bracket
        pos += searchPattern.length();
        
        // Find matching closing bracket
        int bracketCount = 1;
        size_t endPos = pos;
        
        while (endPos < json.length() && bracketCount > 0) {
            if (json[endPos] == '[') {
                ++bracketCount;
            } else if (json[endPos] == ']') {
                --bracketCount;
            }
            ++endPos;
        }
        
        if (bracketCount != 0) {
            return "";
        }
        
        return json.substr(pos, endPos - pos - 1); // -1 excludes closing bracket
    }
    catch (const std::exception&) {
        return "";
    }
}

std::vector<std::string> DiagnosticsPipeClient::SplitString(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    
    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }
    
    return result;
}