#include "db_manager.h"
#include <time.h>

// Singleton instance
DBManager& DBManager::getInstance() {
    static DBManager instance;
    return instance;
}

// Constructor
DBManager::DBManager() 
    : db(nullptr)
    , zErrMsg(nullptr)
    , rc(0)
    , commandQueue(nullptr)
    , workerTaskHandle(nullptr)
    , workerRunning(false)
{
}

// Destructor
DBManager::~DBManager() {
    stopWorkerTask();
    close();
}

// Initialize database
bool DBManager::begin(const char* dbPath) {
    Serial.println("DBManager: Initializing SQLite database...");
    
    // Initialize SQLite with serialized threading mode for multi-threaded access
    sqlite3_config(SQLITE_CONFIG_SERIALIZED);
    sqlite3_initialize();
    
    // Open database with full mutex protection for thread safety
    rc = sqlite3_open_v2(dbPath, &db, 
                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                         nullptr);
    
    if (rc != SQLITE_OK) {
        Serial.println("DBManager: ERROR - Database open failed: " + String(sqlite3_errmsg(db)));
        return false;
    }
    
    Serial.println("DBManager: Database opened successfully (thread-safe mode)");
    
    // Configure SQLite for SD card safety (NO WAL MODE - unsafe on SD cards)
    // WAL mode requires atomic writes which SD cards cannot guarantee
    // Using DELETE journal mode with FULL synchronous for maximum durability
    
    // Use DELETE journal mode (safe for SD cards, removable media)
    rc = sqlite3_exec(db, "PRAGMA journal_mode = DELETE;", 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        Serial.println("DBManager: WARNING - Journal mode failed: " + String(zErrMsg));
        sqlite3_free(zErrMsg);
        zErrMsg = nullptr;
    } else {
        Serial.println("DBManager: DELETE journal mode enabled (SD card safe)");
    }
    
    // FULL synchronous mode - ensures data reaches SD card before commit returns
    // This is critical for preventing corruption on power loss
    sqlite3_exec(db, "PRAGMA synchronous = FULL;", 0, 0, 0);
    Serial.println("DBManager: FULL synchronous mode (maximum safety)");
    
    // Use memory for temp tables (faster, no SD writes)
    sqlite3_exec(db, "PRAGMA temp_store = MEMORY;", 0, 0, 0);
    
    // Set busy timeout to 5 seconds (wait if database locked)
    sqlite3_busy_timeout(db, 5000);
    Serial.println("DBManager: Busy timeout set to 5 seconds");
    
    // Run integrity check on startup only (not per-operation)
    Serial.println("DBManager: Running startup integrity check...");
    rc = sqlite3_exec(db, "PRAGMA integrity_check;", nullptr, nullptr, &zErrMsg);
    if (rc != SQLITE_OK) {
        Serial.println("DBManager: WARNING - Integrity check failed: " + String(zErrMsg));
        Serial.println("DBManager: Database may be corrupted - consider using db_reset command");
        sqlite3_free(zErrMsg);
        zErrMsg = nullptr;
    } else {
        Serial.println("DBManager: Integrity check passed - database is healthy");
    }
    
    if (!createTables()) {
        Serial.println("DBManager: ERROR - Failed to create tables");
        return false;
    }
    
    Serial.println("DBManager: Database initialized successfully");
    return true;
}

// Create database tables
bool DBManager::createTables() {
    Serial.println("DBManager: Creating database tables...");
    
    const char* create_credentials = R"(
        CREATE TABLE IF NOT EXISTS credentials (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            site TEXT,
            username TEXT,
            encrypted_password BLOB,
            iv BLOB
        );
    )";
    
    const char* create_audit = R"(
        CREATE TABLE IF NOT EXISTS audit_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER,
            event TEXT,
            user TEXT
        );
    )";
    
    sqlite3_exec(db, create_credentials, 0, 0, &zErrMsg);
    if (zErrMsg) {
        Serial.println("DBManager: ERROR creating credentials table: " + String(zErrMsg));
        sqlite3_free(zErrMsg);
        zErrMsg = nullptr;
        return false;
    }
    Serial.println("DBManager: Credentials table created/verified");
    
    sqlite3_exec(db, create_audit, 0, 0, &zErrMsg);
    if (zErrMsg) {
        Serial.println("DBManager: ERROR creating audit_log table: " + String(zErrMsg));
        sqlite3_free(zErrMsg);
        zErrMsg = nullptr;
        return false;
    }
    Serial.println("DBManager: Audit log table created/verified");
    
    return true;
}

