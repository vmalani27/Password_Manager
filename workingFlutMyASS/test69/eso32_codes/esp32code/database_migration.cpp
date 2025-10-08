#include "database_migration.h"

bool initializeDatabase(sqlite3* db) {
    if (!db) {
        Serial.println("[DB_MIGRATION] Invalid database pointer");
        return false;
    }
    
    Serial.println("[DB_MIGRATION] Initializing database schema...");
    
    // Create schema version table if it doesn't exist
    const char* schema_version_sql = 
        "CREATE TABLE IF NOT EXISTS " SCHEMA_VERSION_TABLE " ("
        "version INTEGER PRIMARY KEY"
        ");";
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, schema_version_sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        Serial.printf("[DB_MIGRATION] Failed to create schema version table: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    
    // Get current schema version
    int currentVersion = getCurrentSchemaVersion(db);
    Serial.printf("[DB_MIGRATION] Current schema version: %d\n", currentVersion);
    
    if (currentVersion == 0) {
        // Fresh database - check if plaintext credentials table exists
        const char* check_sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='credentials';";
        sqlite3_stmt* stmt;
        rc = sqlite3_prepare_v2(db, check_sql, -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                // Plaintext table exists
                Serial.println("[DB_MIGRATION] Found existing plaintext credentials table");
                currentVersion = PLAINTEXT_SCHEMA_VERSION;
                setSchemaVersion(db, currentVersion);
            } else {
                // No existing table, start with encrypted schema
                Serial.println("[DB_MIGRATION] Fresh database, creating encrypted schema");
                currentVersion = CURRENT_SCHEMA_VERSION;
            }
            sqlite3_finalize(stmt);
        }
    }
    
    // Handle migrations
    if (currentVersion < CURRENT_SCHEMA_VERSION) {
        if (currentVersion == PLAINTEXT_SCHEMA_VERSION) {
            Serial.println("[DB_MIGRATION] Migrating from plaintext to encrypted storage...");
            if (!migrateToEncryptedStorage(db)) {
                Serial.println("[DB_MIGRATION] Migration failed!");
                return false;
            }
        }
        
        // Update schema version
        if (!setSchemaVersion(db, CURRENT_SCHEMA_VERSION)) {
            Serial.println("[DB_MIGRATION] Failed to update schema version");
            return false;
        }
    }
    
    // Ensure encrypted credentials table exists
    if (!createEncryptedCredentialsTable(db)) {
        Serial.println("[DB_MIGRATION] Failed to create encrypted credentials table");
        return false;
    }
    
    // Create audit log table (unchanged)
    const char* audit_sql = 
        "CREATE TABLE IF NOT EXISTS audit_log ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "timestamp INTEGER, "
        "event TEXT, "
        "user TEXT"
        ");";
    
    rc = sqlite3_exec(db, audit_sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        Serial.printf("[DB_MIGRATION] Failed to create audit log table: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    
    Serial.println("[DB_MIGRATION] ✅ Database initialization complete");
    return true;
}

int getCurrentSchemaVersion(sqlite3* db) {
    if (!db) return 0;
    
    const char* sql = "SELECT version FROM " SCHEMA_VERSION_TABLE " LIMIT 1;";
    sqlite3_stmt* stmt;
    int version = 0;
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            version = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    return version;
}

bool setSchemaVersion(sqlite3* db, int version) {
    if (!db) return false;
    
    const char* sql = "INSERT OR REPLACE INTO " SCHEMA_VERSION_TABLE " (version) VALUES (?);";
    sqlite3_stmt* stmt;
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        Serial.printf("[DB_MIGRATION] Failed to prepare schema version update: %s\n", sqlite3_errmsg(db));
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, version);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        Serial.printf("[DB_MIGRATION] Failed to set schema version: %s\n", sqlite3_errmsg(db));
        return false;
    }
    
    Serial.printf("[DB_MIGRATION] Schema version set to %d\n", version);
    return true;
}

