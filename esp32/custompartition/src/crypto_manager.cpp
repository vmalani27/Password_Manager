#include "crypto_manager.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/md.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

// Singleton instance
CryptoManager& CryptoManager::getInstance() {
    static CryptoManager instance;
    return instance;
}

// Constructor
CryptoManager::CryptoManager() 
    : ecdh_ready(false)
    , challenge_pending(false)
    , pairing_state(PAIRING_STATE_UNPAIRED)
    , has_stored_client_key(false)
{
    memset(esp32_private_key, 0, sizeof(esp32_private_key));
    memset(esp32_public_key, 0, sizeof(esp32_public_key));
    memset(client_public_key, 0, sizeof(client_public_key));
    memset(shared_secret, 0, sizeof(shared_secret));
    memset(session_aes_key, 0, sizeof(session_aes_key));
    memset(pending_challenge, 0, sizeof(pending_challenge));
    memset(stored_client_public_key, 0, sizeof(stored_client_public_key));
}

// Destructor
CryptoManager::~CryptoManager() {
    clearECDH();
}

// Initialize crypto manager
bool CryptoManager::begin() {
    Serial.println("CryptoManager: Initializing...");
    return loadOrGenerateKeys();
}

// ============================================================================
// ECDH IMPLEMENTATION
// ============================================================================

// Initialize ECDH and generate ESP32's key pair
bool CryptoManager::initECDH() {
    mbedtls_ecdh_context ecdh_ctx;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    
    mbedtls_ecdh_init(&ecdh_ctx);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    
    const char* pers = "ecdh_esp32";
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                     (const unsigned char*)pers, strlen(pers));
    if (ret != 0) {
        Serial.printf("ECDH: RNG seed failed: %d\n", ret);
        return false;
    }
    
    // Use secp256r1 (NIST P-256) curve
    ret = mbedtls_ecp_group_load(&ecdh_ctx.grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) {
        Serial.printf("ECDH: Curve load failed: %d\n", ret);
        return false;
    }
    
    // Generate our key pair
    ret = mbedtls_ecdh_gen_public(&ecdh_ctx.grp, &ecdh_ctx.d, 
                                   &ecdh_ctx.Q, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        Serial.printf("ECDH: Key generation failed: %d\n", ret);
        return false;
    }
    
    // Export private key
    ret = mbedtls_mpi_write_binary(&ecdh_ctx.d, esp32_private_key, 32);
    if (ret != 0) {
        Serial.printf("ECDH: Private key export failed: %d\n", ret);
        return false;
    }
    
    // Export public key (X, Y coordinates - 32 bytes each)
    ret = mbedtls_mpi_write_binary(&ecdh_ctx.Q.X, esp32_public_key, 32);
    if (ret != 0) {
        Serial.printf("ECDH: Public key X export failed: %d\n", ret);
        return false;
    }
    
    ret = mbedtls_mpi_write_binary(&ecdh_ctx.Q.Y, esp32_public_key + 32, 32);
    if (ret != 0) {
        Serial.printf("ECDH: Public key Y export failed: %d\n", ret);
        return false;
    }
    
    mbedtls_ecdh_free(&ecdh_ctx);
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    
    Serial.println("ECDH: Key pair generated successfully");
    Serial.print("ECDH: Public key (hex): ");
    for (int i = 0; i < 64; i++) {
        Serial.printf("%02X", esp32_public_key[i]);
    }
    Serial.println();
    
    return true;
}

