#include "HistoryLogger.h"
#include <sqlite3.h>
#include <chrono>
#include <cstring>
#include <sstream>
#include <algorithm>

HistoryLogger::HistoryLogger() = default;

HistoryLogger::~HistoryLogger() {
    Shutdown();
}

bool HistoryLogger::Initialize(const std::string& dbPath) {
    if (running_.load()) {
        return false; // already running
    }

    dbPath_ = dbPath;

    // Open database
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath_.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }

    // Enable WAL mode for better concurrent performance
    char* errMsg = nullptr;
    if (sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        if (errMsg) {
            sqlite3_free(errMsg);
        }
    }

    // Create tables
    CreateTables();

    // Rotate old data
    RotateIfNeeded();

    sqlite3_close(db);

    // Start background worker thread
    running_.store(true);
    worker_ = std::thread(&HistoryLogger::RunLoop, this);

    return true;
}

void HistoryLogger::Shutdown() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    if (worker_.joinable()) {
        worker_.join();
    }

    // Flush any remaining items
    std::vector<SensorSnapshot> remaining;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        remaining.swap(pending_);
    }

    if (!remaining.empty()) {
        FlushBatch(remaining);
    }
}

void HistoryLogger::WriteBatch(const std::vector<SensorSnapshot>& batch) {
    if (!running_.load()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        pending_.insert(pending_.end(), batch.begin(), batch.end());
    }
}

void HistoryLogger::RunLoop() {
    // 1-second tick loop: dequeue pending items and flush to DB
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::vector<SensorSnapshot> local;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (pending_.empty()) {
                continue;
            }
            local.swap(pending_);
        }

        FlushBatch(local);
    }
}

void HistoryLogger::CreateTables() {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath_.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return;
    }

    const char* createTableSQL =
        "CREATE TABLE IF NOT EXISTS sensors ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  value REAL,"
        "  units TEXT,"
        "  timestamp_ms INTEGER NOT NULL"
        ");";

    const char* createIndexSQL =
        "CREATE INDEX IF NOT EXISTS idx_sensors_name_ts "
        "ON sensors(name, timestamp_ms);";

    char* errMsg = nullptr;
    if (sqlite3_exec(db, createTableSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        if (errMsg) {
            sqlite3_free(errMsg);
            errMsg = nullptr;
        }
    }

    if (sqlite3_exec(db, createIndexSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        if (errMsg) {
            sqlite3_free(errMsg);
        }
    }

    sqlite3_close(db);
}

void HistoryLogger::FlushBatch(const std::vector<SensorSnapshot>& batch) {
    if (batch.empty()) {
        return;
    }

    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath_.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return;
    }

    // Use a transaction for batch insert performance
    char* errMsg = nullptr;
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg);
    if (errMsg) {
        sqlite3_free(errMsg);
        errMsg = nullptr;
    }

    const char* insertSQL =
        "INSERT INTO sensors (name, value, units, timestamp_ms) "
        "VALUES (?1, ?2, ?3, ?4);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, insertSQL, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return;
    }

    for (const auto& snapshot : batch) {
        sqlite3_bind_text(stmt, 1, snapshot.name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 2, snapshot.value);
        sqlite3_bind_text(stmt, 3, snapshot.units.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(snapshot.timestampMs));

        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errMsg);
    if (errMsg) {
        sqlite3_free(errMsg);
    }

    sqlite3_close(db);
}

void HistoryLogger::RotateIfNeeded() {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath_.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    int64_t cutoffMs = nowMs - retentionDays_ * 86400000LL;

    std::ostringstream sql;
    sql << "DELETE FROM sensors WHERE timestamp_ms < " << cutoffMs << ";";

    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql.str().c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        if (errMsg) {
            sqlite3_free(errMsg);
        }
    }

    // Vacuum to reclaim space after deletion
    sqlite3_exec(db, "VACUUM;", nullptr, nullptr, &errMsg);
    if (errMsg) {
        sqlite3_free(errMsg);
    }

    sqlite3_close(db);
}
