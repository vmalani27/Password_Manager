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
{
}

// Destructor
DBManager::~DBManager() {
    close();
}

// Initialize database
bool DBManager::begin(const char* dbPath) {
    Serial.println("DBManager: Initializing SQLite database...");
    
    sqlite3_initialize();
    rc = sqlite3_open(dbPath, &db);
    
    if (rc != SQLITE_OK) {
        Serial.println("DBManager: ERROR - Database open failed: " + String(sqlite3_errmsg(db)));
        return false;
    }
    
    Serial.println("DBManager: Database opened successfully");
    
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

// Insert encrypted credential
bool DBManager::insertCredential(const String& site, const String& username, const String& password) {
    // Encrypt password before storing
    uint8_t ciphertext[MAX_PASSWORD_ENCRYPTED_SIZE];
    uint8_t iv[IV_SIZE];
    size_t ciphertext_len;
    
    if (!encrypt_password(password, ciphertext, &ciphertext_len, iv)) {
        Serial.println("DBManager: Encryption failed");
        return false;
    }
    
    const char* sql = "INSERT INTO credentials (site, username, encrypted_password, iv) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Serial.println("DBManager: Prepare failed (insert): " + String(sqlite3_errmsg(db)));
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, site.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 3, ciphertext, ciphertext_len, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 4, iv, IV_SIZE, SQLITE_TRANSIENT);
    
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok) {
        Serial.println("DBManager: Insert step failed: " + String(sqlite3_errmsg(db)));
    }
    
    sqlite3_finalize(stmt);
    return ok;
}

// Update encrypted credential
bool DBManager::updateCredential(const String& site, const String& username, const String& password) {
    uint8_t ciphertext[MAX_PASSWORD_ENCRYPTED_SIZE];
    uint8_t iv[IV_SIZE];
    size_t ciphertext_len;
    
    if (!encrypt_password(password, ciphertext, &ciphertext_len, iv)) {
        Serial.println("DBManager: Encryption failed");
        return false;
    }
    
    const char* sql = "UPDATE credentials SET encrypted_password=?, iv=? WHERE site=? AND username=?;";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Serial.println("DBManager: Prepare failed (update): " + String(sqlite3_errmsg(db)));
        return false;
    }
    
    sqlite3_bind_blob(stmt, 1, ciphertext, ciphertext_len, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 2, iv, IV_SIZE, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, site.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, username.c_str(), -1, SQLITE_TRANSIENT);
    
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok) {
        Serial.println("DBManager: Update step failed: " + String(sqlite3_errmsg(db)));
    }
    
    sqlite3_finalize(stmt);
    return ok;
}

// Delete credential
bool DBManager::deleteCredential(const String& site, const String& username) {
    const char* sql = "DELETE FROM credentials WHERE site=? AND username=?;";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Serial.println("DBManager: Prepare failed (delete): " + String(sqlite3_errmsg(db)));
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, site.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
    
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok) {
        Serial.println("DBManager: Delete step failed: " + String(sqlite3_errmsg(db)));
    }
    
    sqlite3_finalize(stmt);
    return ok;
}

// Get and decrypt password
String DBManager::getPassword(const String& site, const String& username) {
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
        
        if (encrypted_data && iv_data) {
            String decrypted;
            if (decrypt_password((const uint8_t*)encrypted_data, encrypted_len, (const uint8_t*)iv_data, decrypted)) {
                out = decrypted;
            }
        }
    }
    
    sqlite3_finalize(stmt);
    return out;
}

// List all credentials
String DBManager::listCredentials() {
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
        out += String(site) + " | " + String(username) + "\\n";
    }
    
    sqlite3_finalize(stmt);
    return out;
}

// Log audit event
void DBManager::auditLog(const String& event, const String& user) {
    const char* sql = "INSERT INTO audit_log (timestamp, event, user) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Serial.println("DBManager: Audit prepare failed: " + String(sqlite3_errmsg(db)));
        return;
    }
    
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)time(NULL));
    sqlite3_bind_text(stmt, 2, event.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, user.c_str(), -1, SQLITE_TRANSIENT);
    
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}
