#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLESecurity.h>
extern "C" {
  #include "sqlite3.h"
}

#define SERVICE_UUID              "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID       "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_RX_UUID    "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define SD_CS 5
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1  

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
sqlite3 *db;
sqlite3_stmt *res;
char *zErrMsg = 0;
int rc;
BLECharacteristic *pCharacteristic;
BLECharacteristic *pNotifyCharacteristic;

void displayMessage(const String &msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(msg.length() > 0 ? msg : "No msg");
  display.display();
  delay(1500);  // Wait so user can read the message
}

void handleCommand(String cmdLine) {
  displayMessage("Processing...");
  std::vector<String> tokens;
  int start = 0;
  int end = cmdLine.indexOf(' ');
  while (end != -1) {
    tokens.push_back(cmdLine.substring(start, end));
    start = end + 1;
    end = cmdLine.indexOf(' ', start);
  }
  tokens.push_back(cmdLine.substring(start));
  if (tokens.size() == 0) return;

  String cmd = tokens[0];
  String result;

  if (cmd == "add" && tokens.size() == 4) {
    String site = tokens[1], user = tokens[2], pass = tokens[3];
    String sql = "INSERT INTO credentials (site, username, password) VALUES ('" + site + "', '" + user + "', '" + pass + "');";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
    result = rc == SQLITE_OK ? "Added." : String(zErrMsg);
  } else if (cmd == "get" && tokens.size() == 3) {
    String site = tokens[1], user = tokens[2];
    String sql = "SELECT password FROM credentials WHERE site='" + site + "' AND username='" + user + "';";
    rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &res, 0);
    if (rc == SQLITE_OK && sqlite3_step(res) == SQLITE_ROW) {
      result = "Password: " + String((const char*)sqlite3_column_text(res, 0));
    } else {
      result = "Not found.";
    }
    sqlite3_finalize(res);
  } else if (cmd == "update" && tokens.size() == 4) {
    String site = tokens[1], user = tokens[2], pass = tokens[3];
    String sql = "UPDATE credentials SET password='" + pass + "' WHERE site='" + site + "' AND username='" + user + "';";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
    result = rc == SQLITE_OK ? "Updated." : String(zErrMsg);
  } else if (cmd == "delete" && tokens.size() == 3) {
    String site = tokens[1], user = tokens[2];
    String sql = "DELETE FROM credentials WHERE site='" + site + "' AND username='" + user + "';";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
    result = rc == SQLITE_OK ? "Deleted." : String(zErrMsg);
  } else if (cmd == "list") {
    String sql = "SELECT site, username FROM credentials;";
    rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &res, 0);
    result = "";
    while (sqlite3_step(res) == SQLITE_ROW) {
      result += "Site: " + String((const char*)sqlite3_column_text(res, 0));
      result += " | User: " + String((const char*)sqlite3_column_text(res, 1)) + "\n";
    }
    sqlite3_finalize(res);
  } else {
    result = "Invalid command or args.";
  }

  displayMessage(result);
  pNotifyCharacteristic->setValue(result.c_str());
  pNotifyCharacteristic->notify();
}

class CommandCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    String value = String(pChar->getValue().c_str());
    if (!value.isEmpty()) {
      handleCommand(value);
    }
  }
};

class MySecurity : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() override {
    displayMessage("Passkey Req: 123456");
    return 123456; // Return your static key if used
  }

  void onPassKeyNotify(uint32_t pass_key) override {
    displayMessage("Passkey Notify: " + String(pass_key));
  }

  bool onConfirmPIN(uint32_t pass_key) override {
    displayMessage("Confirm PIN: " + String(pass_key));
    return true; // Auto-accept
  }

  bool onSecurityRequest() override {
    displayMessage("Security Request");
    return true; // Accept security request
  }

  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
    if (cmpl.success) {
      displayMessage("Pairing Successful!");
    } else {
      displayMessage("Pairing Failed!");
    }
  }
};

void setup() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println(F("Booting..."));
  display.display();
  delay(1000);

  // Initialize SD card
  if (!SD.begin(SD_CS)) {
    displayMessage("SD init failed!");
    while (1);
  }
  displayMessage("SD init OK");

  // Initialize SQLite
  sqlite3_initialize();
  rc = sqlite3_open("/sd/credentials.db", &db);
  if (rc != SQLITE_OK) {
    displayMessage("Can't open DB");
    return;
  }
  displayMessage("DB open OK");

  const char *sql = "CREATE TABLE IF NOT EXISTS credentials ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "site TEXT, username TEXT, password TEXT);";
  rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
  if (rc != SQLITE_OK) {
    displayMessage("Table create error");
    return;
  }
  displayMessage("Table OK");

  // Setup BLE
  BLEDevice::init("ESP32-GATT-Manager");
  BLEDevice::setSecurityCallbacks(new MySecurity());

  // Configure BLE security
  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND); // Enable bonding
  pSecurity->setCapability(ESP_IO_CAP_OUT);          // Display-only capability
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  BLEServer *pServer = BLEDevice::createServer();

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharacteristic->setCallbacks(new CommandCallback());

  pNotifyCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_RX_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  displayMessage("BLE started");
}

void loop() {
  // BLE logic handled in callbacks
}