bool createEncryptedCredentialsTable(sqlite3* db) {
    if (!db) return false;
    
    const char* sql = 
        "CREATE TABLE IF NOT EXISTS credentials_encrypted ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "site_encrypted TEXT NOT NULL, "
        "site_iv TEXT NOT NULL, "
        "site_tag TEXT NOT NULL, "
        "username_encrypted TEXT NOT NULL, "
        "username_iv TEXT NOT NULL, "
        "username_tag TEXT NOT NULL, "
        "password_encrypted TEXT NOT NULL, "
        "password_iv TEXT NOT NULL, "
        "password_tag TEXT NOT NULL, "
        "created_at INTEGER DEFAULT (strftime('%s', 'now'))"
        ");";
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        Serial.printf("[DB_MIGRATION] Failed to create encrypted credentials table: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    
    Serial.println("[DB_MIGRATION] ✅ Encrypted credentials table ready");
    return true;
}

bool migrateToEncryptedStorage(sqlite3* db) {
    if (!db) return false;
    
    Serial.println("[DB_MIGRATION] Starting migration to encrypted storage...");
    
    // Check if we have plaintext credentials to migrate
    if (!hasPlaintextCredentials(db)) {
        Serial.println("[DB_MIGRATION] No plaintext credentials to migrate");
        return true;
    }
    
    // Backup plaintext data first
    if (!backupPlaintextCredentials(db)) {
        Serial.println("[DB_MIGRATION] Failed to backup plaintext credentials");
        return false;
    }
    
    // Get master key for encryption
    uint8_t masterKey[MASTER_KEY_SIZE];
    if (!getMasterKey(masterKey)) {
        Serial.println("[DB_MIGRATION] Failed to get master key for migration");
        return false;
    }
    
    // Create encrypted table
    if (!createEncryptedCredentialsTable(db)) {
        Serial.println("[DB_MIGRATION] Failed to create encrypted table");
        secureZero(masterKey, sizeof(masterKey));
        return false;
    }
    
    // Migrate each credential
    const char* select_sql = "SELECT id, site, username, password FROM credentials;";
    sqlite3_stmt* select_stmt;
    
    int rc = sqlite3_prepare_v2(db, select_sql, -1, &select_stmt, nullptr);
    if (rc != SQLITE_OK) {
        Serial.printf("[DB_MIGRATION] Failed to prepare select statement: %s\n", sqlite3_errmsg(db));
        secureZero(masterKey, sizeof(masterKey));
        return false;
    }
    
    int migrated_count = 0;
    int error_count = 0;
    
    while (sqlite3_step(select_stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(select_stmt, 0);
        const char* site = (const char*)sqlite3_column_text(select_stmt, 1);
        const char* username = (const char*)sqlite3_column_text(select_stmt, 2);
        const char* password = (const char*)sqlite3_column_text(select_stmt, 3);
        
        if (!site || !username || !password) {
            Serial.printf("[DB_MIGRATION] Skipping record %d with null fields\n", id);
            error_count++;
            continue;
        }
        
        // Encrypt each field
        String site_encrypted, site_iv, site_tag;
        String username_encrypted, username_iv, username_tag;
        String password_encrypted, password_iv, password_tag;
        
        bool encrypt_success = true;
        encrypt_success &= encryptCredentialField(String(site), masterKey, CONTEXT_SITE, 
                                                 site_encrypted, site_iv, site_tag);
        encrypt_success &= encryptCredentialField(String(username), masterKey, CONTEXT_USERNAME,
                                                 username_encrypted, username_iv, username_tag);
        encrypt_success &= encryptCredentialField(String(password), masterKey, CONTEXT_PASSWORD,
                                                 password_encrypted, password_iv, password_tag);
        
        if (!encrypt_success) {
            Serial.printf("[DB_MIGRATION] Failed to encrypt record %d\n", id);
            error_count++;
            continue;
        }
        
        // Insert encrypted record
        const char* insert_sql = 
            "INSERT INTO credentials_encrypted "
            "(site_encrypted, site_iv, site_tag, "
            " username_encrypted, username_iv, username_tag, "
            " password_encrypted, password_iv, password_tag) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
        
        sqlite3_stmt* insert_stmt;
        rc = sqlite3_prepare_v2(db, insert_sql, -1, &insert_stmt, nullptr);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(insert_stmt, 1, site_encrypted.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insert_stmt, 2, site_iv.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insert_stmt, 3, site_tag.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insert_stmt, 4, username_encrypted.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insert_stmt, 5, username_iv.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insert_stmt, 6, username_tag.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insert_stmt, 7, password_encrypted.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insert_stmt, 8, password_iv.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insert_stmt, 9, password_tag.c_str(), -1, SQLITE_TRANSIENT);
            
            if (sqlite3_step(insert_stmt) == SQLITE_DONE) {
                migrated_count++;
                Serial.printf("[DB_MIGRATION] Migrated record %d: %s\n", id, site);
            } else {
                Serial.printf("[DB_MIGRATION] Failed to insert encrypted record %d: %s\n", 
                             id, sqlite3_errmsg(db));
                error_count++;
            }
            sqlite3_finalize(insert_stmt);
        } else {
            Serial.printf("[DB_MIGRATION] Failed to prepare insert statement: %s\n", sqlite3_errmsg(db));
            error_count++;
        }
    }
    
    sqlite3_finalize(select_stmt);
    secureZero(masterKey, sizeof(masterKey));
    
    Serial.printf("[DB_MIGRATION] Migration complete: %d migrated, %d errors\n", 
                 migrated_count, error_count);
    
    if (error_count == 0 && migrated_count > 0) {
        // Verify encrypted data can be decrypted
        if (verifyEncryptedData(db)) {
            // Rename old table to backup
            const char* rename_sql = "ALTER TABLE credentials RENAME TO credentials_plaintext_backup;";
            rc = sqlite3_exec(db, rename_sql, nullptr, nullptr, nullptr);
            if (rc == SQLITE_OK) {
                Serial.println("[DB_MIGRATION] ✅ Plaintext table backed up successfully");
            }
            
            // Rename encrypted table to main table
            const char* rename_new_sql = "ALTER TABLE credentials_encrypted RENAME TO credentials;";
            rc = sqlite3_exec(db, rename_new_sql, nullptr, nullptr, nullptr);
            if (rc == SQLITE_OK) {
                Serial.println("[DB_MIGRATION] ✅ Encrypted table activated successfully");
                return true;
            } else {
                Serial.printf("[DB_MIGRATION] Failed to activate encrypted table: %s\n", sqlite3_errmsg(db));
            }
        } else {
            Serial.println("[DB_MIGRATION] Encrypted data verification failed");
        }
    }
    
    return false;
}

bool hasPlaintextCredentials(sqlite3* db) {
    if (!db) return false;
    
    const char* sql = "SELECT COUNT(*) FROM credentials WHERE 1=1;";
    sqlite3_stmt* stmt;
    bool has_data = false;
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int count = sqlite3_column_int(stmt, 0);
            has_data = (count > 0);
        }
        sqlite3_finalize(stmt);
    }
    
    return has_data;
}

