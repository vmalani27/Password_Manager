/*
  App-level secure flow for ESP32 (Bluedroid / BLEDevice)
  - session token + AUTH command
  - prepared statements for SQL (prevent injection)
  - rate limiting + lockout on failed AUTH attempts
  - basic audit logging table
  - AES-256-GCM credential encryption at rest
  - keeps existing OLED/notification behavior
*/

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
extern "C" {
  #include "sqlite3.h"
}
#include "esp_system.h" // for esp_random()

// AES-256 Encryption support
#include "encryption_utils.h"
#include "key_manager.h"
#include "database_migration.h"

// ----- Config -----
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NOTIFICATION_CHARACTERISTIC_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define SD_CS 5

// ----- Display / BLE Globals -----
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
sqlite3 *db;
sqlite3_stmt *res; // general stmt variable (careful to finalize)
BLECharacteristic *pCharacteristic;
BLEServer *pServer;
char *zErrMsg = 0;
int rc;

// ----- Connection monitoring -----
unsigned long lastActivityMs = 0;
const unsigned long CONNECTION_TIMEOUT_MS = 30000; // 30 seconds
bool wasConnected = false;

// ----- Session & Rate limiting -----
String sessionToken = "";
bool sessionAuthorized = false;
int failedAuthAttempts = 0;
const int MAX_FAILED_ATTEMPTS = 5;
unsigned long lockoutUntilMs = 0; // if millis() < lockoutUntilMs, locked out
const unsigned long LOCKOUT_DURATION_MS = 60UL * 1000UL; // 1 minute

// Helper: update OLED + Serial
void updateOutput(const String &msg) {
  Serial.println(msg);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  // display only first two lines to avoid overflow
  display.println(msg);
  display.display();
}

// BLE notification helper
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

// Utility: generate a random hex session token (8 hex chars)
String generateSessionToken() {
  uint32_t r = esp_random(); // good hardware RNG
  char buf[16];
  snprintf(buf, sizeof(buf), "%08X", r);
  return String(buf);
}

// ----- SQLite helpers (prepared statements with AES-256 encryption) -----
// Note: these wrapper functions use encrypted storage with authenticated encryption
bool insertCredential(const String &site, const String &username, const String &password) {
  // Get master key for encryption
  uint8_t masterKey[MASTER_KEY_SIZE];
  if (!getMasterKey(masterKey)) {
    updateOutput("Failed to get master key for insert");
    return false;
  }
  
  // Encrypt each field separately
  String site_encrypted, site_iv, site_tag;
  String username_encrypted, username_iv, username_tag;
  String password_encrypted, password_iv, password_tag;
  
  bool encrypt_success = true;
  encrypt_success &= encryptCredentialField(site, masterKey, CONTEXT_SITE, 
                                           site_encrypted, site_iv, site_tag);
  encrypt_success &= encryptCredentialField(username, masterKey, CONTEXT_USERNAME,
                                           username_encrypted, username_iv, username_tag);
  encrypt_success &= encryptCredentialField(password, masterKey, CONTEXT_PASSWORD,
                                           password_encrypted, password_iv, password_tag);
  
  // Clear master key from memory
  secureZero(masterKey, sizeof(masterKey));
  
  if (!encrypt_success) {
    updateOutput("Encryption failed during insert");
    return false;
  }
  
  // Insert encrypted data
  const char* sql = 
    "INSERT INTO credentials "
    "(site_encrypted, site_iv, site_tag, "
    " username_encrypted, username_iv, username_tag, "
    " password_encrypted, password_iv, password_tag) "
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
  
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    updateOutput("Prepare failed (insert): " + String(sqlite3_errmsg(db)));
    return false;
  }
  
  sqlite3_bind_text(stmt, 1, site_encrypted.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, site_iv.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, site_tag.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, username_encrypted.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, username_iv.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, username_tag.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, password_encrypted.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 8, password_iv.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 9, password_tag.c_str(), -1, SQLITE_TRANSIENT);
  
  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  if (!ok) updateOutput("Insert step failed: " + String(sqlite3_errmsg(db)));
  sqlite3_finalize(stmt);
  return ok;
}

