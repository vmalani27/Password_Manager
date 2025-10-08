#include "encryption_utils.h"

// Global encryption context
static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context ctr_drbg;
static bool encryption_initialized = false;

bool initEncryption() {
    if (encryption_initialized) {
        return true;
    }
    
    Serial.println("[CRYPTO] Initializing encryption system...");
    
    // Initialize entropy and random number generator
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    
    const char* pers = "esp32_password_manager";
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                   (const unsigned char*)pers, strlen(pers));
    
    if (ret != 0) {
        Serial.printf("[CRYPTO] Failed to seed random number generator: -0x%04x\n", -ret);
        return false;
    }
    
    encryption_initialized = true;
    Serial.println("[CRYPTO] ✅ Encryption system initialized successfully");
    return true;
}

bool generateMasterKey(uint8_t* key) {
    if (!encryption_initialized && !initEncryption()) {
        return false;
    }
    
    int ret = mbedtls_ctr_drbg_random(&ctr_drbg, key, MASTER_KEY_SIZE);
    if (ret != 0) {
        Serial.printf("[CRYPTO] Failed to generate master key: -0x%04x\n", -ret);
        return false;
    }
    
    Serial.println("[CRYPTO] ✅ Master key generated successfully");
    return true;
}

bool generateIV(uint8_t* iv) {
    if (!encryption_initialized && !initEncryption()) {
        return false;
    }
    
    int ret = mbedtls_ctr_drbg_random(&ctr_drbg, iv, AES_IV_SIZE);
    if (ret != 0) {
        Serial.printf("[CRYPTO] Failed to generate IV: -0x%04x\n", -ret);
        return false;
    }
    
    return true;
}

bool deriveKeyFromMaster(const uint8_t* masterKey, const char* context, uint8_t* derivedKey) {
    if (!masterKey || !context || !derivedKey) {
        Serial.println("[CRYPTO] Invalid parameters for key derivation");
        return false;
    }
    
    // Use HKDF-SHA256 for key derivation
    mbedtls_md_context_t md_ctx;
    mbedtls_md_init(&md_ctx);
    
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md_info) {
        Serial.println("[CRYPTO] Failed to get SHA256 info");
        mbedtls_md_free(&md_ctx);
        return false;
    }
    
    int ret = mbedtls_md_setup(&md_ctx, md_info, 1); // 1 for HMAC
    if (ret != 0) {
        Serial.printf("[CRYPTO] Failed to setup MD context: -0x%04x\n", -ret);
        mbedtls_md_free(&md_ctx);
        return false;
    }
    
    // HKDF Extract: HMAC-SHA256(salt=context, ikm=masterKey)
    uint8_t prk[32]; // SHA256 output size
    ret = mbedtls_md_hmac(md_info, (const unsigned char*)context, strlen(context),
                         masterKey, MASTER_KEY_SIZE, prk);
    
    if (ret != 0) {
        Serial.printf("[CRYPTO] HKDF extract failed: -0x%04x\n", -ret);
        mbedtls_md_free(&md_ctx);
        return false;
    }
    
    // HKDF Expand: HMAC-SHA256(prk, info=context||0x01)
    uint8_t info[64];
    size_t context_len = strlen(context);
    memcpy(info, context, context_len);
    info[context_len] = 0x01; // Counter for first block
    
    ret = mbedtls_md_hmac(md_info, prk, sizeof(prk), info, context_len + 1, derivedKey);
    
    mbedtls_md_free(&md_ctx);
    secureZero(prk, sizeof(prk));
    secureZero(info, sizeof(info));
    
    if (ret != 0) {
        Serial.printf("[CRYPTO] HKDF expand failed: -0x%04x\n", -ret);
        return false;
    }
    
    return true;
}

