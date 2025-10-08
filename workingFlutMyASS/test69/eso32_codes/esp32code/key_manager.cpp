#include "key_manager.h"

static bool key_manager_initialized = false;
static nvs_handle_t nvs_handle_storage;

bool initKeyManager() {
    if (key_manager_initialized) {
        return true;
    }
    
    Serial.println("[KEY_MGR] Initializing key management system...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        Serial.println("[KEY_MGR] NVS partition needs to be erased, doing so...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    if (ret != ESP_OK) {
        Serial.printf("[KEY_MGR] Failed to initialize NVS: %s\n", esp_err_to_name(ret));
        return false;
    }
    
    // Open NVS namespace
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle_storage);
    if (ret != ESP_OK) {
        Serial.printf("[KEY_MGR] Failed to open NVS namespace: %s\n", esp_err_to_name(ret));
        return false;
    }
    
    key_manager_initialized = true;
    Serial.println("[KEY_MGR] ✅ Key management system initialized successfully");
    
    // Ensure master key is set up
    return ensureMasterKeyInitialized();
}

bool masterKeyExists() {
    if (!key_manager_initialized && !initKeyManager()) {
        return false;
    }
    
    Serial.println("[KEY_MGR] Checking if master key exists...");
    
    // Use the global handle for consistency
    size_t required_size = 0;
    esp_err_t ret = nvs_get_blob(nvs_handle_storage, MASTER_KEY_NVS_KEY, NULL, &required_size);
    
    bool key_exists = false;
    if (ret == ESP_OK && required_size == MASTER_KEY_SIZE) {
        // Verify the initialization flag as well
        uint8_t init_flag = 0;
        size_t flag_size = sizeof(init_flag);
        ret = nvs_get_blob(nvs_handle_storage, KEY_INITIALIZED_FLAG, &init_flag, &flag_size);
        key_exists = (ret == ESP_OK && init_flag == 1);
    }
    
    Serial.printf("[KEY_MGR] Master key exists check: %s\n", key_exists ? "true" : "false");
    return key_exists;
}

bool generateAndStoreMasterKey() {
    if (!key_manager_initialized && !initKeyManager()) {
        return false;
    }
    
    Serial.println("[KEY_MGR] Generating new master key...");
    
    uint8_t masterKey[MASTER_KEY_SIZE];
    if (!generateMasterKey(masterKey)) {
        Serial.println("[KEY_MGR] Failed to generate master key");
        return false;
    }
    
    // Store the master key
    esp_err_t ret = nvs_set_blob(nvs_handle_storage, MASTER_KEY_NVS_KEY, masterKey, MASTER_KEY_SIZE);
    if (ret != ESP_OK) {
        Serial.printf("[KEY_MGR] Failed to store master key: %s\n", esp_err_to_name(ret));
        secureZero(masterKey, sizeof(masterKey));
        return false;
    }
    
    // Set initialization flag
    uint8_t init_flag = 1;
    ret = nvs_set_blob(nvs_handle_storage, KEY_INITIALIZED_FLAG, &init_flag, sizeof(init_flag));
    if (ret != ESP_OK) {
        Serial.printf("[KEY_MGR] Failed to set initialization flag: %s\n", esp_err_to_name(ret));
        secureZero(masterKey, sizeof(masterKey));
        return false;
    }
    
    // Commit changes to flash
    ret = nvs_commit(nvs_handle_storage);
    if (ret != ESP_OK) {
        Serial.printf("[KEY_MGR] Failed to commit master key to NVS: %s\n", esp_err_to_name(ret));
        secureZero(masterKey, sizeof(masterKey));
        return false;
    }
    
    secureZero(masterKey, sizeof(masterKey));
    Serial.println("[KEY_MGR] ✅ Master key generated and stored successfully");
    return true;
}