// Compute shared secret from client's public key
bool CryptoManager::computeSharedSecret(const uint8_t* client_pubkey) {
    mbedtls_ecdh_context ecdh_ctx;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    
    mbedtls_ecdh_init(&ecdh_ctx);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    
    const char* pers = "ecdh_shared";
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                     (const unsigned char*)pers, strlen(pers));
    if (ret != 0) {
        Serial.printf("ECDH: RNG seed failed: %d\n", ret);
        return false;
    }
    
    // Load curve
    ret = mbedtls_ecp_group_load(&ecdh_ctx.grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) {
        Serial.printf("ECDH: Curve load failed: %d\n", ret);
        return false;
    }
    
    // Load our private key
    ret = mbedtls_mpi_read_binary(&ecdh_ctx.d, esp32_private_key, 32);
    if (ret != 0) {
        Serial.printf("ECDH: Private key import failed: %d\n", ret);
        return false;
    }
    
    // Load client's public key (X, Y coordinates)
    ret = mbedtls_mpi_read_binary(&ecdh_ctx.Qp.X, client_pubkey, 32);
    if (ret != 0) {
        Serial.printf("ECDH: Client public key X import failed: %d\n", ret);
        return false;
    }
    
    ret = mbedtls_mpi_read_binary(&ecdh_ctx.Qp.Y, client_pubkey + 32, 32);
    if (ret != 0) {
        Serial.printf("ECDH: Client public key Y import failed: %d\n", ret);
        return false;
    }
    
    ret = mbedtls_mpi_lset(&ecdh_ctx.Qp.Z, 1);
    if (ret != 0) {
        Serial.printf("ECDH: Client public key Z set failed: %d\n", ret);
        return false;
    }
    
    // Compute shared secret
    size_t olen;
    ret = mbedtls_ecdh_calc_secret(&ecdh_ctx, &olen, shared_secret, 32,
                                    mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        Serial.printf("ECDH: Shared secret computation failed: %d\n", ret);
        return false;
    }
    
    Serial.printf("ECDH: Shared secret computed (%d bytes)\n", olen);
    
    // Derive session AES key using HKDF-like approach (manual implementation)
    // HKDF-Extract: PRK = HMAC-SHA256(salt, shared_secret)
    uint8_t prk[32];
    mbedtls_md_context_t hkdf_ctx;
    mbedtls_md_init(&hkdf_ctx);
    
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    const uint8_t salt[16] = {'B','L','E','_','P','A','S','S','W','O','R','D','_','M','G','R'};
    
    ret = mbedtls_md_setup(&hkdf_ctx, md, 1);
    if (ret != 0) {
        Serial.printf("ECDH: HMAC setup failed: %d\n", ret);
        return false;
    }
    
    ret = mbedtls_md_hmac_starts(&hkdf_ctx, salt, 16);
    if (ret != 0) {
        Serial.printf("ECDH: HMAC start failed: %d\n", ret);
        mbedtls_md_free(&hkdf_ctx);
        return false;
    }
    
    ret = mbedtls_md_hmac_update(&hkdf_ctx, shared_secret, olen);
    if (ret != 0) {
        Serial.printf("ECDH: HMAC update failed: %d\n", ret);
        mbedtls_md_free(&hkdf_ctx);
        return false;
    }
    
    ret = mbedtls_md_hmac_finish(&hkdf_ctx, prk);
    if (ret != 0) {
        Serial.printf("ECDH: HMAC finish failed: %d\n", ret);
        mbedtls_md_free(&hkdf_ctx);
        return false;
    }
    
    // HKDF-Expand: session_key = HMAC-SHA256(prk, info || 0x01)
    const uint8_t info[8] = {'S','E','S','S','I','O','N', 0x01};
    
    ret = mbedtls_md_hmac_starts(&hkdf_ctx, prk, 32);
    if (ret != 0) {
        Serial.printf("ECDH: HMAC expand start failed: %d\n", ret);
        mbedtls_md_free(&hkdf_ctx);
        return false;
    }
    
    ret = mbedtls_md_hmac_update(&hkdf_ctx, info, 8);
    if (ret != 0) {
        Serial.printf("ECDH: HMAC expand update failed: %d\n", ret);
        mbedtls_md_free(&hkdf_ctx);
        return false;
    }
    
    ret = mbedtls_md_hmac_finish(&hkdf_ctx, session_aes_key);
    if (ret != 0) {
        Serial.printf("ECDH: HMAC expand finish failed: %d\n", ret);
        mbedtls_md_free(&hkdf_ctx);
        return false;
    }
    
    mbedtls_md_free(&hkdf_ctx);
    
    Serial.println("ECDH: Session key derived successfully");
    
    // Store client public key
    memcpy(client_public_key, client_pubkey, ECDH_PUBLIC_KEY_SIZE);
    
    // Zero out shared secret (only keep derived key)
    memset(shared_secret, 0, sizeof(shared_secret));
    
    ecdh_ready = true;
    
    mbedtls_ecdh_free(&ecdh_ctx);
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    
    return true;
}

// Clear ECDH session state on disconnect (but preserve NVS pairing data)
void CryptoManager::clearECDH() {
    memset(client_public_key, 0, sizeof(client_public_key));
    memset(shared_secret, 0, sizeof(shared_secret));
    memset(session_aes_key, 0, sizeof(session_aes_key));
    ecdh_ready = false;
    challenge_pending = false;
    Serial.println("ECDH: Session state cleared");
}

// ============================================================================
// NVS DEVICE BINDING FUNCTIONS
// ============================================================================

