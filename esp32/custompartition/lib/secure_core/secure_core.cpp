#include "secure_core.h"
#include "mbedtls/md.h"
#include "mbedtls/aes.h"
#include "esp_efuse.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_efuse.h"
#include "esp_efuse_table.h"


// Static variables for key management
static uint8_t runtime_key[32];
static bool key_ready = false;
static TaskHandle_t key_task_handle = NULL;

// Derive runtime key from eFuse BLOCK3 + challenge using HMAC-SHA256
bool derive_key_from_efuse_and_challenge(const uint8_t* challenge, size_t challenge_len, uint8_t* output_key) {
    // Read eFuse BLOCK3 (256 bits = 32 bytes)
    uint8_t efuse_key[32];
    size_t efuse_len = sizeof(efuse_key);
    // Read eFuse BLOCK3 (raw 256-bit user key)
esp_err_t ret = esp_efuse_read_block(EFUSE_BLK3, efuse_key, 0, 256);

if (ret != ESP_OK) {
    Serial.printf("eFuse BLOCK3 read failed: %s\n", esp_err_to_name(ret));
    return false;
}


    Serial.println("eFuse BLOCK3 read successfully");
    
    // HMAC-SHA256: HMAC(eFuse_key, challenge) -> runtime_key
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (mbedtls_md_setup(&ctx, info, 1) != 0) {
        Serial.println("HMAC setup failed");
        mbedtls_md_free(&ctx);
        return false;
    }
    
    if (mbedtls_md_hmac_starts(&ctx, efuse_key, sizeof(efuse_key)) != 0) {
        Serial.println("HMAC start failed");
        mbedtls_md_free(&ctx);
        return false;
    }
    
    if (mbedtls_md_hmac_update(&ctx, challenge, challenge_len) != 0) {
        Serial.println("HMAC update failed");
        mbedtls_md_free(&ctx);
        return false;
    }
    
    if (mbedtls_md_hmac_finish(&ctx, output_key) != 0) {
        Serial.println("HMAC finish failed");
        mbedtls_md_free(&ctx);
        return false;
    }
    
    mbedtls_md_free(&ctx);
    
    // Zero out the eFuse key from memory for security
    memset(efuse_key, 0, sizeof(efuse_key));
    
    Serial.println("Runtime key derived successfully");
    return true;
}

// Key manager task - generates runtime key
void key_manager_task(void* parameter) {
    Serial.println("Key manager task starting...");
    
    // Generate a random challenge for this boot session
    uint8_t challenge[16];
    for (int i = 0; i < sizeof(challenge); i++) {
        challenge[i] = esp_random() & 0xFF;
    }
    
    Serial.println("Generated random challenge");
    
    // Derive the runtime key
    if (derive_key_from_efuse_and_challenge(challenge, sizeof(challenge), runtime_key)) {
        key_ready = true;
        Serial.println("Runtime key generation completed successfully");
    } else {
        Serial.println("Failed to derive runtime key");
        // Don't set key_ready = true, system will halt
    }
    
    // Task completes - key is ready for use
    vTaskDelete(NULL);
}

// Initialize key manager (minimal setup)
bool initKeyManager() {
    Serial.println("Initializing key manager...");
    // mbedTLS is already initialized by ESP-IDF
    key_ready = false;
    return true;
}

// Start the key derivation task
void startKeyManagerTask() {
    Serial.println("Starting key manager task...");
    xTaskCreate(key_manager_task, "key_mgr", 4096, NULL, 1, &key_task_handle);
}

// Wait for runtime key to be ready
void waitForRuntimeKeyReady() {
    Serial.println("Waiting for runtime key to be ready...");
    while (!key_ready) {
        vTaskDelay(pdMS_TO_TICKS(100));
        Serial.print(".");
    }
    Serial.println("\nRuntime key is ready for use");
}

// Check if key is ready
bool isRuntimeKeyReady() {
    return key_ready;
}

// Generate random IV
void generate_iv(uint8_t* iv) {
    for (int i = 0; i < IV_SIZE; i++) {
        iv[i] = esp_random() & 0xFF;
    }
}

