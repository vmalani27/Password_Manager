#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <secure_core.h>

extern "C" {
    #include "sqlite3.h"
}

// ============================================================================
// DATABASE MANAGEMENT
// ============================================================================

class DBManager {
public:
    // Singleton access
    static DBManager& getInstance();
    
    // Initialization
    bool begin(const char* dbPath = "/sd/credentials.db");
    void close();
    
    // Credential operations
    bool insertCredential(const String& site, const String& username, const String& password);
    bool updateCredential(const String& site, const String& username, const String& password);
    bool deleteCredential(const String& site, const String& username);
    String getPassword(const String& site, const String& username);
    String listCredentials();
    
    // Audit logging
    void auditLog(const String& event, const String& user);
    
    // Direct database access (if needed)
    sqlite3* getDatabase() { return db; }
    
private:
    DBManager();
    ~DBManager();
    DBManager(const DBManager&) = delete;
    DBManager& operator=(const DBManager&) = delete;
    
    sqlite3* db;
    char* zErrMsg;
    int rc;
    
    bool createTables();
};

#endif // DB_MANAGER_H