// Load persistent keys from NVS or generate new ones if unpaired
bool CryptoManager::loadOrGenerateKeys() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    
    nvs_handle_t nvs_handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        Serial.printf("NVS: Failed to open namespace: %d\n", err);
        return false;
    }
    
    // Check if device is paired
    uint8_t paired = 0;
    err = nvs_get_u8(nvs_handle, NVS_KEY_PAIRED, &paired);
    if (err == ESP_OK && paired == 1) {
        // Device is paired - load keys from NVS
        pairing_state = PAIRING_STATE_PAIRED;
        
        size_t key_size = 32;
        err = nvs_get_blob(nvs_handle, NVS_KEY_ESP_PRIVATE, esp32_private_key, &key_size);
        if (err != ESP_OK || key_size != 32) {
            Serial.println("NVS: Failed to load ESP32 private key");
            nvs_close(nvs_handle);
            return false;
        }
        
        key_size = 64;
        err = nvs_get_blob(nvs_handle, NVS_KEY_ESP_PUBLIC, esp32_public_key, &key_size);
        if (err != ESP_OK || key_size != 64) {
            Serial.println("NVS: Failed to load ESP32 public key");
            nvs_close(nvs_handle);
            return false;
        }
        
        key_size = 64;
        err = nvs_get_blob(nvs_handle, NVS_KEY_CLIENT_PUBLIC, stored_client_public_key, &key_size);
        if (err == ESP_OK && key_size == 64) {
            has_stored_client_key = true;
            Serial.println("NVS: Loaded paired device keys");
        } else {
            Serial.println("NVS: Warning - paired state but no client key found");
            has_stored_client_key = false;
        }
        
        nvs_close(nvs_handle);
        Serial.println("NVS: Device is PAIRED - keys loaded from storage");
        return true;
        
    } else {
        // Device is unpaired - generate new ephemeral keys
        pairing_state = PAIRING_STATE_UNPAIRED;
        has_stored_client_key = false;
        nvs_close(nvs_handle);
        
        Serial.println("NVS: Device is UNPAIRED - generating new keys");
        return initECDH();
    }
}

// Save pairing to NVS after successful ECDH handshake
bool CryptoManager::savePairing(const uint8_t* client_pubkey_to_save) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        Serial.printf("NVS: Failed to open namespace for pairing: %d\n", err);
        return false;
    }
    
    // Save ESP32 keys
    err = nvs_set_blob(nvs_handle, NVS_KEY_ESP_PRIVATE, esp32_private_key, 32);
    if (err != ESP_OK) {
        Serial.println("NVS: Failed to save ESP32 private key");
        nvs_close(nvs_handle);
        return false;
    }
    
    err = nvs_set_blob(nvs_handle, NVS_KEY_ESP_PUBLIC, esp32_public_key, 64);
    if (err != ESP_OK) {
        Serial.println("NVS: Failed to save ESP32 public key");
        nvs_close(nvs_handle);
        return false;
    }
    
    // Save client public key
    err = nvs_set_blob(nvs_handle, NVS_KEY_CLIENT_PUBLIC, client_pubkey_to_save, 64);
    if (err != ESP_OK) {
        Serial.println("NVS: Failed to save client public key");
        nvs_close(nvs_handle);
        return false;
    }
    
    // Mark as paired
    err = nvs_set_u8(nvs_handle, NVS_KEY_PAIRED, 1);
    if (err != ESP_OK) {
        Serial.println("NVS: Failed to set paired flag");
        nvs_close(nvs_handle);
        return false;
    }
    
    // Commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        Serial.println("NVS: Failed to commit pairing");
        nvs_close(nvs_handle);
        return false;
    }
    
    nvs_close(nvs_handle);
    
    // Update runtime state
    pairing_state = PAIRING_STATE_PAIRED;
    memcpy(stored_client_public_key, client_pubkey_to_save, 64);
    has_stored_client_key = true;
    
    Serial.println("NVS: Device pairing saved successfully");
    return true;
}

// Unpair device (factory reset for pairing)
bool CryptoManager::unpairDevice() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        Serial.printf("NVS: Failed to open namespace for unpair: %d\n", err);
        return false;
    }
    
    // Erase all pairing data
    nvs_erase_key(nvs_handle, NVS_KEY_ESP_PRIVATE);
    nvs_erase_key(nvs_handle, NVS_KEY_ESP_PUBLIC);
    nvs_erase_key(nvs_handle, NVS_KEY_CLIENT_PUBLIC);
    nvs_erase_key(nvs_handle, NVS_KEY_PAIRED);
    
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    // Clear runtime state
    pairing_state = PAIRING_STATE_UNPAIRED;
    has_stored_client_key = false;
    memset(esp32_private_key, 0, sizeof(esp32_private_key));
    memset(esp32_public_key, 0, sizeof(esp32_public_key));
    memset(stored_client_public_key, 0, sizeof(stored_client_public_key));
    clearECDH();
    
    Serial.println("NVS: Device unpaired - all pairing data erased");
    
    // Generate new ephemeral keys for next pairing
    return initECDH();
}

// Check if device is paired
bool CryptoManager::isDevicePaired() {
    return (pairing_state == PAIRING_STATE_PAIRED && has_stored_client_key);
}