// Encrypt password using AES-256-CBC
bool encrypt_password(const String& plaintext, uint8_t* ciphertext, size_t* ciphertext_len, uint8_t* iv) {
    if (!key_ready) {
        Serial.println("Encryption failed: Runtime key not ready");
        return false;
    }
    
    if (plaintext.length() == 0) {
        Serial.println("Encryption failed: Empty plaintext");
        return false;
    }
    
    // Generate random IV
    generate_iv(iv);
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    // Set encryption key (AES-256)
    if (mbedtls_aes_setkey_enc(&aes, runtime_key, 256) != 0) {
        Serial.println("AES key setup failed");
        mbedtls_aes_free(&aes);
        return false;
    }
    
    // Prepare padded input (PKCS#7 padding to 16-byte boundary)
    size_t padded_len = ((plaintext.length() / 16) + 1) * 16;
    if (padded_len > MAX_PASSWORD_ENCRYPTED_SIZE) {
        Serial.println("Password too long for encryption buffer");
        mbedtls_aes_free(&aes);
        return false;
    }
    
    uint8_t* padded_input = (uint8_t*)malloc(padded_len);
    memset(padded_input, 0, padded_len);
    memcpy(padded_input, plaintext.c_str(), plaintext.length());
    
    // Apply PKCS#7 padding
    uint8_t pad_val = padded_len - plaintext.length();
    for (size_t i = plaintext.length(); i < padded_len; i++) {
        padded_input[i] = pad_val;
    }
    
    // Copy IV for CBC mode (it gets modified during encryption)
    uint8_t iv_copy[16];
    memcpy(iv_copy, iv, 16);
    
    // Encrypt with AES-256-CBC
    int ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padded_len, iv_copy, padded_input, ciphertext);
    
    *ciphertext_len = padded_len;
    
    // Clean up
    free(padded_input);
    mbedtls_aes_free(&aes);
    
    if (ret == 0) {
        Serial.printf("Password encrypted successfully (%zu bytes)\n", *ciphertext_len);
        return true;
    } else {
        Serial.printf("AES encryption failed: %d\n", ret);
        return false;
    }
}

// Decrypt password using AES-256-CBC
bool decrypt_password(const uint8_t* ciphertext, size_t ciphertext_len, const uint8_t* iv, String& plaintext) {
    if (!key_ready) {
        Serial.println("Decryption failed: Runtime key not ready");
        return false;
    }
    
    if (ciphertext_len == 0 || ciphertext_len % 16 != 0) {
        Serial.println("Decryption failed: Invalid ciphertext length");
        return false;
    }
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    // Set decryption key (AES-256)
    if (mbedtls_aes_setkey_dec(&aes, runtime_key, 256) != 0) {
        Serial.println("AES key setup failed for decryption");
        mbedtls_aes_free(&aes);
        return false;
    }
    
    // Allocate buffer for decryption
    uint8_t* decrypted = (uint8_t*)malloc(ciphertext_len);
    
    // Copy IV for CBC mode (it gets modified during decryption)
    uint8_t iv_copy[16];
    memcpy(iv_copy, iv, 16);
    
    // Decrypt with AES-256-CBC
    int ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, ciphertext_len, iv_copy, ciphertext, decrypted);
    
    if (ret == 0) {
        // Remove PKCS#7 padding
        uint8_t pad_val = decrypted[ciphertext_len - 1];
        if (pad_val > 0 && pad_val <= 16) {
            // Validate padding
            bool valid_padding = true;
            for (size_t i = ciphertext_len - pad_val; i < ciphertext_len; i++) {
                if (decrypted[i] != pad_val) {
                    valid_padding = false;
                    break;
                }
            }
            
            if (valid_padding) {
                size_t actual_len = ciphertext_len - pad_val;
                decrypted[actual_len] = '\0';
                plaintext = String((char*)decrypted);
                Serial.printf("Password decrypted successfully (%zu bytes)\n", actual_len);
            } else {
                Serial.println("Invalid padding in decrypted data");
                ret = -1;
            }
        } else {
            Serial.println("Invalid padding value");
            ret = -1;
        }
    } else {
        Serial.printf("AES decryption failed: %d\n", ret);
    }
    
    // Clean up
    free(decrypted);
    mbedtls_aes_free(&aes);
    
    return (ret == 0);
}