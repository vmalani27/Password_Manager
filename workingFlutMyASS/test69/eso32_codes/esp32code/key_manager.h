#ifndef KEY_MANAGER_H
#define KEY_MANAGER_H

#include <Arduino.h>
#include <nvs_flash.h>
#include <nvs.h>
#include "encryption_utils.h"

// NVS namespace for key storage
#define NVS_NAMESPACE "esp32_keys"
#define MASTER_KEY_NVS_KEY "master_key"
#define KEY_INITIALIZED_FLAG "key_init"

/**
 * Initialize the key management system
 * Must be called once during setup before any key operations
 */
bool initKeyManager();

/**
 * Check if a master key already exists in NVS storage
 * @return true if master key exists, false otherwise
 */
bool masterKeyExists();

/**
 * Generate and store a new master key in NVS
 * This will overwrite any existing master key
 * @return true if generation and storage successful, false otherwise
 */
bool generateAndStoreMasterKey();

/**
 * Load the master key from NVS storage
 * @param key Buffer to store the loaded key (must be MASTER_KEY_SIZE bytes)
 * @return true if key loaded successfully, false otherwise
 */
bool loadMasterKey(uint8_t* key);

/**
 * Store a master key in NVS (used for key import/backup scenarios)
 * @param key The master key to store (MASTER_KEY_SIZE bytes)
 * @return true if storage successful, false otherwise
 */
bool storeMasterKey(const uint8_t* key);

/**
 * Delete the master key from NVS storage
 * This will make all encrypted data unrecoverable
 * @return true if deletion successful, false otherwise
 */
bool deleteMasterKey();

/**
 * Get the master key for encryption operations
 * Automatically generates and stores a key if none exists
 * @param key Buffer to store the master key (must be MASTER_KEY_SIZE bytes)
 * @return true if master key obtained successfully, false otherwise
 */
bool getMasterKey(uint8_t* key);

/**
 * Verify the integrity of the stored master key
 * Performs basic sanity checks on the key
 * @return true if key appears valid, false otherwise
 */
bool verifyMasterKey();

/**
 * Get key manager status information for debugging
 * @param status String to store status information
 */
void getKeyManagerStatus(String& status);

/**
 * Initialize first-time setup if no master key exists
 * Called automatically during system initialization
 * @return true if setup successful, false otherwise
 */
bool ensureMasterKeyInitialized();

#endif // KEY_MANAGER_H