bool backupPlaintextCredentials(sqlite3* db) {
    if (!db) return false;
    
    Serial.println("[DB_MIGRATION] Creating backup of plaintext credentials...");
    
    const char* sql = 
        "CREATE TABLE IF NOT EXISTS credentials_backup_" 
        "AS SELECT * FROM credentials;";
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        Serial.printf("[DB_MIGRATION] Backup failed: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    
    Serial.println("[DB_MIGRATION] ✅ Plaintext backup created");
    return true;
}

bool verifyEncryptedData(sqlite3* db) {
    if (!db) return false;
    
    Serial.println("[DB_MIGRATION] Verifying encrypted data...");
    
    // Get master key
    uint8_t masterKey[MASTER_KEY_SIZE];
    if (!getMasterKey(masterKey)) {
        Serial.println("[DB_MIGRATION] Failed to get master key for verification");
        return false;
    }
    
    // Try to decrypt one record
    const char* sql = 
        "SELECT site_encrypted, site_iv, site_tag FROM credentials_encrypted LIMIT 1;";
    sqlite3_stmt* stmt;
    bool verification_passed = false;
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* site_encrypted = (const char*)sqlite3_column_text(stmt, 0);
            const char* site_iv = (const char*)sqlite3_column_text(stmt, 1);
            const char* site_tag = (const char*)sqlite3_column_text(stmt, 2);
            
            if (site_encrypted && site_iv && site_tag) {
                String decrypted;
                verification_passed = decryptCredentialField(
                    String(site_encrypted), String(site_iv), String(site_tag),
                    masterKey, CONTEXT_SITE, decrypted
                );
                
                if (verification_passed) {
                    Serial.printf("[DB_MIGRATION] ✅ Verification passed, decrypted: %s\n", decrypted.c_str());
                } else {
                    Serial.println("[DB_MIGRATION] ❌ Verification failed - decryption error");
                }
            }
        }
        sqlite3_finalize(stmt);
    }
    
    secureZero(masterKey, sizeof(masterKey));
    return verification_passed;
}

void getMigrationStatus(sqlite3* db, String& status) {
    status = "Database Migration Status:\n";
    
    if (!db) {
        status += "- Database: Not connected\n";
        return;
    }
    
    status += "- Current Schema Version: " + String(getCurrentSchemaVersion(db)) + "\n";
    status += "- Target Schema Version: " + String(CURRENT_SCHEMA_VERSION) + "\n";
    
    // Check table existence
    const char* tables[] = {"credentials", "credentials_encrypted", "credentials_backup"};
    for (int i = 0; i < 3; i++) {
        const char* check_sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?;";
        sqlite3_stmt* stmt;
        bool exists = false;
        
        int rc = sqlite3_prepare_v2(db, check_sql, -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, tables[i], -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                exists = true;
            }
            sqlite3_finalize(stmt);
        }
        
        status += "- Table " + String(tables[i]) + ": " + (exists ? "Exists" : "Missing") + "\n";
    }
}