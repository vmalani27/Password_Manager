#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <vector>
#include <secure_core.h>
#include "mbedtls/ecdh.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/md.h"
#include "nvs_flash.h"
#include "nvs.h"

extern "C" {
  #include "sqlite3.h"
}
#include "esp_system.h"
#include "esp_gap_ble_api.h"

// Constants from oldcode.ino
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  
#define NOTIFICATION_CHARACTERISTIC_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define ECDH_PUBLIC_KEY_CHARACTERISTIC_UUID "6E400005-B5A3-F393-E0A9-E50E24DCCA9E"
#define SD_CS 5

// Global variables from oldcode.ino
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
sqlite3 *db;
sqlite3_stmt *res;
BLECharacteristic *pCharacteristic;
BLEServer *pServer;
char *zErrMsg = 0;
int rc;

unsigned long lastActivityMs = 0;
const unsigned long CONNECTION_TIMEOUT_MS = 30000;
bool wasConnected = false;

String sessionToken = "";
bool sessionAuthorized = false;
int failedAuthAttempts = 0;
const int MAX_FAILED_ATTEMPTS = 5;
unsigned long lockoutUntilMs = 0;
const unsigned long LOCKOUT_DURATION_MS = 60UL * 1000UL;

// ECDH state variables
uint8_t esp32_private_key[32];
uint8_t esp32_public_key[64];
uint8_t client_public_key[64];
uint8_t shared_secret[32];
uint8_t session_aes_key[32];
bool ecdh_ready = false;
uint8_t pending_challenge[16];
bool challenge_pending = false;

BLECharacteristic* pEcdhCharacteristic = nullptr;

// Device binding state
#define NVS_NAMESPACE "pwmgr"
#define NVS_KEY_ESP_PRIVATE "esp_priv"
#define NVS_KEY_ESP_PUBLIC "esp_pub"
#define NVS_KEY_CLIENT_PUBLIC "client_pub"
#define NVS_KEY_PAIRED "paired"

typedef enum {
    PAIRING_STATE_UNPAIRED = 0,
    PAIRING_STATE_PAIRED = 1
} pairing_state_t;

pairing_state_t pairing_state = PAIRING_STATE_UNPAIRED;
uint8_t stored_client_public_key[64];
bool has_stored_client_key = false;

// Function prototypes
void updateOutput(const String &msg);
void sendNotification(const String &data);
String generateSessionToken();
bool insertCredential(const String &site, const String &username, const String &password);
bool updateCredential(const String &site, const String &username, const String &password);
bool deleteCredential(const String &site, const String &username);
String getPassword(const String &site, const String &username);
String listCredentials();
void auditLog(const String &event, const String &user);
void handleCommand(String cmdLine);
bool initECDH();
bool computeSharedSecret(const uint8_t* client_pubkey);
void clearECDH();
bool loadOrGenerateKeys();
bool savePairing(const uint8_t* client_pubkey_to_save);
bool unpairDevice();
bool isDevicePaired();

// ============================================================================
// ECDH IMPLEMENTATION
// ============================================================================

