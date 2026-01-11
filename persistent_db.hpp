#include <unordered_map>
#include <queue>
#include <string>
#include <mutex>
#include <thread>
#include <sqlite3.h>
#include <iostream>

// SQLite persistent storage
class SQLiteDB {
private:
    sqlite3* db;
    mutable std::mutex db_mutex;
    
public:
    SQLiteDB(const std::string& db_path = "cache.db") {
        int rc = sqlite3_open(db_path.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
            db = nullptr;
            return;
        }
        
        // Create table if it doesn't exist
        const char* create_table_sql = 
            "CREATE TABLE IF NOT EXISTS cache_data ("
            "key TEXT PRIMARY KEY,"
            "value TEXT NOT NULL"
            ");";
        
        char* err_msg = nullptr;
        rc = sqlite3_exec(db, create_table_sql, nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL error: " << err_msg << std::endl;
            sqlite3_free(err_msg);
        }
    }
    
    ~SQLiteDB() {
        if (db) {
            sqlite3_close(db);
        }
    }
    
    bool put_to_db(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(db_mutex);

        if(!db) return false;
        
        const char* sql = "INSERT OR REPLACE INTO cache_data (key, value) VALUES (?, ?);";
        sqlite3_stmt* stmt;
        
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
        
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        return rc == SQLITE_DONE;
    }
    
    std::pair<bool, std::string> get_from_db(const std::string& key) {
        std::lock_guard<std::mutex> lock(db_mutex);

        if(!db) return {false, ""};
        
        const char* sql = "SELECT value FROM cache_data WHERE key = ?;";
        sqlite3_stmt* stmt;
        
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed: " << sqlite3_errmsg(db) << std::endl;
            return {false, ""};
        }
        
        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
        
        std::pair<bool, std::string> result = {false, ""};
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* value = sqlite3_column_text(stmt, 0);
            if (value) {
                result = {true, std::string(reinterpret_cast<const char*>(value))};
            }
        }
        
        sqlite3_finalize(stmt);
        return result;
    }
    
    bool remove_from_db(const std::string& key) {
        std::lock_guard<std::mutex> lock(db_mutex);
        
        const char* sql = "DELETE FROM cache_data WHERE key = ?;";
        sqlite3_stmt* stmt;
        
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
        
        rc = sqlite3_step(stmt);
        int changes = sqlite3_changes(db);
        sqlite3_finalize(stmt);
        
        return rc == SQLITE_DONE && changes > 0;
    }
};