// Close database
void DBManager::close() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

// Start worker task
bool DBManager::startWorkerTask() {
    if (workerRunning) {
        Serial.println("DBManager: Worker task already running");
        return true;
    }
    
    // Create command queue (10 commands max)
    commandQueue = xQueueCreate(10, sizeof(DbCommand));
    if (!commandQueue) {
        Serial.println("DBManager: ERROR - Failed to create command queue");
        return false;
    }
    
    // Create worker task (8KB stack, priority 5)
    BaseType_t result = xTaskCreate(
        workerTaskFunction,
        "DB_Worker",
        8192,  // 8KB stack
        this,  // Pass DBManager instance as parameter
        5,     // Priority
        &workerTaskHandle
    );
    
    if (result != pdPASS) {
        Serial.println("DBManager: ERROR - Failed to create worker task");
        vQueueDelete(commandQueue);
        commandQueue = nullptr;
        return false;
    }
    
    workerRunning = true;
    Serial.println("DBManager: Worker task started successfully");
    return true;
}

// Stop worker task
void DBManager::stopWorkerTask() {
    if (!workerRunning) {
        return;
    }
    
    workerRunning = false;
    
    if (workerTaskHandle) {
        vTaskDelete(workerTaskHandle);
        workerTaskHandle = nullptr;
    }
    
    if (commandQueue) {
        vQueueDelete(commandQueue);
        commandQueue = nullptr;
    }
    
    Serial.println("DBManager: Worker task stopped");
}

// Worker task function (runs in dedicated FreeRTOS task)
void DBManager::workerTaskFunction(void* parameter) {
    DBManager* dbMgr = static_cast<DBManager*>(parameter);
    DbCommand cmd;
    
    Serial.println("DBManager: Worker task running");
    
    while (dbMgr->workerRunning) {
        // Wait for command (block for max 100ms)
        if (xQueueReceive(dbMgr->commandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Process command based on type
            switch (cmd.type) {
                case DB_CMD_INSERT:
                    if (cmd.resultFlag) {
                        *cmd.resultFlag = dbMgr->insertCredentialInternal(cmd.site, cmd.username, cmd.password);
                    }
                    break;
                    
                case DB_CMD_UPDATE:
                    if (cmd.resultFlag) {
                        *cmd.resultFlag = dbMgr->updateCredentialInternal(cmd.site, cmd.username, cmd.password);
                    }
                    break;
                    
                case DB_CMD_DELETE:
                    if (cmd.resultFlag) {
                        *cmd.resultFlag = dbMgr->deleteCredentialInternal(cmd.site, cmd.username);
                    }
                    break;
                    
                case DB_CMD_GET:
                    if (cmd.resultString) {
                        *cmd.resultString = dbMgr->getPasswordInternal(cmd.site, cmd.username);
                    }
                    break;
                    
                case DB_CMD_LIST:
                    if (cmd.resultString) {
                        *cmd.resultString = dbMgr->listCredentialsInternal();
                    }
                    break;
                    
                case DB_CMD_AUDIT_LOG:
                    dbMgr->auditLogInternal(cmd.event, cmd.username);  // Reuse username field for user
                    break;
            }
            
            // Notify caller task that operation is complete
            if (cmd.callerTask) {
                xTaskNotifyGive(cmd.callerTask);
            }
        }
    }
    
    Serial.println("DBManager: Worker task exiting");
    vTaskDelete(NULL);
}

// Public API - Insert credential (thread-safe)
bool DBManager::insertCredential(const String& site, const String& username, const String& password) {
    if (!workerRunning || !commandQueue) {
        Serial.println("DBManager: Worker not running, using direct call");
        return insertCredentialInternal(site, username, password);
    }
    
    bool result = false;
    DbCommand cmd;
    cmd.type = DB_CMD_INSERT;
    cmd.site = site;
    cmd.username = username;
    cmd.password = password;
    cmd.callerTask = xTaskGetCurrentTaskHandle();
    cmd.resultFlag = &result;
    cmd.resultString = nullptr;
    
    // Send command to queue
    if (xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(1000)) != pdTRUE) {
        Serial.println("DBManager: Queue full, command dropped");
        return false;
    }
    
    // Wait for worker to complete (max 5 seconds)
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000));
    
    return result;
}