bool encryptData(const String& plaintext, const uint8_t* key, const uint8_t* iv, 
                 String& ciphertext, uint8_t* tag) {
    if (!key || !iv || !tag) {
        Serial.println("[CRYPTO] Invalid parameters for encryption");
        return false;
    }
    
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    
    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, AES_KEY_SIZE * 8);
    if (ret != 0) {
        Serial.printf("[CRYPTO] Failed to set GCM key: -0x%04x\n", -ret);
        mbedtls_gcm_free(&gcm);
        return false;
    }
    
    // Allocate buffer for ciphertext
    size_t plaintext_len = plaintext.length();
    uint8_t* cipher_buffer = (uint8_t*)malloc(plaintext_len);
    if (!cipher_buffer) {
        Serial.println("[CRYPTO] Failed to allocate cipher buffer");
        mbedtls_gcm_free(&gcm);
        return false;
    }
    
    ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, plaintext_len,
                                   iv, AES_IV_SIZE, NULL, 0, // No additional authenticated data
                                   (const unsigned char*)plaintext.c_str(), cipher_buffer,
                                   AES_TAG_SIZE, tag);
    
    if (ret != 0) {
        Serial.printf("[CRYPTO] GCM encryption failed: -0x%04x\n", -ret);
        free(cipher_buffer);
        mbedtls_gcm_free(&gcm);
        return false;
    }
    
    // Convert to hex string
    ciphertext = bytesToHex(cipher_buffer, plaintext_len);
    
    free(cipher_buffer);
    mbedtls_gcm_free(&gcm);
    return true;
}

bool decryptData(const String& ciphertext, const uint8_t* key, const uint8_t* iv,
                 const uint8_t* tag, String& plaintext) {
    if (!key || !iv || !tag) {
        Serial.println("[CRYPTO] Invalid parameters for decryption");
        return false;
    }
    
    // Convert hex string back to binary
    size_t cipher_len = ciphertext.length() / 2;
    uint8_t* cipher_buffer = (uint8_t*)malloc(cipher_len);
    if (!cipher_buffer) {
        Serial.println("[CRYPTO] Failed to allocate cipher buffer for decryption");
        return false;
    }
    
    if (hexToBytes(ciphertext, cipher_buffer, cipher_len) != cipher_len) {
        Serial.println("[CRYPTO] Failed to convert hex to bytes");
        free(cipher_buffer);
        return false;
    }
    
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    
    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, AES_KEY_SIZE * 8);
    if (ret != 0) {
        Serial.printf("[CRYPTO] Failed to set GCM key for decryption: -0x%04x\n", -ret);
        free(cipher_buffer);
        mbedtls_gcm_free(&gcm);
        return false;
    }
    
    // Allocate buffer for plaintext
    uint8_t* plain_buffer = (uint8_t*)malloc(cipher_len + 1); // +1 for null terminator
    if (!plain_buffer) {
        Serial.println("[CRYPTO] Failed to allocate plaintext buffer");
        free(cipher_buffer);
        mbedtls_gcm_free(&gcm);
        return false;
    }
    
    ret = mbedtls_gcm_auth_decrypt(&gcm, cipher_len, iv, AES_IV_SIZE,
                                  NULL, 0, // No additional authenticated data
                                  tag, AES_TAG_SIZE, cipher_buffer, plain_buffer);
    
    if (ret != 0) {
        Serial.printf("[CRYPTO] GCM decryption/authentication failed: -0x%04x\n", -ret);
        free(cipher_buffer);
        free(plain_buffer);
        mbedtls_gcm_free(&gcm);
        return false;
    }
    
    // Convert to string
    plain_buffer[cipher_len] = '\0';
    plaintext = String((char*)plain_buffer);
    
    secureZero(plain_buffer, cipher_len + 1);
    free(cipher_buffer);
    free(plain_buffer);
    mbedtls_gcm_free(&gcm);
    return true;
}

String bytesToHex(const uint8_t* data, size_t len) {
    String result;
    result.reserve(len * 2);
    
    for (size_t i = 0; i < len; i++) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02X", data[i]);
        result += hex;
    }
    
    return result;
}

