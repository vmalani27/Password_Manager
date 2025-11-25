#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <secure_core.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

extern "C" {
    #include "sqlite3.h"
}

// ============================================================================
// DATABASE COMMAND TYPES
// ============================================================================

enum DbCommandType {
    DB_CMD_INSERT,
    DB_CMD_UPDATE,
    DB_CMD_DELETE,
    DB_CMD_GET,
    DB_CMD_LIST,
    DB_CMD_AUDIT_LOG
};

struct DbCommand {
    DbCommandType type;
    String site;
    String username;
    String password;
    String event;
    TaskHandle_t callerTask;  // For response notification
    bool* resultFlag;         // Pointer to result boolean
    String* resultString;     // Pointer to result string
};

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
    
    // Start worker task
    bool startWorkerTask();
    void stopWorkerTask();
    
    // Credential operations (thread-safe, enqueue to worker)
    bool insertCredential(const String& site, const String& username, const String& password);
    bool updateCredential(const String& site, const String& username, const String& password);
    bool deleteCredential(const String& site, const String& username);
    String getPassword(const String& site, const String& username);
    String listCredentials();
    
    // Audit logging (thread-safe)
    void auditLog(const String& event, const String& user);
    
    // Worker task function (static for FreeRTOS)
    static void workerTaskFunction(void* parameter);
    
    // Direct database access (UNSAFE - only for internal use by worker task)
    sqlite3* getDatabase() { return db; }
    
private:
    DBManager();
    ~DBManager();
    DBManager(const DBManager&) = delete;
    DBManager& operator=(const DBManager&) = delete;
    
    sqlite3* db;
    char* zErrMsg;
    int rc;
    
    // FreeRTOS queue and task
    QueueHandle_t commandQueue;
    TaskHandle_t workerTaskHandle;
    bool workerRunning;
    
    // Internal database operations (called only by worker task)
    bool createTables();
    bool insertCredentialInternal(const String& site, const String& username, const String& password);
    bool updateCredentialInternal(const String& site, const String& username, const String& password);
    bool deleteCredentialInternal(const String& site, const String& username);
    String getPasswordInternal(const String& site, const String& username);
    String listCredentialsInternal();
    void auditLogInternal(const String& event, const String& user);
};

#endif // DB_MANAGER_H