// Public API - Update credential (thread-safe)
bool DBManager::updateCredential(const String& site, const String& username, const String& password) {
    if (!workerRunning || !commandQueue) {
        return updateCredentialInternal(site, username, password);
    }
    
    bool result = false;
    DbCommand cmd;
    cmd.type = DB_CMD_UPDATE;
    cmd.site = site;
    cmd.username = username;
    cmd.password = password;
    cmd.callerTask = xTaskGetCurrentTaskHandle();
    cmd.resultFlag = &result;
    cmd.resultString = nullptr;
    
    if (xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(1000)) != pdTRUE) {
        Serial.println("DBManager: Queue full, command dropped");
        return false;
    }
    
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000));
    return result;
}

// Public API - Delete credential (thread-safe)
bool DBManager::deleteCredential(const String& site, const String& username) {
    if (!workerRunning || !commandQueue) {
        return deleteCredentialInternal(site, username);
    }
    
    bool result = false;
    DbCommand cmd;
    cmd.type = DB_CMD_DELETE;
    cmd.site = site;
    cmd.username = username;
    cmd.callerTask = xTaskGetCurrentTaskHandle();
    cmd.resultFlag = &result;
    cmd.resultString = nullptr;
    
    if (xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(1000)) != pdTRUE) {
        Serial.println("DBManager: Queue full, command dropped");
        return false;
    }
    
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000));
    return result;
}

// Public API - Get password (thread-safe)
String DBManager::getPassword(const String& site, const String& username) {
    if (!workerRunning || !commandQueue) {
        return getPasswordInternal(site, username);
    }
    
    String result = "";
    DbCommand cmd;
    cmd.type = DB_CMD_GET;
    cmd.site = site;
    cmd.username = username;
    cmd.callerTask = xTaskGetCurrentTaskHandle();
    cmd.resultFlag = nullptr;
    cmd.resultString = &result;
    
    if (xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(1000)) != pdTRUE) {
        Serial.println("DBManager: Queue full, command dropped");
        return "";
    }
    
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000));
    return result;
}

// Public API - List credentials (thread-safe)
String DBManager::listCredentials() {
    if (!workerRunning || !commandQueue) {
        return listCredentialsInternal();
    }
    
    String result = "";
    DbCommand cmd;
    cmd.type = DB_CMD_LIST;
    cmd.callerTask = xTaskGetCurrentTaskHandle();
    cmd.resultFlag = nullptr;
    cmd.resultString = &result;
    
    if (xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(1000)) != pdTRUE) {
        Serial.println("DBManager: Queue full, command dropped");
        return "";
    }
    
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000));
    return result;
}

// Public API - Audit log (thread-safe, fire-and-forget)
void DBManager::auditLog(const String& event, const String& user) {
    if (!workerRunning || !commandQueue) {
        auditLogInternal(event, user);
        return;
    }
    
    DbCommand cmd;
    cmd.type = DB_CMD_AUDIT_LOG;
    cmd.event = event;
    cmd.username = user;  // Reuse username field
    cmd.callerTask = nullptr;  // No response needed
    cmd.resultFlag = nullptr;
    cmd.resultString = nullptr;
    
    // Fire and forget - don't wait for completion
    xQueueSend(commandQueue, &cmd, 0);
}

// ============================================================================
// INTERNAL METHODS (CALLED ONLY BY WORKER TASK)
// ============================================================================