size_t hexToBytes(const String& hex, uint8_t* data, size_t maxLen) {
    if (hex.length() % 2 != 0) {
        Serial.println("[CRYPTO] Invalid hex string length (must be even)");
        return 0;
    }
    
    size_t bytes_len = hex.length() / 2;
    if (bytes_len > maxLen) {
        Serial.printf("[CRYPTO] Hex string too long: %d bytes, max %d\n", bytes_len, maxLen);
        return 0;
    }
    
    for (size_t i = 0; i < bytes_len; i++) {
        char hex_byte[3] = {hex[i*2], hex[i*2+1], '\0'};
        data[i] = (uint8_t)strtol(hex_byte, NULL, 16);
    }
    
    return bytes_len;
}

void secureZero(void* ptr, size_t len) {
    if (!ptr) return;
    
    volatile uint8_t* p = (volatile uint8_t*)ptr;
    for (size_t i = 0; i < len; i++) {
        p[i] = 0;
    }
}

bool encryptCredentialField(const String& plaintext, const uint8_t* masterKey,
                           const char* context, String& ciphertext, 
                           String& ivHex, String& tagHex) {
    if (!masterKey || !context) {
        Serial.println("[CRYPTO] Invalid parameters for credential encryption");
        return false;
    }
    
    // Derive context-specific key
    uint8_t derivedKey[AES_KEY_SIZE];
    if (!deriveKeyFromMaster(masterKey, context, derivedKey)) {
        Serial.println("[CRYPTO] Failed to derive encryption key");
        return false;
    }
    
    // Generate random IV
    uint8_t iv[AES_IV_SIZE];
    if (!generateIV(iv)) {
        Serial.println("[CRYPTO] Failed to generate IV");
        secureZero(derivedKey, sizeof(derivedKey));
        return false;
    }
    
    // Encrypt the data
    uint8_t tag[AES_TAG_SIZE];
    bool success = encryptData(plaintext, derivedKey, iv, ciphertext, tag);
    
    if (success) {
        ivHex = bytesToHex(iv, AES_IV_SIZE);
        tagHex = bytesToHex(tag, AES_TAG_SIZE);
    }
    
    // Clear sensitive data
    secureZero(derivedKey, sizeof(derivedKey));
    secureZero(iv, sizeof(iv));
    secureZero(tag, sizeof(tag));
    
    return success;
}

bool decryptCredentialField(const String& ciphertext, const String& ivHex,
                           const String& tagHex, const uint8_t* masterKey,
                           const char* context, String& plaintext) {
    if (!masterKey || !context) {
        Serial.println("[CRYPTO] Invalid parameters for credential decryption");
        return false;
    }
    
    // Derive context-specific key
    uint8_t derivedKey[AES_KEY_SIZE];
    if (!deriveKeyFromMaster(masterKey, context, derivedKey)) {
        Serial.println("[CRYPTO] Failed to derive decryption key");
        return false;
    }
    
    // Convert hex strings back to binary
    uint8_t iv[AES_IV_SIZE];
    uint8_t tag[AES_TAG_SIZE];
    
    if (hexToBytes(ivHex, iv, AES_IV_SIZE) != AES_IV_SIZE) {
        Serial.println("[CRYPTO] Invalid IV hex string");
        secureZero(derivedKey, sizeof(derivedKey));
        return false;
    }
    
    if (hexToBytes(tagHex, tag, AES_TAG_SIZE) != AES_TAG_SIZE) {
        Serial.println("[CRYPTO] Invalid tag hex string");
        secureZero(derivedKey, sizeof(derivedKey));
        return false;
    }
    
    // Decrypt the data
    bool success = decryptData(ciphertext, derivedKey, iv, tag, plaintext);
    
    // Clear sensitive data
    secureZero(derivedKey, sizeof(derivedKey));
    secureZero(iv, sizeof(iv));
    secureZero(tag, sizeof(tag));
    
    return success;
}