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

  // If not authorized, only 'request_token' or 'auth <token>' allowed
  if (!sessionAuthorized) {
    if (cmd.equalsIgnoreCase("request_token")) {
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

  // Initialize security system FIRST
  Serial.println("Initializing key manager...");
  if (!initKeyManager()) {
    Serial.println("ERROR: Key manager init failed!");
    while(1) delay(1000);
  }
  
  Serial.println("Starting key manager task...");
  startKeyManagerTask();
  
  Serial.println("Waiting for runtime key to be ready...");
  waitForRuntimeKeyReady();
  Serial.println("Runtime key generation completed successfully");


  
  // SD init
  SPI.begin(18, 19, 23, 5);  // SCK, MISO, MOSI, CS

  Serial.println("Initializing SD card...");
  if (!SD.begin(5, SPI, 8000000)) { // 8 MHz safe speed
    // updateOutput("SD init failed.");
    Serial.println("ERROR: SD card initialization failed");
    while (true) delay(1000);
  }
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
  if (wasConnected && pServer) {
    int connectedCount = pServer->getConnectedCount();
    
    if (connectedCount == 0) {
      updateOutput("Connection lost - resetting session");
      sessionAuthorized = false;
      sessionToken = "";
      wasConnected = false;
    }
    else if (millis() - lastActivityMs > CONNECTION_TIMEOUT_MS) {
      updateOutput("Connection timeout - no activity for 30s");
      sessionAuthorized = false;
      sessionToken = "";
    }
  }
  
  delay(1000);
}