bool updateCredential(const String &site, const String &username, const String &password) {
  // Get master key for encryption
  uint8_t masterKey[MASTER_KEY_SIZE];
  if (!getMasterKey(masterKey)) {
    updateOutput("Failed to get master key for update");
    return false;
  }
  
  // Encrypt the new password
  String password_encrypted, password_iv, password_tag;
  bool encrypt_success = encryptCredentialField(password, masterKey, CONTEXT_PASSWORD,
                                               password_encrypted, password_iv, password_tag);
  
  if (!encrypt_success) {
    secureZero(masterKey, sizeof(masterKey));
    updateOutput("Password encryption failed during update");
    return false;
  }
  
  // For site and username matching, we need to encrypt them to compare
  String site_encrypted, site_iv, site_tag;
  String username_encrypted, username_iv, username_tag;
  
  encrypt_success &= encryptCredentialField(site, masterKey, CONTEXT_SITE,
                                           site_encrypted, site_iv, site_tag);
  encrypt_success &= encryptCredentialField(username, masterKey, CONTEXT_USERNAME,
                                           username_encrypted, username_iv, username_tag);
  
  // Clear master key from memory
  secureZero(masterKey, sizeof(masterKey));
  
  if (!encrypt_success) {
    updateOutput("Site/username encryption failed during update");
    return false;
  }
  
  // Update by finding matching encrypted site and username
  // Note: This is complex with encrypted data, so we'll do a search and update approach
  const char* select_sql = 
    "SELECT id, site_encrypted, site_iv, site_tag, "
    "        username_encrypted, username_iv, username_tag "
    "FROM credentials;";
  
  sqlite3_stmt *select_stmt = nullptr;
  if (sqlite3_prepare_v2(db, select_sql, -1, &select_stmt, nullptr) != SQLITE_OK) {
    updateOutput("Prepare failed (update select): " + String(sqlite3_errmsg(db)));
    return false;
  }
  
  int target_id = -1;
  
  // Find the matching record by decrypting and comparing
  if (!getMasterKey(masterKey)) {
    updateOutput("Failed to get master key for update search");
    sqlite3_finalize(select_stmt);
    return false;
  }
  
  while (sqlite3_step(select_stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(select_stmt, 0);
    const char* stored_site_enc = (const char*)sqlite3_column_text(select_stmt, 1);
    const char* stored_site_iv = (const char*)sqlite3_column_text(select_stmt, 2);
    const char* stored_site_tag = (const char*)sqlite3_column_text(select_stmt, 3);
    const char* stored_username_enc = (const char*)sqlite3_column_text(select_stmt, 4);
    const char* stored_username_iv = (const char*)sqlite3_column_text(select_stmt, 5);
    const char* stored_username_tag = (const char*)sqlite3_column_text(select_stmt, 6);
    
    if (!stored_site_enc || !stored_site_iv || !stored_site_tag ||
        !stored_username_enc || !stored_username_iv || !stored_username_tag) {
      continue;
    }
    
    // Decrypt stored values and compare
    String decrypted_site, decrypted_username;
    bool decrypt_success = true;
    decrypt_success &= decryptCredentialField(String(stored_site_enc), String(stored_site_iv), 
                                             String(stored_site_tag), masterKey, CONTEXT_SITE, decrypted_site);
    decrypt_success &= decryptCredentialField(String(stored_username_enc), String(stored_username_iv),
                                             String(stored_username_tag), masterKey, CONTEXT_USERNAME, decrypted_username);
    
    if (decrypt_success && decrypted_site == site && decrypted_username == username) {
      target_id = id;
      break;
    }
  }
  
  sqlite3_finalize(select_stmt);
  secureZero(masterKey, sizeof(masterKey));
  
  if (target_id == -1) {
    updateOutput("No matching credential found for update");
    return false;
  }
  
  // Update the password for the found record
  const char* update_sql = 
    "UPDATE credentials SET "
    "password_encrypted=?, password_iv=?, password_tag=? "
    "WHERE id=?;";
  
  sqlite3_stmt *update_stmt = nullptr;
  if (sqlite3_prepare_v2(db, update_sql, -1, &update_stmt, nullptr) != SQLITE_OK) {
    updateOutput("Prepare failed (update): " + String(sqlite3_errmsg(db)));
    return false;
  }
  
  sqlite3_bind_text(update_stmt, 1, password_encrypted.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(update_stmt, 2, password_iv.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(update_stmt, 3, password_tag.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(update_stmt, 4, target_id);
  
  bool ok = (sqlite3_step(update_stmt) == SQLITE_DONE);
  if (!ok) updateOutput("Update step failed: " + String(sqlite3_errmsg(db)));
  sqlite3_finalize(update_stmt);
  return ok;
}

bool deleteCredential(const String &site, const String &username) {
  // Get master key for decryption during search
  uint8_t masterKey[MASTER_KEY_SIZE];
  if (!getMasterKey(masterKey)) {
    updateOutput("Failed to get master key for delete");
    return false;
  }
  
  // Search for the matching record by decrypting stored values
  const char* select_sql = 
    "SELECT id, site_encrypted, site_iv, site_tag, "
    "        username_encrypted, username_iv, username_tag "
    "FROM credentials;";
  
  sqlite3_stmt *select_stmt = nullptr;
  if (sqlite3_prepare_v2(db, select_sql, -1, &select_stmt, nullptr) != SQLITE_OK) {
    updateOutput("Prepare failed (delete select): " + String(sqlite3_errmsg(db)));
    secureZero(masterKey, sizeof(masterKey));
    return false;
  }
  
  int target_id = -1;
  
  while (sqlite3_step(select_stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(select_stmt, 0);
    const char* stored_site_enc = (const char*)sqlite3_column_text(select_stmt, 1);
    const char* stored_site_iv = (const char*)sqlite3_column_text(select_stmt, 2);
    const char* stored_site_tag = (const char*)sqlite3_column_text(select_stmt, 3);
    const char* stored_username_enc = (const char*)sqlite3_column_text(select_stmt, 4);
    const char* stored_username_iv = (const char*)sqlite3_column_text(select_stmt, 5);
    const char* stored_username_tag = (const char*)sqlite3_column_text(select_stmt, 6);
    
    if (!stored_site_enc || !stored_site_iv || !stored_site_tag ||
        !stored_username_enc || !stored_username_iv || !stored_username_tag) {
      continue;
    }
    
    // Decrypt stored values and compare
    String decrypted_site, decrypted_username;
    bool decrypt_success = true;
    decrypt_success &= decryptCredentialField(String(stored_site_enc), String(stored_site_iv), 
                                             String(stored_site_tag), masterKey, CONTEXT_SITE, decrypted_site);
    decrypt_success &= decryptCredentialField(String(stored_username_enc), String(stored_username_iv),
                                             String(stored_username_tag), masterKey, CONTEXT_USERNAME, decrypted_username);
    
    if (decrypt_success && decrypted_site == site && decrypted_username == username) {
      target_id = id;
      break;
    }
  }
  
  sqlite3_finalize(select_stmt);
  secureZero(masterKey, sizeof(masterKey));
  
  if (target_id == -1) {
    updateOutput("No matching credential found for delete");
    return false;
  }
  
  // Delete the found record
  const char* delete_sql = "DELETE FROM credentials WHERE id=?;";
  sqlite3_stmt *delete_stmt = nullptr;
  if (sqlite3_prepare_v2(db, delete_sql, -1, &delete_stmt, nullptr) != SQLITE_OK) {
    updateOutput("Prepare failed (delete): " + String(sqlite3_errmsg(db)));
    return false;
  }
  
  sqlite3_bind_int(delete_stmt, 1, target_id);
  bool ok = (sqlite3_step(delete_stmt) == SQLITE_DONE);
  if (!ok) updateOutput("Delete step failed: " + String(sqlite3_errmsg(db)));
  sqlite3_finalize(delete_stmt);
  return ok;
}

// Get password (returns empty string on not found or error)
String getPassword(const String &site, const String &username) {
  // Get master key for decryption during search
  uint8_t masterKey[MASTER_KEY_SIZE];
  if (!getMasterKey(masterKey)) {
    updateOutput("Failed to get master key for password retrieval");
    return "";
  }
  
  // Search for the matching record by decrypting stored values
  const char* select_sql = 
    "SELECT site_encrypted, site_iv, site_tag, "
    "       username_encrypted, username_iv, username_tag, "
    "       password_encrypted, password_iv, password_tag "
    "FROM credentials;";
  
  sqlite3_stmt *stmt = nullptr;
  String result = "";
  
  if (sqlite3_prepare_v2(db, select_sql, -1, &stmt, nullptr) != SQLITE_OK) {
    updateOutput("Prepare failed (password select): " + String(sqlite3_errmsg(db)));
    secureZero(masterKey, sizeof(masterKey));
    return result;
  }
  
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* stored_site_enc = (const char*)sqlite3_column_text(stmt, 0);
    const char* stored_site_iv = (const char*)sqlite3_column_text(stmt, 1);
    const char* stored_site_tag = (const char*)sqlite3_column_text(stmt, 2);
    const char* stored_username_enc = (const char*)sqlite3_column_text(stmt, 3);
    const char* stored_username_iv = (const char*)sqlite3_column_text(stmt, 4);
    const char* stored_username_tag = (const char*)sqlite3_column_text(stmt, 5);
    const char* stored_password_enc = (const char*)sqlite3_column_text(stmt, 6);
    const char* stored_password_iv = (const char*)sqlite3_column_text(stmt, 7);
    const char* stored_password_tag = (const char*)sqlite3_column_text(stmt, 8);
    
    if (!stored_site_enc || !stored_site_iv || !stored_site_tag ||
        !stored_username_enc || !stored_username_iv || !stored_username_tag ||
        !stored_password_enc || !stored_password_iv || !stored_password_tag) {
      continue;
    }
    
    // Decrypt stored site and username to compare
    String decrypted_site, decrypted_username;
    bool decrypt_success = true;
    decrypt_success &= decryptCredentialField(String(stored_site_enc), String(stored_site_iv), 
                                             String(stored_site_tag), masterKey, CONTEXT_SITE, decrypted_site);
    decrypt_success &= decryptCredentialField(String(stored_username_enc), String(stored_username_iv),
                                             String(stored_username_tag), masterKey, CONTEXT_USERNAME, decrypted_username);
    
    if (decrypt_success && decrypted_site == site && decrypted_username == username) {
      // Found matching record, decrypt and return password
      String decrypted_password;
      if (decryptCredentialField(String(stored_password_enc), String(stored_password_iv),
                                 String(stored_password_tag), masterKey, CONTEXT_PASSWORD, decrypted_password)) {
        result = decrypted_password;
        break;
      } else {
        updateOutput("Failed to decrypt password");
        break;
      }
    }
  }
  
  sqlite3_finalize(stmt);
  secureZero(masterKey, sizeof(masterKey));
  return result;
}

// List credentials (site | username pairs); returns multi-line string
String listCredentials() {
  // Get master key for decryption
  uint8_t masterKey[MASTER_KEY_SIZE];
  if (!getMasterKey(masterKey)) {
    updateOutput("Failed to get master key for credential listing");
    return "";
  }
  
  const char* sql = 
    "SELECT site_encrypted, site_iv, site_tag, "
    "       username_encrypted, username_iv, username_tag "
    "FROM credentials;";
  
  sqlite3_stmt *stmt = nullptr;
  String out = "";
  
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    updateOutput("Prepare failed (list): " + String(sqlite3_errmsg(db)));
    secureZero(masterKey, sizeof(masterKey));
    return out;
  }
  
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* site_enc = (const char*)sqlite3_column_text(stmt, 0);
    const char* site_iv = (const char*)sqlite3_column_text(stmt, 1);
    const char* site_tag = (const char*)sqlite3_column_text(stmt, 2);
    const char* username_enc = (const char*)sqlite3_column_text(stmt, 3);
    const char* username_iv = (const char*)sqlite3_column_text(stmt, 4);
    const char* username_tag = (const char*)sqlite3_column_text(stmt, 5);
    
    if (!site_enc || !site_iv || !site_tag ||
        !username_enc || !username_iv || !username_tag) {
      continue;
    }
    
    // Decrypt site and username for display
    String decrypted_site, decrypted_username;
    bool decrypt_success = true;
    decrypt_success &= decryptCredentialField(String(site_enc), String(site_iv), 
                                             String(site_tag), masterKey, CONTEXT_SITE, decrypted_site);
    decrypt_success &= decryptCredentialField(String(username_enc), String(username_iv),
                                             String(username_tag), masterKey, CONTEXT_USERNAME, decrypted_username);
    
    if (decrypt_success) {
      out += "Site: " + decrypted_site + " | User: " + decrypted_username + "\n";
    } else {
      out += "Site: [decrypt failed] | User: [decrypt failed]\n";
    }
  }
  
  sqlite3_finalize(stmt);
  secureZero(masterKey, sizeof(masterKey));
  return out;
}

// Audit log insertion
void auditLog(const String &event, const String &user) {
  const char* sql = "INSERT INTO audit_log (timestamp, event, user) VALUES (?, ?, ?);";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    // non-fatal
    updateOutput("Audit prepare failed: " + String(sqlite3_errmsg(db)));
    return;
  }
  sqlite3_bind_int64(stmt, 1, (sqlite3_int64)time(NULL));
  sqlite3_bind_text(stmt, 2, event.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, user.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

// ----- Command handling -----
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

  // If not authorized, only 'request_token' or 'auth <token>' allowed
  if (!sessionAuthorized) {
    if (cmd.equalsIgnoreCase("request_token")) {
      sessionToken = generateSessionToken();
      updateOutput("Generated token: " + sessionToken);
      sendNotification("TOKEN " + sessionToken); // send token to app via notification
      auditLog("TOKEN_ISSUED", "unknown");
      return;
    }
    else if (cmd.equalsIgnoreCase("auth") && tokens.size() == 2) {
      String provided = tokens[1];
      if (provided == sessionToken && sessionToken.length() > 0) {
        sessionAuthorized = true;
        failedAuthAttempts = 0;
        updateOutput("AUTH OK");
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
  // Implement commands: add/get/update/delete/list
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
    sendNotification("LIST:\n" + out);
    auditLog("LIST", "client");
    return;
  }

  if (cmd.equalsIgnoreCase("logout")) {
    sessionAuthorized = false;
    sessionToken = "";
    updateOutput("Logged out");
    sendNotification("LOGOUT");
    return;
  }

  // Unknown
  updateOutput("Invalid command or wrong args");
  sendNotification("INVALID");
}

// ----- BLE callbacks -----
// Security callbacks for BLE pairing
class MySecurityCallbacks : public BLESecurityCallbacks {
public:
  uint32_t onPassKeyRequest() override {
    updateOutput("BLE: Passkey requested\nPIN: 123456");
    return 123456;
  }

  void onPassKeyNotify(uint32_t pass_key) override {
    updateOutput("BLE: Passkey " + String(pass_key));
  }

  bool onConfirmPIN(uint32_t pass_key) override {
    updateOutput("BLE: Confirm PIN " + String(pass_key));
    return true; // Auto-accept for development
  }

  bool onSecurityRequest() override {
    updateOutput("BLE: Security request");
    return true; // Allow bonding
  }

  void onAuthenticationComplete(esp_ble_auth_cmpl_t param) override {
    if (param.success) {
      updateOutput("BLE: Bonded successfully!");
    } else {
      // Don't treat bonding failures as fatal if session is already working
      if (sessionAuthorized) {
        updateOutput("BLE: Re-bonding failed (code " + String(param.fail_reason) + ") - continuing session");
      } else {
        updateOutput("BLE: Initial bonding failed (code " + String(param.fail_reason) + ")");
      }
    }
  }
};

// Command callbacks
class CommandCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    if (pChar) {
      String val = pChar->getValue().c_str();
      if (val.length() == 0) return;
      handleCommand(val);
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
    // Reset authorization on disconnect
    sessionAuthorized = false;
    sessionToken = "";
    wasConnected = false;
    updateOutput("Session cleared on disconnect.");
  }
};

// ----- Setup / loop -----
void setup() {
  Serial.begin(115200);

  // Initialize display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Initializing..."));
  display.display();

  // SD init
  if (!SD.begin(SD_CS)) {
    updateOutput("SD init failed.");
    while (true); // halt if SD not present (your original behavior)
  }
  updateOutput("SD initialized.");
  delay(200);

  // Initialize encryption system
  updateOutput("Initializing encryption...");
  if (!initEncryption()) {
    updateOutput("Encryption init failed!");
    while (true); // halt if encryption fails
  }
  if (!initKeyManager()) {
    updateOutput("Key manager init failed!");
    while (true); // halt if key manager fails
  }
  updateOutput("Encryption ready.");
  delay(200);

  // SQLite
  sqlite3_initialize();
  rc = sqlite3_open("/sd/credentials.db", &db);
  if (rc != SQLITE_OK) {
    updateOutput("DB open failed: " + String(sqlite3_errmsg(db)));
    // continue but DB won't work
  } else {
    // Initialize database with encryption migration
    updateOutput("Setting up database...");
    if (!initializeDatabase(db)) {
      updateOutput("DB migration failed!");
      // continue but note the error
    } else {
      updateOutput("Database ready (encrypted).");
    }
  }

  // BLE init (Bluedroid)
  updateOutput("Initializing BLE...");
  BLEDevice::init("ESP32-GATT-Manager");
  delay(200);
  
  // ============ BLE SECURITY CONFIGURATION ============
  // Set security callbacks
  BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());
  
  // Set encryption level
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
  
  // Set static passkey (123456)
  const uint32_t DEV_PASSKEY = 123456;
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, (void*)&DEV_PASSKEY, sizeof(uint32_t));
  
  // Configure security parameters: Secure Connections + MITM + Bonding
  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
  esp_ble_io_cap_t iocap = ESP_IO_CAP_OUT; // Display only (ESP32 shows PIN)
  uint8_t key_size = 16;
  uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  
  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
  
  updateOutput("BLE Security enabled (PIN: 123456)");
  // ====================================================
  
  pServer = BLEDevice::createServer();
  if (!pServer) { updateOutput("Failed create BLE server"); while(true); }
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *rxChar = pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
  rxChar->setCallbacks(new CommandCallback());

  pCharacteristic = pService->createCharacteristic(NOTIFICATION_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setValue("Ready");

  pService->start();
  BLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
  BLEDevice::getAdvertising()->start();
  BLEDevice::setMTU(256);

  updateOutput("BLE running. Waiting for client...");
}

void loop() {
  // Monitor connection status and detect timeouts
  if (wasConnected && pServer) {
    int connectedCount = pServer->getConnectedCount();
    
    // Check if client disconnected without triggering onDisconnect
    if (connectedCount == 0) {
      updateOutput("Connection lost - resetting session");
      sessionAuthorized = false;
      sessionToken = "";
      wasConnected = false;
    }
    // Check for activity timeout
    else if (millis() - lastActivityMs > CONNECTION_TIMEOUT_MS) {
      updateOutput("Connection timeout - no activity for 30s");
      sessionAuthorized = false;
      sessionToken = "";
      // Note: Don't set wasConnected = false here, let the actual disconnect handle it
    }
  }
  
  delay(1000); // Check every second
}