bool loadMasterKey(uint8_t* key) {
    if (!key) {
        Serial.println("[KEY_MGR] Invalid key buffer provided");
        return false;
    }
    
    if (!key_manager_initialized && !initKeyManager()) {
        return false;
    }
    
    Serial.println("[KEY_MGR] Loading master key from NVS...");
    
    // First try with the existing global handle (avoid handle conflicts)
    size_t required_size = MASTER_KEY_SIZE;
    esp_err_t ret = nvs_get_blob(nvs_handle_storage, MASTER_KEY_NVS_KEY, key, &required_size);
    
    // If that fails, try reopening the handle
    if (ret != ESP_OK) {
        Serial.printf("[KEY_MGR] Read with global handle failed: %s, trying fresh handle...\n", esp_err_to_name(ret));
        
        // Close and reopen the global handle to ensure fresh state
        nvs_close(nvs_handle_storage);
        delay(10); // Brief delay for NVS state stabilization
        
        ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle_storage);
        if (ret != ESP_OK) {
            Serial.printf("[KEY_MGR] Failed to reopen NVS handle: %s\n", esp_err_to_name(ret));
            key_manager_initialized = false;
            return false;
        }
        
        // Try again with refreshed handle
        required_size = MASTER_KEY_SIZE;
        ret = nvs_get_blob(nvs_handle_storage, MASTER_KEY_NVS_KEY, key, &required_size);
    }
    
    if (ret != ESP_OK) {
        Serial.printf("[KEY_MGR] Failed to load master key: %s\n", esp_err_to_name(ret));
        return false;
    }
    
    if (required_size != MASTER_KEY_SIZE) {
        Serial.printf("[KEY_MGR] Master key size mismatch: expected %d, got %d\n", 
                     MASTER_KEY_SIZE, required_size);
        secureZero(key, MASTER_KEY_SIZE);
        return false;
    }
    
    Serial.println("[KEY_MGR] ✅ Master key loaded successfully from NVS");
    return true;
}

bool storeMasterKey(const uint8_t* key) {
    if (!key) {
        Serial.println("[KEY_MGR] Invalid key provided for storage");
        return false;
    }
    
    if (!key_manager_initialized && !initKeyManager()) {
        return false;
    }
    
    Serial.println("[KEY_MGR] Storing provided master key...");
    
    // Store the master key
    esp_err_t ret = nvs_set_blob(nvs_handle_storage, MASTER_KEY_NVS_KEY, key, MASTER_KEY_SIZE);
    if (ret != ESP_OK) {
        Serial.printf("[KEY_MGR] Failed to store master key: %s\n", esp_err_to_name(ret));
        return false;
    }
    
    // Set initialization flag
    uint8_t init_flag = 1;
    ret = nvs_set_blob(nvs_handle_storage, KEY_INITIALIZED_FLAG, &init_flag, sizeof(init_flag));
    if (ret != ESP_OK) {
        Serial.printf("[KEY_MGR] Failed to set initialization flag: %s\n", esp_err_to_name(ret));
        return false;
    }
    
    // Commit changes to flash
    ret = nvs_commit(nvs_handle_storage);
    if (ret != ESP_OK) {
        Serial.printf("[KEY_MGR] Failed to commit master key to NVS: %s\n", esp_err_to_name(ret));
        return false;
    }
    
    // Add a brief delay to ensure NVS state is stable after commit
    delay(20);
    
    Serial.println("[KEY_MGR] ✅ Master key stored successfully");
    return true;
}

bool deleteMasterKey() {
    if (!key_manager_initialized && !initKeyManager()) {
        return false;
    }
    
    Serial.println("[KEY_MGR] ⚠️ Deleting master key - all encrypted data will be unrecoverable!");
    
    // Delete the master key
    esp_err_t ret = nvs_erase_key(nvs_handle_storage, MASTER_KEY_NVS_KEY);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        Serial.printf("[KEY_MGR] Failed to delete master key: %s\n", esp_err_to_name(ret));
        return false;
    }
    
    // Delete initialization flag
    ret = nvs_erase_key(nvs_handle_storage, KEY_INITIALIZED_FLAG);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        Serial.printf("[KEY_MGR] Failed to delete initialization flag: %s\n", esp_err_to_name(ret));
        return false;
    }
    
    // Commit changes
    ret = nvs_commit(nvs_handle_storage);
    if (ret != ESP_OK) {
        Serial.printf("[KEY_MGR] Failed to commit key deletion: %s\n", esp_err_to_name(ret));
        return false;
    }
    
    Serial.println("[KEY_MGR] ✅ Master key deleted successfully");
    return true;
}