// Initialize ECDH and generate ESP32's key pair
bool initECDH() {
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
bool computeSharedSecret(const uint8_t* client_pubkey) {
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
    
    // Zero out shared secret (only keep derived key)
    memset(shared_secret, 0, sizeof(shared_secret));
    
    ecdh_ready = true;
    
    mbedtls_ecdh_free(&ecdh_ctx);
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    
    return true;
}

// Clear ECDH session state on disconnect (but preserve NVS pairing data)
void clearECDH() {
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
bool loadOrGenerateKeys() {
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
bool savePairing(const uint8_t* client_pubkey_to_save) {
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
bool unpairDevice() {
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
bool isDevicePaired() {
    return (pairing_state == PAIRING_STATE_PAIRED);
}

// BLE callback classes (moved to top for proper declaration)
class MySecurityCallbacks : public BLESecurityCallbacks {
public:
  uint32_t onPassKeyRequest() override {
    updateOutput("BLE: Passkey requested\\nPIN: 123456");
    return 123456;
  }

  void onPassKeyNotify(uint32_t pass_key) override {
    updateOutput("BLE: Passkey " + String(pass_key));
  }

  bool onConfirmPIN(uint32_t pass_key) override {
    updateOutput("BLE: Confirm PIN " + String(pass_key));
    return true;
  }

  bool onSecurityRequest() override {
    updateOutput("BLE: Security request");
    return true;
  }

  void onAuthenticationComplete(esp_ble_auth_cmpl_t param) override {
    if (param.success) {
      updateOutput("BLE: Bonded successfully!");
    } else {
      if (sessionAuthorized) {
        updateOutput("BLE: Re-bonding failed (code " + String(param.fail_reason) + ") - continuing session");
      } else {
        updateOutput("BLE: Initial bonding failed (code " + String(param.fail_reason) + ")");
      }
    }
  }
};

class CommandCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    if (pChar) {
      String val = pChar->getValue().c_str();
      if (val.length() == 0) return;
      handleCommand(val);
    }
  }
};

class EcdhCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    
    if (rxValue.length() == 64) {
      Serial.println("ECDH: Received client public key");
      
      // Check if device is already paired
      if (isDevicePaired()) {
        // Verify this is the paired device
        if (has_stored_client_key && memcmp(rxValue.c_str(), stored_client_public_key, 64) == 0) {
          Serial.println("ECDH: Recognized paired device");
          memcpy(client_public_key, rxValue.c_str(), 64);
          
          // Compute shared secret with stored keys
          if (computeSharedSecret(client_public_key)) {
            sendNotification("ECDH_OK");
            Serial.println("ECDH: Paired device authenticated");
          } else {
            sendNotification("ECDH_FAIL");
            Serial.println("ECDH: Paired device auth failed");
          }
        } else {
          // Different device trying to connect
          Serial.println("ECDH: Rejected - device already paired to another client");
          sendNotification("ECDH_ALREADY_PAIRED");
        }
      } else {
        // Device is unpaired - accept new pairing
        Serial.println("ECDH: New pairing initiated");
        memcpy(client_public_key, rxValue.c_str(), 64);
        
        // Compute shared secret
        if (computeSharedSecret(client_public_key)) {
          // Save pairing to NVS
          if (savePairing(client_public_key)) {
            sendNotification("ECDH_OK_PAIRED");
            Serial.println("ECDH: New device paired successfully");
          } else {
            sendNotification("ECDH_OK");
            Serial.println("ECDH: Handshake OK but pairing save failed");
          }
        } else {
          sendNotification("ECDH_FAIL");
          Serial.println("ECDH: Handshake failed");
        }
      }
    } else {
      Serial.printf("ECDH: Invalid key length: %d\n", rxValue.length());
      sendNotification("ECDH_INVALID");
    }
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) {
    updateOutput("Client connected.");
    lastActivityMs = millis();
    wasConnected = true;
  }
  void onDisconnect(BLEServer* server) {
    updateOutput("Client disconnected.");
    server->getAdvertising()->start();
    sessionAuthorized = false;
    sessionToken = "";
    wasConnected = false;
    clearECDH(); // Clear ECDH state
    updateOutput("Session cleared on disconnect.");
  }
};

// Main functions
void updateOutput(const String &msg) {
  Serial.println(msg);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(msg);
  display.display();
}

void sendNotification(const String &data) {
  Serial.println("DEBUG: sendNotification called with: " + data);
  
  if (!pCharacteristic) {
    updateOutput("pCharacteristic is null.");
    return;
  }
  if (!pServer) {
    updateOutput("pServer is null.");
    return;
  }
  
  int connectedCount = pServer->getConnectedCount();
  Serial.println("DEBUG: Connected clients: " + String(connectedCount));
  
  if (connectedCount > 0) {
    pCharacteristic->setValue(data.c_str());
    pCharacteristic->notify();
    Serial.println("DEBUG: Notification sent: " + data);
    delay(10);
  } else {
    updateOutput("No BLE client connected.");
  }
}

String generateSessionToken() {
  uint32_t r = esp_random();
  char buf[16];
  snprintf(buf, sizeof(buf), "%08X", r);
  return String(buf);
}

// **MODIFIED: insertCredential with encryption**
bool insertCredential(const String &site, const String &username, const String &password) {
  // Encrypt password before storing
  uint8_t ciphertext[MAX_PASSWORD_ENCRYPTED_SIZE];
  uint8_t iv[IV_SIZE];
  size_t ciphertext_len;
  
  if (!encrypt_password(password, ciphertext, &ciphertext_len, iv)) {
    updateOutput("Encryption failed");
    return false;
  }
  
  const char* sql = "INSERT INTO credentials (site, username, encrypted_password, iv) VALUES (?, ?, ?, ?);";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    updateOutput("Prepare failed (insert): " + String(sqlite3_errmsg(db)));
    return false;
  }
  sqlite3_bind_text(stmt, 1, site.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_blob(stmt, 3, ciphertext, ciphertext_len, SQLITE_TRANSIENT);
  sqlite3_bind_blob(stmt, 4, iv, IV_SIZE, SQLITE_TRANSIENT);
  
  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  if (!ok) updateOutput("Insert step failed: " + String(sqlite3_errmsg(db)));
  sqlite3_finalize(stmt);
  return ok;
}

// **MODIFIED: updateCredential with encryption**
bool updateCredential(const String &site, const String &username, const String &password) {
  uint8_t ciphertext[MAX_PASSWORD_ENCRYPTED_SIZE];
  uint8_t iv[IV_SIZE];
  size_t ciphertext_len;
  
  if (!encrypt_password(password, ciphertext, &ciphertext_len, iv)) {
    updateOutput("Encryption failed");
    return false;
  }
  
  const char* sql = "UPDATE credentials SET encrypted_password=?, iv=? WHERE site=? AND username=?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    updateOutput("Prepare failed (update): " + String(sqlite3_errmsg(db)));
    return false;
  }
  sqlite3_bind_blob(stmt, 1, ciphertext, ciphertext_len, SQLITE_TRANSIENT);
  sqlite3_bind_blob(stmt, 2, iv, IV_SIZE, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, site.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, username.c_str(), -1, SQLITE_TRANSIENT);
  
  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  if (!ok) updateOutput("Update step failed: " + String(sqlite3_errmsg(db)));
  sqlite3_finalize(stmt);
  return ok;
}

