#include "HistoryLogger.h"
#include <sqlite3.h>
#include <chrono>
#include <cstring>
#include <sstream>

HistoryLogger::HistoryLogger() = default;

HistoryLogger::~HistoryLogger() {
    Shutdown();
}

bool HistoryLogger::Initialize(const std::string& dbPath) {
    if (running_.load()) return false;

    dbPath_ = dbPath;

    auto db = static_cast<sqlite3*>(db_);
    if (sqlite3_open(dbPath_.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }
    db_ = db;

    // WAL mode for better concurrent performance
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

    CreateTables();
    RotateIfNeeded();

    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    lastRotateCheckMs_ = nowMs;

    running_.store(true);
    worker_ = std::thread(&HistoryLogger::RunLoop, this);
    return true;
}

void HistoryLogger::Shutdown() {
    if (!running_.load()) return;
    running_.store(false);
    queueCv_.notify_all();

    if (worker_.joinable())
        worker_.join();

    // Flush remaining
    std::vector<SensorSnapshot> remaining;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        remaining.swap(pending_);
    }
    if (!remaining.empty())
        FlushBatch(remaining);

    if (db_) {
        sqlite3_close(static_cast<sqlite3*>(db_));
        db_ = nullptr;
    }
}

void HistoryLogger::WriteBatch(const std::vector<SensorSnapshot>& batch) {
    if (!running_.load()) return;

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        pending_.insert(pending_.end(), batch.begin(), batch.end());
        if (pending_.size() >= maxPending_)
            queueCv_.notify_one();
    }
}

void HistoryLogger::RunLoop() {
    using namespace std::chrono_literals;
    auto db = static_cast<sqlite3*>(db_);
    if (!db) return;

    while (running_.load()) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        queueCv_.wait_for(lock, 1s);

        std::vector<SensorSnapshot> local;
        local.swap(pending_);
        lock.unlock();

        if (!local.empty())
            FlushBatch(local);

        // Daily rotation check
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (nowMs - lastRotateCheckMs_ > 3600 * 1000) {
            RotateIfNeeded();
            lastRotateCheckMs_ = nowMs;
        }
    }
}

void HistoryLogger::CreateTables() {
    auto db = static_cast<sqlite3*>(db_);
    if (!db) return;

    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS sensors ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  value REAL,"
        "  units TEXT,"
        "  timestamp_ms INTEGER NOT NULL"
        ");", nullptr, nullptr, nullptr);

    sqlite3_exec(db,
        "CREATE INDEX IF NOT EXISTS idx_sensors_name_ts "
        "ON sensors(name, timestamp_ms);", nullptr, nullptr, nullptr);
}

void HistoryLogger::FlushBatch(const std::vector<SensorSnapshot>& batch) {
    if (batch.empty()) return;
    auto db = static_cast<sqlite3*>(db_);
    if (!db) return;

    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    const char* insertSQL =
        "INSERT INTO sensors (name, value, units, timestamp_ms) "
        "VALUES (?1, ?2, ?3, ?4);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, insertSQL, -1, &stmt, nullptr) != SQLITE_OK) return;

    for (const auto& snapshot : batch) {
        sqlite3_bind_text(stmt, 1, snapshot.name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 2, snapshot.value);
        sqlite3_bind_text(stmt, 3, snapshot.units.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(snapshot.timestampMs));
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
}

void HistoryLogger::RotateIfNeeded() {
    auto db = static_cast<sqlite3*>(db_);
    if (!db) return;

    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    int64_t cutoffMs = nowMs - retentionDays_ * 86400000LL;

    std::ostringstream sql;
    sql << "DELETE FROM sensors WHERE timestamp_ms < " << cutoffMs << ";";
    sqlite3_exec(db, sql.str().c_str(), nullptr, nullptr, nullptr);
}
