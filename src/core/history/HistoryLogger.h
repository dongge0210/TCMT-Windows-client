#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <functional>

struct SensorSnapshot {
    std::string name;       // metric name, e.g. "cpu/usage"
    double value;
    std::string units;      // e.g. "%", "C", "MHz"
    uint64_t timestampMs;   // unix timestamp ms
};

class HistoryLogger {
public:
    HistoryLogger();
    ~HistoryLogger();

    bool Initialize(const std::string& dbPath);
    void Shutdown();
    bool IsRunning() const { return running_.load(); }

    // Thread-safe: push a batch of sensor readings
    void WriteBatch(const std::vector<SensorSnapshot>& batch);

private:
    void RunLoop();         // background thread: dequeue, flush to DB
    void CreateTables();     // CREATE TABLE IF NOT EXISTS
    void FlushBatch(const std::vector<SensorSnapshot>& batch);
    void RotateIfNeeded();   // delete data older than retention

    std::string dbPath_;
    std::atomic<bool> running_{false};
    std::thread worker_;
    std::mutex queueMutex_;
    std::vector<SensorSnapshot> pending_;
    size_t maxPending_ = 1000;   // max items before forced flush
    int64_t retentionDays_ = 7; // auto-delete data older than this
};