bool getMasterKey(uint8_t* key) {
    if (!key) {
        Serial.println("[KEY_MGR] Invalid key buffer provided");
        return false;
    }
    
    if (!key_manager_initialized && !initKeyManager()) {
        return false;
    }
    
    // Try to load the key first
    if (loadMasterKey(key)) {
        Serial.println("[KEY_MGR] ✅ Master key loaded successfully");
        return true;
    }
    
    // If loading failed, check if master key exists
    Serial.println("[KEY_MGR] Master key load failed, checking if key exists...");
    if (!masterKeyExists()) {
        Serial.println("[KEY_MGR] No master key found, generating new one...");
        if (!generateAndStoreMasterKey()) {
            Serial.println("[KEY_MGR] Failed to generate master key");
            return false;
        }
        
        // Add a small delay to ensure NVS write completes
        delay(10);
        
        // Try to load the newly generated key
        if (loadMasterKey(key)) {
            Serial.println("[KEY_MGR] ✅ Newly generated master key loaded successfully");
            return true;
        } else {
            Serial.println("[KEY_MGR] ❌ Failed to load newly generated master key");
            return false;
        }
    } else {
        Serial.println("[KEY_MGR] Master key exists but failed to load - possible corruption");
        return false;
    }
}

bool verifyMasterKey() {
    if (!masterKeyExists()) {
        Serial.println("[KEY_MGR] No master key to verify");
        return false;
    }
    
    uint8_t key[MASTER_KEY_SIZE];
    if (!loadMasterKey(key)) {
        Serial.println("[KEY_MGR] Failed to load master key for verification");
        return false;
    }
    
    // Basic sanity check - key should not be all zeros
    bool all_zero = true;
    for (int i = 0; i < MASTER_KEY_SIZE; i++) {
        if (key[i] != 0) {
            all_zero = false;
            break;
        }
    }
    
    secureZero(key, sizeof(key));
    
    if (all_zero) {
        Serial.println("[KEY_MGR] Master key verification failed - key is all zeros");
        return false;
    }
    
    Serial.println("[KEY_MGR] ✅ Master key verification passed");
    return true;
}

void getKeyManagerStatus(String& status) {
    status = "Key Manager Status:\n";
    status += "- Initialized: " + String(key_manager_initialized ? "Yes" : "No") + "\n";
    status += "- Master Key Exists: " + String(masterKeyExists() ? "Yes" : "No") + "\n";
    
    if (masterKeyExists()) {
        status += "- Key Verification: " + String(verifyMasterKey() ? "Pass" : "Fail") + "\n";
    }
    
    // NVS info
    nvs_stats_t nvs_stats;
    if (nvs_get_stats(NULL, &nvs_stats) == ESP_OK) {
        status += "- NVS Used Entries: " + String(nvs_stats.used_entries) + "\n";
        status += "- NVS Free Entries: " + String(nvs_stats.free_entries) + "\n";
    }
}

bool ensureMasterKeyInitialized() {
    if (masterKeyExists() && verifyMasterKey()) {
        Serial.println("[KEY_MGR] ✅ Master key already initialized and verified");
        return true;
    }
    
    if (!masterKeyExists()) {
        Serial.println("[KEY_MGR] First-time setup: generating master key...");
        return generateAndStoreMasterKey();
    }
    
    // Key exists but verification failed - this is a problem
    Serial.println("[KEY_MGR] ⚠️ Master key exists but verification failed");
    Serial.println("[KEY_MGR] This may indicate key corruption or hardware issues");
    
    // For now, we'll try to regenerate the key
    // In production, you might want to prompt the user
    Serial.println("[KEY_MGR] Regenerating master key (existing encrypted data will be lost)");
    return generateAndStoreMasterKey();
}