// **MODIFIED: getPassword with decryption** 
String getPassword(const String &site, const String &username) {
  const char* sql = "SELECT encrypted_password, iv FROM credentials WHERE site=? AND username=?;";
  sqlite3_stmt *stmt = nullptr;
  String out = "";
  
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    updateOutput("Prepare failed (select): " + String(sqlite3_errmsg(db)));
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

// Functions from oldcode.ino (unchanged)
bool deleteCredential(const String &site, const String &username) {
  const char* sql = "DELETE FROM credentials WHERE site=? AND username=?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    updateOutput("Prepare failed (delete): " + String(sqlite3_errmsg(db)));
    return false;
  }
  sqlite3_bind_text(stmt, 1, site.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  if (!ok) updateOutput("Delete step failed: " + String(sqlite3_errmsg(db)));
  sqlite3_finalize(stmt);
  return ok;
}

String listCredentials() {
  const char* sql = "SELECT site, username FROM credentials;";
  sqlite3_stmt *stmt = nullptr;
  String out = "";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    updateOutput("Prepare failed (list): " + String(sqlite3_errmsg(db)));
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

void auditLog(const String &event, const String &user) {
  const char* sql = "INSERT INTO audit_log (timestamp, event, user) VALUES (?, ?, ?);";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    updateOutput("Audit prepare failed: " + String(sqlite3_errmsg(db)));
    return;
  }
  sqlite3_bind_int64(stmt, 1, (sqlite3_int64)time(NULL));
  sqlite3_bind_text(stmt, 2, event.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, user.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

// Command handling from oldcode.ino (unchanged)
void handleCommand(String cmdLine) {
  updateOutput("Processing: " + cmdLine);
  
  // Update activity timestamp
  lastActivityMs = millis();

  // Tokenize by spaces (simple)
  std::vector<String> tokens;
  char *tok = strtok((char*)cmdLine.c_str(), " ");
  while (tok != NULL) {
    tokens.push_back(String(tok));
    tok = strtok(NULL, " ");
  }
  if (tokens.size() == 0) return;

  String cmd = tokens[0];

  // Check lockout
  if (millis() < lockoutUntilMs) {
    updateOutput("Locked out until " + String(lockoutUntilMs));
    sendNotification("LOCKED");
    return;
  }

  // If not authorized, only 'request_token', 'auth <token>', or ECDH auth allowed
  if (!sessionAuthorized) {
    // ECDH challenge-response authentication
    if (cmd.equalsIgnoreCase("ecdh_auth")) {
      if (!ecdh_ready) {
        sendNotification("ECDH NOT READY");
        updateOutput("ECDH auth requested but ECDH not ready");
        return;
      }
      
      // Generate 16-byte challenge
      for (int i = 0; i < 16; i++) {
        pending_challenge[i] = esp_random() & 0xFF;
      }
      
      // Send challenge to client as hex
      String challengeHex = "";
      for (int i = 0; i < 16; i++) {
        char buf[3];
        sprintf(buf, "%02X", pending_challenge[i]);
        challengeHex += buf;
      }
      sendNotification("CHALLENGE " + challengeHex);
      challenge_pending = true;
      updateOutput("ECDH: Challenge sent");
      return;
    }
    else if (cmd.equalsIgnoreCase("respond") && tokens.size() == 2) {
      if (!challenge_pending) {
        sendNotification("NO CHALLENGE");
        updateOutput("Response received but no challenge pending");
        return;
      }
      
      // Client sends: "respond <HMAC_hex>"
      String responseHex = tokens[1];
      if (responseHex.length() != 64) { // 32 bytes = 64 hex chars
        sendNotification("INVALID RESPONSE");
        updateOutput("Invalid response length: " + String(responseHex.length()));
        return;
      }
      
      // Convert hex to bytes
      uint8_t client_hmac[32];
      for (int i = 0; i < 32; i++) {
        sscanf(responseHex.substring(i*2, i*2+2).c_str(), "%02hhx", &client_hmac[i]);
      }
      
      // Compute expected HMAC(session_key, challenge)
      uint8_t expected_hmac[32];
      mbedtls_md_context_t ctx;
      mbedtls_md_init(&ctx);
      
      const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
      mbedtls_md_setup(&ctx, info, 1);
      mbedtls_md_hmac_starts(&ctx, session_aes_key, 32);
      mbedtls_md_hmac_update(&ctx, pending_challenge, 16);
      mbedtls_md_hmac_finish(&ctx, expected_hmac);
      mbedtls_md_free(&ctx);
      
      // Compare
      if (memcmp(client_hmac, expected_hmac, 32) == 0) {
        sessionAuthorized = true;
        challenge_pending = false;
        failedAuthAttempts = 0;
        updateOutput("ECDH Auth: Success");
        sendNotification("AUTH OK");
        auditLog("ECDH_AUTH_SUCCESS", "ecdh_client");
      } else {
        failedAuthAttempts++;
        updateOutput("ECDH Auth: Invalid response (" + String(failedAuthAttempts) + ")");
        sendNotification("AUTH FAIL");
        auditLog("ECDH_AUTH_FAIL", "unknown");
        if (failedAuthAttempts >= MAX_FAILED_ATTEMPTS) {
          lockoutUntilMs = millis() + LOCKOUT_DURATION_MS;
          failedAuthAttempts = 0;
          updateOutput("Locked out for 1 minute");
        }
      }
      return;
    }
    // Legacy token-based auth (for backward compatibility)
    else if (cmd.equalsIgnoreCase("request_token")) {
      sessionToken = generateSessionToken();
      updateOutput("Generated token: " + sessionToken);
      sendNotification("TOKEN " + sessionToken);
      auditLog("TOKEN_ISSUED", "unknown");
      return;
    }
    else if (cmd.equalsIgnoreCase("auth") && tokens.size() == 2) {
      String provided = tokens[1];
      if (provided == sessionToken && sessionToken.length() > 0) {
        sessionAuthorized = true;
        failedAuthAttempts = 0;
        updateOutput("AUTH OK (legacy token)");
        sendNotification("AUTH OK");
        auditLog("AUTH_SUCCESS", "paired_client");
      } else {
        failedAuthAttempts++;
        updateOutput("AUTH FAIL (" + String(failedAuthAttempts) + ")");
        sendNotification("AUTH FAIL");
        auditLog("AUTH_FAIL", "unknown");
        if (failedAuthAttempts >= MAX_FAILED_ATTEMPTS) {
          lockoutUntilMs = millis() + LOCKOUT_DURATION_MS;
          failedAuthAttempts = 0;
          updateOutput("Too many failed auths. Locking for 1 min.");
          sendNotification("LOCKED");
        }
      }
      return;
    }
    else {
      updateOutput("Not authorized. Send 'request_token' then 'auth <token>'.");
      sendNotification("NOT AUTHORIZED");
      return;
    }
  }

  // From here onward, sessionAuthorized == true
  if (cmd.equalsIgnoreCase("add") && tokens.size() == 4) {
    bool ok = insertCredential(tokens[1], tokens[2], tokens[3]);
    updateOutput(ok ? "Added successfully" : "Error: Insert failed");
    sendNotification(ok ? "Added" : "ADD FAIL");
    auditLog(ok ? "ADD" : "ADD_FAIL", tokens[2]);
    return;
  }

  if (cmd.equalsIgnoreCase("get") && tokens.size() == 3) {
    String pw = getPassword(tokens[1], tokens[2]);
    if (pw.length()) {
      String resp = "Password: " + pw;
      updateOutput(resp);
      sendNotification(resp);
      auditLog("GET", tokens[2]);
    } else {
      updateOutput("Entry not found");
      sendNotification("NOT FOUND");
      auditLog("GET_MISS", tokens[2]);
    }
    return;
  }

  if (cmd.equalsIgnoreCase("update") && tokens.size() == 4) {
    bool ok = updateCredential(tokens[1], tokens[2], tokens[3]);
    updateOutput(ok ? "Updated successfully" : "Update failed");
    sendNotification(ok ? "Updated" : "UPDATE FAIL");
    auditLog(ok ? "UPDATE" : "UPDATE_FAIL", tokens[2]);
    return;
  }

  if (cmd.equalsIgnoreCase("delete") && tokens.size() == 3) {
    bool ok = deleteCredential(tokens[1], tokens[2]);
    updateOutput(ok ? "Deleted successfully" : "Delete failed");
    sendNotification(ok ? "Deleted" : "DELETE FAIL");
    auditLog(ok ? "DELETE" : "DELETE_FAIL", tokens[2]);
    return;
  }

  if (cmd.equalsIgnoreCase("list")) {
    String out = listCredentials();
    if (out.length() == 0) out = "(none)";
    updateOutput(out);
    sendNotification("LIST:\\n" + out);
    auditLog("LIST", "client");
    return;
  }

  if (cmd.equalsIgnoreCase("unpair")) {
    // Unpair device (requires authorization)
    if (unpairDevice()) {
      sessionAuthorized = false;
      sessionToken = "";
      updateOutput("Device unpaired");
      sendNotification("UNPAIRED");
      auditLog("UNPAIR", "client");
      // Restart advertising for new pairing
      if (pServer) {
        pServer->getAdvertising()->start();
      }
    } else {
      updateOutput("Unpair failed");
      sendNotification("UNPAIR_FAIL");
    }
    return;
  }

  if (cmd.equalsIgnoreCase("logout")) {
    sessionAuthorized = false;
    sessionToken = "";
    updateOutput("Logged out");
    sendNotification("LOGOUT");
    return;
  }

  // Unknown command
  updateOutput("Invalid command or wrong args");
  sendNotification("INVALID");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== ESP32 Password Manager Starting ===");

  // Initialize security system FIRST (SYNCHRONOUS)
  Serial.println("Initializing key manager...");
  if (!initKeyManager()) {
    Serial.println("ERROR: Key manager init failed!");
    while(1) delay(1000);
  }
  
  Serial.println("Deriving runtime encryption key...");
  if (!deriveRuntimeKey()) {
    Serial.println("ERROR: Runtime key derivation failed!");
    while(1) delay(1000);
  }
  Serial.println("Runtime key ready");

  // Initialize display
  Serial.println("Initializing OLED display...");
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("ERROR: SSD1306 allocation failed");
    for (;;);
  }
  Serial.println("Display initialized successfully");
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Initializing..."));
  display.display();

  // SD init
  SPI.begin(18, 19, 23, 5);  // SCK, MISO, MOSI, CS
  Serial.println("Initializing SD card...");
  if (!SD.begin(5, SPI, 8000000)) {
    updateOutput("SD init failed.");
    Serial.println("ERROR: SD card initialization failed");
    while (true) delay(1000);
  }
  updateOutput("SD initialized.");
  Serial.println("SD card initialized successfully");
  delay(200);

  // SQLite init with encrypted schema
  Serial.println("Initializing SQLite database...");
  sqlite3_initialize();
  rc = sqlite3_open("/sd/credentials.db", &db);
  if (rc != SQLITE_OK) {
    String error = "DB open failed: " + String(sqlite3_errmsg(db));
    updateOutput(error);
    Serial.println("ERROR: " + error);
  } else {
    Serial.println("Database opened successfully");
    
    // Create tables with encrypted schema
    Serial.println("Creating database tables...");
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
      Serial.println("ERROR creating credentials table: " + String(zErrMsg));
      sqlite3_free(zErrMsg);
      zErrMsg = 0;
    } else {
      Serial.println("Credentials table created/verified");
    }
    
    sqlite3_exec(db, create_audit, 0, 0, &zErrMsg);
    if (zErrMsg) {
      Serial.println("ERROR creating audit_log table: " + String(zErrMsg));
      sqlite3_free(zErrMsg);
      zErrMsg = 0;
    } else {
      Serial.println("Audit log table created/verified");
    }
  }

  // BLE initialization
  Serial.println("Initializing BLE...");
  updateOutput("Initializing BLE...");
  BLEDevice::init("ESP32-PWD-Manager");
  delay(200);
  Serial.println("BLE device initialized");
  
  // Set security callbacks
  Serial.println("Setting up BLE security...");
  BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
  
  // Set static passkey
  const uint32_t DEV_PASSKEY = 123456;
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, (void*)&DEV_PASSKEY, sizeof(uint32_t));
  
  // Configure security parameters
  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
  esp_ble_io_cap_t iocap = ESP_IO_CAP_OUT;
  uint8_t key_size = 16;
  uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  
  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
  
  Serial.println("BLE security configured (PIN: 123456)");
  
  // Create BLE server
  Serial.println("Creating BLE server...");
  pServer = BLEDevice::createServer();
  if (!pServer) {
    Serial.println("ERROR: Failed to create BLE server");
    updateOutput("Failed create BLE server");
    while(true) delay(1000);
  }
  pServer->setCallbacks(new ServerCallbacks());
  Serial.println("BLE server created");

  // Create BLE service
  Serial.println("Creating BLE service...");
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  // RX characteristic (write from client)
  Serial.println("Creating RX characteristic...");
  BLECharacteristic *rxChar = pService->createCharacteristic(
    CHARACTERISTIC_UUID, 
    BLECharacteristic::PROPERTY_WRITE
  );
  rxChar->setCallbacks(new CommandCallback());
  
  // TX characteristic (notifications to client)
  Serial.println("Creating TX characteristic...");
  pCharacteristic = pService->createCharacteristic(
    NOTIFICATION_CHARACTERISTIC_UUID, 
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
  );
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setValue("Ready");
  
  // ECDH characteristic (read for ESP32 public key, write for client public key)
  Serial.println("Creating ECDH characteristic...");
  pEcdhCharacteristic = pService->createCharacteristic(
    ECDH_PUBLIC_KEY_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
  pEcdhCharacteristic->setCallbacks(new EcdhCallback());
  
  // Load or generate ECDH keys from NVS (handles paired/unpaired state)
  Serial.println("Loading device pairing state...");
  if (loadOrGenerateKeys()) {
    pEcdhCharacteristic->setValue(esp32_public_key, 64);
    if (isDevicePaired()) {
      Serial.println("ECDH: Device is PAIRED - public key set in characteristic");
      updateOutput("Device PAIRED");
    } else {
      Serial.println("ECDH: Device is UNPAIRED - public key set in characteristic");
      updateOutput("Device UNPAIRED");
    }
  } else {
    Serial.println("ERROR: ECDH initialization failed");
    updateOutput("ECDH init failed");
  }
  
  Serial.println("Starting BLE service...");
  pService->start();
  
  Serial.println("Starting BLE advertising...");
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  BLEAdvertisementData advData;
  BLEAdvertisementData scanResp;

  // Primary advertisement packet
  advData.setName("ESP32-PWD-Manager");
  advData.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
  advData.setCompleteServices(BLEUUID(SERVICE_UUID));

  // Scan response (some phones *require* this)
  scanResp.setName("ESP32-PWD-Manager");

  // Apply packets
  pAdvertising->setAdvertisementData(advData);
  pAdvertising->setScanResponseData(scanResp);

  // Connection params (safe defaults)
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);

  // Start advertising
  BLEDevice::startAdvertising();
  Serial.println("BLE advertising started");
  
  Serial.println("Setting BLE MTU...");
  BLEDevice::setMTU(256);

  updateOutput("System ready with encryption!");
  Serial.println("\n=== System Ready ===");
  Serial.println("BLE Name: ESP32-PWD-Manager");
  Serial.println("PIN: 123456");
  Serial.println("Waiting for connections...\n");
  
  lastActivityMs = millis();
}

void loop() {
  // Simple disconnect handling - timeout logic removed until ECDH is implemented
  // TODO Phase 1.2: Add proper reconnection with challenge-response after ECDH
  if (wasConnected && pServer) {
    int connectedCount = pServer->getConnectedCount();
    
    if (connectedCount == 0) {
      updateOutput("Connection lost - resetting session");
      sessionAuthorized = false;
      sessionToken = "";
      wasConnected = false;
    }
  }
  
  delay(1000);
}