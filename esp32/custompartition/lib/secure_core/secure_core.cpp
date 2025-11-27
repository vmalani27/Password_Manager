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

// Derive deterministic DB key from eFuse BLOCK3 using HMAC-SHA256
// This ensures the SAME key is derived on every boot (required for database decryption)
// Security: eFuse BLOCK3 is hardware root-of-trust, label "DB_KEY_V1" is deterministic
static bool derive_db_key_from_efuse(uint8_t* out, size_t out_len) {
    if (out_len < 32) {
        Serial.println("ERROR: Output buffer too small for DB key");
        return false;
    }

    // Read eFuse BLOCK3 (256 bits = 32 bytes)
    uint8_t efuse_key[32] = {0};
    esp_err_t ret = esp_efuse_read_block(EFUSE_BLK3, efuse_key, 0, 256);
    
    if (ret != ESP_OK) {
        Serial.printf("eFuse BLOCK3 read failed: %s\n", esp_err_to_name(ret));
        return false;
    }
    
    Serial.println("eFuse BLOCK3 read successfully");

    // Deterministic label for DB key derivation
    // Same eFuse + same label = same key every boot
    const uint8_t label[] = "DB_KEY_V1";

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);

    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (mbedtls_md_setup(&ctx, info, 1) != 0) {
        Serial.println("ERROR: HMAC setup failed");
        mbedtls_md_free(&ctx);
        memset(efuse_key, 0, sizeof(efuse_key));
        return false;
    }

    // HMAC-SHA256(efuse_key, "DB_KEY_V1") -> 32-byte deterministic key
    if (mbedtls_md_hmac_starts(&ctx, efuse_key, sizeof(efuse_key)) != 0) {
        Serial.println("ERROR: HMAC start failed");
        mbedtls_md_free(&ctx);
        memset(efuse_key, 0, sizeof(efuse_key));
        return false;
    }
    
    if (mbedtls_md_hmac_update(&ctx, label, sizeof(label) - 1) != 0) {
        Serial.println("ERROR: HMAC update failed");
        mbedtls_md_free(&ctx);
        memset(efuse_key, 0, sizeof(efuse_key));
        return false;
    }
    
    if (mbedtls_md_hmac_finish(&ctx, out) != 0) {
        Serial.println("ERROR: HMAC finish failed");
        mbedtls_md_free(&ctx);
        memset(efuse_key, 0, sizeof(efuse_key));
        return false;
    }

    mbedtls_md_free(&ctx);
    
    // Zero out the eFuse key from memory for security
    memset(efuse_key, 0, sizeof(efuse_key));

    Serial.println("DB runtime key derived from eFuse (deterministic)");
    return true;
}

// Synchronous runtime key derivation (deterministic)
// Derives the same key on every boot for database encryption
bool deriveRuntimeKey() {
    Serial.println("Deriving deterministic runtime key from eFuse...");
    
    // Derive deterministic DB key from eFuse BLOCK3
    // Same hardware -> same key every boot
    if (derive_db_key_from_efuse(runtime_key, sizeof(runtime_key))) {
        key_ready = true;
        Serial.println("Runtime key derived and ready (stable across reboots)");
        return true;
    } else {
        Serial.println("ERROR: Failed to derive runtime key");
        return false;
    }
}

// Initialize key manager
// Note: deriveRuntimeKey() is called separately in main.cpp setup()
bool initKeyManager() {
    Serial.println("Initializing key manager...");
    // mbedTLS is already initialized by ESP-IDF
    key_ready = false;
    Serial.println("Key manager initialized (ready for key derivation)");
    return true;
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

// ============================================================================
// SESSION ENCRYPTION (BLE Traffic - AES-256-CTR)
// ============================================================================

// Generate random nonce for CTR mode
void generate_nonce(uint8_t* nonce) {
    for (int i = 0; i < NONCE_SIZE; i++) {
        nonce[i] = esp_random() & 0xFF;
    }
}

// Encrypt data for BLE transmission using session key (AES-256-CTR)
// CTR mode: no padding needed, ciphertext length = plaintext length
bool encrypt_session(const uint8_t* session_key, const uint8_t* plaintext, size_t plaintext_len,
                     uint8_t* ciphertext, size_t* ciphertext_len, uint8_t* nonce) {
    if (!session_key || !plaintext || !ciphertext || !ciphertext_len || !nonce) {
        Serial.println("Session encryption failed: NULL parameters");
        return false;
    }
    
    if (plaintext_len == 0 || plaintext_len > MAX_SESSION_ENCRYPTED_SIZE) {
        Serial.printf("Session encryption failed: Invalid length %zu\n", plaintext_len);
        return false;
    }
    
    // Generate random nonce (IV for CTR mode)
    generate_nonce(nonce);
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    // Set encryption key (AES-256)
    if (mbedtls_aes_setkey_enc(&aes, session_key, 256) != 0) {
        Serial.println("Session encryption: AES key setup failed");
        mbedtls_aes_free(&aes);
        return false;
    }
    
    // CTR mode variables
    uint8_t nonce_counter[16];
    uint8_t stream_block[16];
    size_t nc_off = 0;
    
    memcpy(nonce_counter, nonce, 16);
    memset(stream_block, 0, 16);
    
    // Encrypt with AES-256-CTR (no padding, output length = input length)
    int ret = mbedtls_aes_crypt_ctr(&aes, plaintext_len, &nc_off, 
                                    nonce_counter, stream_block,
                                    plaintext, ciphertext);
    
    mbedtls_aes_free(&aes);
    
    if (ret != 0) {
        Serial.printf("Session encryption failed: %d\n", ret);
        return false;
    }
    
    *ciphertext_len = plaintext_len;  // CTR mode: same length
    return true;
}

// Decrypt data from BLE transmission using session key (AES-256-CTR)
bool decrypt_session(const uint8_t* session_key, const uint8_t* ciphertext, size_t ciphertext_len,
                     const uint8_t* nonce, uint8_t* plaintext, size_t* plaintext_len) {
    if (!session_key || !ciphertext || !nonce || !plaintext || !plaintext_len) {
        Serial.println("Session decryption failed: NULL parameters");
        return false;
    }
    
    if (ciphertext_len == 0 || ciphertext_len > MAX_SESSION_ENCRYPTED_SIZE) {
        Serial.printf("Session decryption failed: Invalid length %zu\n", ciphertext_len);
        return false;
    }
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    // Set encryption key (CTR mode uses encryption for both directions)
    if (mbedtls_aes_setkey_enc(&aes, session_key, 256) != 0) {
        Serial.println("Session decryption: AES key setup failed");
        mbedtls_aes_free(&aes);
        return false;
    }
    
    // CTR mode variables
    uint8_t nonce_counter[16];
    uint8_t stream_block[16];
    size_t nc_off = 0;
    
    memcpy(nonce_counter, nonce, 16);
    memset(stream_block, 0, 16);
    
    // Decrypt with AES-256-CTR (symmetric operation)
    int ret = mbedtls_aes_crypt_ctr(&aes, ciphertext_len, &nc_off,
                                    nonce_counter, stream_block,
                                    ciphertext, plaintext);
    
    mbedtls_aes_free(&aes);
    
    if (ret != 0) {
        Serial.printf("Session decryption failed: %d\n", ret);
        return false;
    }
    
    *plaintext_len = ciphertext_len;  // CTR mode: same length
    plaintext[*plaintext_len] = '\0';  // Null-terminate for String conversion
    
    return true;
}