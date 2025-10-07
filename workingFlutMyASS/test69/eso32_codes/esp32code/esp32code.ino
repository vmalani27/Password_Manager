/*
  App-level secure flow for ESP32 (Bluedroid / BLEDevice)
  - session token + AUTH command
  - prepared statements for SQL (prevent injection)
  - rate limiting + lockout on failed AUTH attempts
  - basic audit logging table
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

// ----- SQLite helpers (prepared statements) -----
// Note: these wrapper functions inline parameterized queries (safe)
bool insertCredential(const String &site, const String &username, const String &password) {
  const char* sql = "INSERT INTO credentials (site, username, password) VALUES (?, ?, ?);";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    updateOutput("Prepare failed (insert): " + String(sqlite3_errmsg(db)));
    return false;
  }
  sqlite3_bind_text(stmt, 1, site.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, password.c_str(), -1, SQLITE_TRANSIENT);
  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  if (!ok) updateOutput("Insert step failed: " + String(sqlite3_errmsg(db)));
  sqlite3_finalize(stmt);
  return ok;
}

bool updateCredential(const String &site, const String &username, const String &password) {
  const char* sql = "UPDATE credentials SET password=? WHERE site=? AND username=?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    updateOutput("Prepare failed (update): " + String(sqlite3_errmsg(db)));
    return false;
  }
  sqlite3_bind_text(stmt, 1, password.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, site.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, username.c_str(), -1, SQLITE_TRANSIENT);
  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  if (!ok) updateOutput("Update step failed: " + String(sqlite3_errmsg(db)));
  sqlite3_finalize(stmt);
  return ok;
}

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

// Get password (returns empty string on not found or error)
String getPassword(const String &site, const String &username) {
  const char* sql = "SELECT password FROM credentials WHERE site=? AND username=?;";
  sqlite3_stmt *stmt = nullptr;
  String out = "";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    updateOutput("Prepare failed (select): " + String(sqlite3_errmsg(db)));
    return out;
  }
  sqlite3_bind_text(stmt, 1, site.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char* txt = sqlite3_column_text(stmt, 0);
    if (txt) out = String((const char*)txt);
  }
  sqlite3_finalize(stmt);
  return out;
}

// List credentials (site | username pairs); returns multi-line string
String listCredentials() {
  const char* sql = "SELECT site, username FROM credentials;";
  sqlite3_stmt *stmt = nullptr;
  String out = "";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    updateOutput("Prepare failed (list): " + String(sqlite3_errmsg(db)));
    return out;
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char* s = sqlite3_column_text(stmt, 0);
    const unsigned char* u = sqlite3_column_text(stmt, 1);
    if (s && u) {
      out += "Site: " + String((const char*)s) + " | User: " + String((const char*)u) + "\n";
    }
  }
  sqlite3_finalize(stmt);
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

  // SQLite
  sqlite3_initialize();
  rc = sqlite3_open("/sd/credentials.db", &db);
  if (rc != SQLITE_OK) {
    updateOutput("DB open failed: " + String(sqlite3_errmsg(db)));
    // continue but DB won't work
  } else {
    // create tables if not exists
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS credentials (id INTEGER PRIMARY KEY AUTOINCREMENT, site TEXT, username TEXT, password TEXT);", 0, 0, &zErrMsg);
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS audit_log (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER, event TEXT, user TEXT);", 0, 0, &zErrMsg);
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
