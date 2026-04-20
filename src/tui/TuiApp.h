#pragma once

#include "LogBuffer.h"
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>

// Forward declarations for ncurses
struct _win_st;
typedef struct _win_st WINDOW;

namespace tcmt {

// Data snapshot for TUI rendering (filled by main thread)
struct TuiData {
    // CPU
    std::string cpuName;
    double cpuUsage = 0.0;
    int physicalCores = 0;
    int performanceCores = 0;
    int efficiencyCores = 0;
    double pCoreFreq = 0.0;
    double eCoreFreq = 0.0;
    double cpuTemp = 0.0;

    // Memory
    uint64_t totalMemory = 0;
    uint64_t usedMemory = 0;
    uint64_t availableMemory = 0;

    // GPU
    std::string gpuName;
    uint64_t gpuMemory = 0;
    double gpuUsage = 0.0;
    double gpuTemp = 0.0;

    // Disk
    struct DiskInfo {
        std::string label;
        uint64_t totalSize = 0;
        uint64_t usedSpace = 0;
        std::string fileSystem;
    };
    std::vector<DiskInfo> disks;

    // Network
    struct NetInfo {
        std::string name;
        std::string ip;
        std::string type;
        uint64_t speed = 0;
    };
    std::vector<NetInfo> adapters;

    // OS
    std::string osVersion;

    // Temperatures
    std::vector<std::pair<std::string, double>> temperatures;

    // Timestamp
    std::string timestamp;
};

class TuiApp {
public:
    TuiApp();
    ~TuiApp();

    // Start/stop the TUI (runs in its own thread)
    void Start();
    void Stop();
    bool IsRunning() const;

    // Update data from main thread (thread-safe)
    void UpdateData(const TuiData& data);

    // Get the log buffer for Logger to write into
    LogBuffer& GetLogBuffer();

private:
    void Run();
    void InitColors();
    void DrawHeader(WINDOW* win);
    void DrawCpuPanel(WINDOW* win, const TuiData& data);
    void DrawMemoryPanel(WINDOW* win, const TuiData& data);
    void DrawGpuPanel(WINDOW* win, const TuiData& data);
    void DrawDiskPanel(WINDOW* win, const TuiData& data);
    void DrawNetworkPanel(WINDOW* win, const TuiData& data);
    void DrawTempPanel(WINDOW* win, const TuiData& data);
    void DrawLogPanel(WINDOW* win);

    // Utility
    static std::string FormatSize(uint64_t bytes);
    static std::string FormatSpeed(uint64_t bps);
    static std::string FormatBar(double pct, int width);
    static std::string TrimRight(const std::string& s, size_t maxLen);

    std::thread thread_;
    std::atomic<bool> running_{false};

    TuiData data_;
    mutable std::mutex dataMutex_;

    LogBuffer logBuffer_;

    // Window dimensions
    int termRows_ = 0;
    int termCols_ = 0;

    // Log panel scroll offset
    int logScrollOffset_ = 0;
};

} // namespace tcmt
