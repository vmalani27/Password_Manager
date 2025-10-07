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

// Custom service and characteristic UUIDs
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
BLEServer* pServer = NULL;
BLEService* pService = NULL;
BLECharacteristic *pCharacteristic;
BLECharacteristic *pNotifyCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Store bonded device address
String bondedDeviceAddress = "";

// Helper function to convert Bluetooth address to string
String btAddressToString(esp_bd_addr_t addr) {
  char str[18];
  sprintf(str, "%02X:%02X:%02X:%02X:%02X:%02X",
          addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
  return String(str);
}

void displayMessage(const String &msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(msg.length() > 0 ? msg : "No msg");
  display.display();
  delay(1500);  // Wait so user can read the message
}

void handleCommand(String cmdLine) {
  // Only process commands if device is bonded
  if (bondedDeviceAddress.length() == 0) {
    displayMessage("Not bonded!");
    return;
  }

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
    displayMessage("Passkey: 123456");
    return 123456;
  }

  void onPassKeyNotify(uint32_t pass_key) override {
    displayMessage("Passkey: " + String(pass_key));
  }

  bool onConfirmPIN(uint32_t pass_key) override {
    displayMessage("Confirm: " + String(pass_key));
    return true;
  }

  bool onSecurityRequest() override {
    displayMessage("Security Req");
    return true;
  }

  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
    if (cmpl.success) {
      bondedDeviceAddress = btAddressToString(cmpl.bd_addr);
      displayMessage("Paired: " + bondedDeviceAddress);
    } else {
      displayMessage("Pair Failed!");
    }
  }
};

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    displayMessage("Connected");
    esp_bd_addr_t addr;
    memcpy(addr, pServer->getAddress().getNative(), sizeof(esp_bd_addr_t));
    bondedDeviceAddress = btAddressToString(addr);
  }

  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    displayMessage("Disconnected");
    // Start advertising again
    BLEDevice::startAdvertising();
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

  if (!SD.begin(SD_CS)) {
    displayMessage("SD init failed!");
    while (1);
  }
  displayMessage("SD init OK");

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

  // Initialize BLE
  BLEDevice::init("ESP32-GATT-Manager");
  BLEDevice::setSecurityCallbacks(new MySecurity());

  // Configure security
  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
  pSecurity->setCapability(ESP_IO_CAP_OUT);
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  pService = pServer->createService(BLEUUID(SERVICE_UUID), 30);

  // Create BLE Characteristics
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharacteristic->setCallbacks(new CommandCallback());

  pNotifyCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_RX_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  displayMessage("BLE started");
}

void loop() {
  // Disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // Give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // Restart advertising
    oldDeviceConnected = deviceConnected;
  }
  // Connecting
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
} 