// Insert encrypted credential (internal)
bool DBManager::insertCredentialInternal(const String& site, const String& username, const String& password) {
    Serial.println("DBManager: insertCredentialInternal called");
    Serial.println("  Site: " + site);
    Serial.println("  Username: " + username);
    Serial.println("  Task: " + String((uint32_t)xTaskGetCurrentTaskHandle(), HEX));
    
    // Check database connection health
    if (!db) {
        Serial.println("DBManager: ERROR - Database connection is NULL");
        return false;
    }
    
    // Encrypt password before storing
    uint8_t ciphertext[MAX_PASSWORD_ENCRYPTED_SIZE];
    uint8_t iv[IV_SIZE];
    size_t ciphertext_len;
    
    if (!encrypt_password(password, ciphertext, &ciphertext_len, iv)) {
        Serial.println("DBManager: Encryption failed");
        return false;
    }
    
    Serial.println("DBManager: Password encrypted successfully");
    
    // Begin transaction for atomic write (critical for SD card safety)
    Serial.println("DBManager: Beginning transaction...");
    rc = sqlite3_exec(db, "BEGIN IMMEDIATE;", 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        Serial.println("DBManager: BEGIN IMMEDIATE failed: " + String(zErrMsg));
        sqlite3_free(zErrMsg);
        zErrMsg = nullptr;
        return false;
    }
    
    const char* sql = "INSERT INTO credentials (site, username, encrypted_password, iv) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    
    Serial.println("DBManager: Preparing SQL statement...");
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        Serial.println("DBManager: Prepare failed (insert): " + String(sqlite3_errmsg(db)) + " (code: " + String(rc) + ")");
        return false;
    }
    
    Serial.println("DBManager: Binding parameters...");
    sqlite3_bind_text(stmt, 1, site.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 3, ciphertext, ciphertext_len, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 4, iv, IV_SIZE, SQLITE_TRANSIENT);
    
    Serial.println("DBManager: Executing INSERT...");
    rc = sqlite3_step(stmt);
    bool ok = (rc == SQLITE_DONE);
    
    sqlite3_finalize(stmt);
    
    if (!ok) {
        Serial.println("DBManager: Insert step failed: " + String(sqlite3_errmsg(db)) + " (code: " + String(rc) + ")");
        // Rollback transaction on failure
        sqlite3_exec(db, "ROLLBACK;", 0, 0, 0);
        Serial.println("DBManager: Transaction rolled back");
        return false;
    }
    
    // Commit transaction (ensures atomic write to SD card)
    Serial.println("DBManager: Committing transaction...");
    rc = sqlite3_exec(db, "COMMIT;", 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        Serial.println("DBManager: COMMIT failed: " + String(zErrMsg));
        sqlite3_free(zErrMsg);
        zErrMsg = nullptr;
        sqlite3_exec(db, "ROLLBACK;", 0, 0, 0);
        return false;
    }
    
    Serial.println("DBManager: Insert successful (committed to SD card)");
    return true;
}

// Update encrypted credential (internal)
bool DBManager::updateCredentialInternal(const String& site, const String& username, const String& password) {
    uint8_t ciphertext[MAX_PASSWORD_ENCRYPTED_SIZE];
    uint8_t iv[IV_SIZE];
    size_t ciphertext_len;
    
    if (!encrypt_password(password, ciphertext, &ciphertext_len, iv)) {
        Serial.println("DBManager: Encryption failed");
        return false;
    }
    
    // Begin transaction
    rc = sqlite3_exec(db, "BEGIN IMMEDIATE;", 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        Serial.println("DBManager: BEGIN failed (update): " + String(zErrMsg));
        sqlite3_free(zErrMsg);
        zErrMsg = nullptr;
        return false;
    }
    
    const char* sql = "UPDATE credentials SET encrypted_password=?, iv=? WHERE site=? AND username=?;";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Serial.println("DBManager: Prepare failed (update): " + String(sqlite3_errmsg(db)));
        sqlite3_exec(db, "ROLLBACK;", 0, 0, 0);
        return false;
    }
    
    sqlite3_bind_blob(stmt, 1, ciphertext, ciphertext_len, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 2, iv, IV_SIZE, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, site.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, username.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        Serial.println("DBManager: Update step failed: " + String(sqlite3_errmsg(db)));
        sqlite3_exec(db, "ROLLBACK;", 0, 0, 0);
        return false;
    }
    
    // Commit transaction
    rc = sqlite3_exec(db, "COMMIT;", 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        Serial.println("DBManager: COMMIT failed (update): " + String(zErrMsg));
        sqlite3_free(zErrMsg);
        zErrMsg = nullptr;
        sqlite3_exec(db, "ROLLBACK;", 0, 0, 0);
        return false;
    }
    
    return true;
}

