#ifndef DATABASE_MIGRATION_H
#define DATABASE_MIGRATION_H

#include <Arduino.h>
extern "C" {
  #include "sqlite3.h"
}
#include "key_manager.h"
#include "encryption_utils.h"

// Database schema version tracking
#define SCHEMA_VERSION_TABLE "schema_version"
#define CURRENT_SCHEMA_VERSION 2
#define PLAINTEXT_SCHEMA_VERSION 1

/**
 * Initialize the database with proper schema and handle migrations
 * @param db Pointer to the SQLite database connection
 * @return true if initialization/migration successful, false otherwise
 */
bool initializeDatabase(sqlite3* db);

/**
 * Get the current schema version from the database
 * @param db Pointer to the SQLite database connection
 * @return Current schema version, or 0 if not set
 */
int getCurrentSchemaVersion(sqlite3* db);

/**
 * Set the schema version in the database
 * @param db Pointer to the SQLite database connection
 * @param version Schema version to set
 * @return true if successful, false otherwise
 */
bool setSchemaVersion(sqlite3* db, int version);

/**
 * Create the encrypted credentials table (version 2 schema)
 * @param db Pointer to the SQLite database connection
 * @return true if creation successful, false otherwise
 */
bool createEncryptedCredentialsTable(sqlite3* db);

/**
 * Migrate from plaintext (v1) to encrypted (v2) credentials table
 * @param db Pointer to the SQLite database connection
 * @return true if migration successful, false otherwise
 */
bool migrateToEncryptedStorage(sqlite3* db);

/**
 * Check if there are any plaintext credentials to migrate
 * @param db Pointer to the SQLite database connection
 * @return true if plaintext credentials exist, false otherwise
 */
bool hasPlaintextCredentials(sqlite3* db);

/**
 * Backup plaintext credentials before migration
 * @param db Pointer to the SQLite database connection
 * @return true if backup successful, false otherwise
 */
bool backupPlaintextCredentials(sqlite3* db);

/**
 * Verify that encrypted data can be decrypted correctly
 * @param db Pointer to the SQLite database connection
 * @return true if verification successful, false otherwise
 */
bool verifyEncryptedData(sqlite3* db);

/**
 * Get migration status information for debugging
 * @param db Pointer to the SQLite database connection
 * @param status String to store status information
 */
void getMigrationStatus(sqlite3* db, String& status);

#endif // DATABASE_MIGRATION_H