// Delete credential (internal)
bool DBManager::deleteCredentialInternal(const String& site, const String& username) {
    // Begin transaction
    rc = sqlite3_exec(db, "BEGIN IMMEDIATE;", 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        Serial.println("DBManager: BEGIN failed (delete): " + String(zErrMsg));
        sqlite3_free(zErrMsg);
        zErrMsg = nullptr;
        return false;
    }
    
    const char* sql = "DELETE FROM credentials WHERE site=? AND username=?;";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Serial.println("DBManager: Prepare failed (delete): " + String(sqlite3_errmsg(db)));
        sqlite3_exec(db, "ROLLBACK;", 0, 0, 0);
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, site.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        Serial.println("DBManager: Delete step failed: " + String(sqlite3_errmsg(db)));
        sqlite3_exec(db, "ROLLBACK;", 0, 0, 0);
        return false;
    }
    
    // Commit transaction
    rc = sqlite3_exec(db, "COMMIT;", 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        Serial.println("DBManager: COMMIT failed (delete): " + String(zErrMsg));
        sqlite3_free(zErrMsg);
        zErrMsg = nullptr;
        sqlite3_exec(db, "ROLLBACK;", 0, 0, 0);
        return false;
    }
    
    return true;
}

// Get and decrypt password (internal)
String DBManager::getPasswordInternal(const String& site, const String& username) {
    const char* sql = "SELECT encrypted_password, iv FROM credentials WHERE site=? AND username=?;";
    sqlite3_stmt* stmt = nullptr;
    String out = "";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Serial.println("DBManager: Prepare failed (select): " + String(sqlite3_errmsg(db)));
        return out;
    }
    
    sqlite3_bind_text(stmt, 1, site.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const void* encrypted_data = sqlite3_column_blob(stmt, 0);
        int encrypted_len = sqlite3_column_bytes(stmt, 0);
        const void* iv_data = sqlite3_column_blob(stmt, 1);
        int iv_len = sqlite3_column_bytes(stmt, 1);
        
        // Validate data before attempting decryption
        if (!encrypted_data || encrypted_len == 0) {
            Serial.printf("DBManager: Empty encrypted data for %s/%s\n", site.c_str(), username.c_str());
            sqlite3_finalize(stmt);
            return out;
        }
        
        if (!iv_data || iv_len != 16) {
            Serial.printf("DBManager: Invalid IV data for %s/%s (length: %d)\n", site.c_str(), username.c_str(), iv_len);
            sqlite3_finalize(stmt);
            return out;
        }
        
        if (encrypted_len % 16 != 0) {
            Serial.printf("DBManager: Invalid encrypted data length for %s/%s (%d bytes, not multiple of 16)\n", 
                         site.c_str(), username.c_str(), encrypted_len);
            sqlite3_finalize(stmt);
            return out;
        }
        
        String decrypted;
        if (decrypt_password((const uint8_t*)encrypted_data, encrypted_len, (const uint8_t*)iv_data, decrypted)) {
            out = decrypted;
        } else {
            Serial.printf("DBManager: Decryption failed for %s/%s - data may be corrupted\n", site.c_str(), username.c_str());
        }
    }
    
    sqlite3_finalize(stmt);
    return out;
}

// List all credentials (internal)
String DBManager::listCredentialsInternal() {
    const char* sql = "SELECT site, username FROM credentials;";
    sqlite3_stmt* stmt = nullptr;
    String out = "";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Serial.println("DBManager: Prepare failed (list): " + String(sqlite3_errmsg(db)));
        return out;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* site = (const char*)sqlite3_column_text(stmt, 0);
        const char* username = (const char*)sqlite3_column_text(stmt, 1);
        out += String(site) + " " + String(username) + "\n";
    }
    
    sqlite3_finalize(stmt);
    return out;
}

// Log audit event (internal)
void DBManager::auditLogInternal(const String& event, const String& user) {
    // Begin transaction (audit logs also need atomicity)
    rc = sqlite3_exec(db, "BEGIN IMMEDIATE;", 0, 0, 0);
    if (rc != SQLITE_OK) {
        Serial.println("DBManager: BEGIN failed (audit): " + String(sqlite3_errmsg(db)));
        return;
    }
    
    const char* sql = "INSERT INTO audit_log (timestamp, event, user) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Serial.println("DBManager: Audit prepare failed: " + String(sqlite3_errmsg(db)));
        sqlite3_exec(db, "ROLLBACK;", 0, 0, 0);
        return;
    }
    
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)time(NULL));
    sqlite3_bind_text(stmt, 2, event.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, user.c_str(), -1, SQLITE_TRANSIENT);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        Serial.println("DBManager: Audit log failed: " + String(sqlite3_errmsg(db)));
        sqlite3_exec(db, "ROLLBACK;", 0, 0, 0);
        return;
    }
    
    // Commit transaction
    sqlite3_exec(db, "COMMIT;", 0, 0, 0);
}
