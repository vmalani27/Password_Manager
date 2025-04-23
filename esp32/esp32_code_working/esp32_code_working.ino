#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1  // Reset pin not used
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

extern "C" {
  #include "sqlite3.h"
}

#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define SD_CS 5

sqlite3 *db;
sqlite3_stmt *res;
char *zErrMsg = 0;
int rc;

BLECharacteristic *pCharacteristic;

void handleCommand(String cmdLine) {
  Serial.println("Processing command: " + cmdLine);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Processing:");
  display.println(cmdLine);
  display.display();

  // Tokenize
  std::vector<String> tokens;
  char *token = strtok((char*)cmdLine.c_str(), " ");
  while (token != NULL) {
    tokens.push_back(String(token));
    token = strtok(NULL, " ");
  }

  if (tokens.size() == 0) return;

  String cmd = tokens[0];

  if (cmd == "add" && tokens.size() == 4) {
    String site = tokens[1], user = tokens[2], pass = tokens[3];
    String sql = "INSERT INTO credentials (site, username, password) VALUES ('" +
                  site + "', '" + user + "', '" + pass + "');";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
    String result = rc == SQLITE_OK ? "Added." : zErrMsg;
    Serial.println(result);
    display.clearDisplay();
    display.println(result);
    display.display();
  }

  else if (cmd == "get" && tokens.size() == 3) {
    String site = tokens[1], user = tokens[2];
    String sql = "SELECT password FROM credentials WHERE site='" + site +
                 "' AND username='" + user + "';";
    rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &res, 0);
    if (rc == SQLITE_OK && sqlite3_step(res) == SQLITE_ROW) {
      String password = "Password: " + String((const char*)sqlite3_column_text(res, 0));
      Serial.println(password);
      display.clearDisplay();
      display.println(password);
      display.display();
    } else {
      Serial.println("Not found.");
      display.clearDisplay();
      display.println("Not found.");
      display.display();
    }
    sqlite3_finalize(res);
  }

  else if (cmd == "update" && tokens.size() == 4) {
    String site = tokens[1], user = tokens[2], pass = tokens[3];
    String sql = "UPDATE credentials SET password='" + pass +
                 "' WHERE site='" + site + "' AND username='" + user + "';";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
    String result = rc == SQLITE_OK ? "Updated." : zErrMsg;
    Serial.println(result);
    display.clearDisplay();
    display.println(result);
    display.display();
  }

  else if (cmd == "delete" && tokens.size() == 3) {
    String site = tokens[1], user = tokens[2];
    String sql = "DELETE FROM credentials WHERE site='" + site +
                 "' AND username='" + user + "';";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
    String result = rc == SQLITE_OK ? "Deleted." : zErrMsg;
    Serial.println(result);
    display.clearDisplay();
    display.println(result);
    display.display();
  }

  else if (cmd == "list") {
    String sql = "SELECT site, username FROM credentials;";
    rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &res, 0);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Listing:");
    while (sqlite3_step(res) == SQLITE_ROW) {
      String site = String((const char*)sqlite3_column_text(res, 0));
      String user = String((const char*)sqlite3_column_text(res, 1));
      Serial.print("Site: ");
      Serial.print(site);
      Serial.print(" | User: ");
      Serial.println(user);
      display.println("Site: " + site);
      display.println("User: " + user);
    }
    display.display();
    sqlite3_finalize(res);
  }

  else {
    Serial.println("Invalid command or wrong argument count.");
    display.clearDisplay();
    display.println("Invalid command.");
    display.display();
  }
}

class CommandCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    String value = String(pChar->getValue().c_str());
    if (!value.isEmpty()) {
      handleCommand(String(value.c_str()));
    }
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    pServer->getAdvertising()->stop(); // Stop advertising when connected
    Serial.println("Client connected.");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("BLE Status: Connected");
    display.display();
  }

  void onDisconnect(BLEServer* pServer) {
    pServer->getAdvertising()->start(); // Restart advertising when disconnected
    Serial.println("Client disconnected.");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("BLE Status: Disconnected");
    display.display();
  }
};

void sendNotification(String data) {
  pCharacteristic->setValue(data.c_str()); // Set the full data as the characteristic value
  pCharacteristic->notify();              // Notify the client
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize the display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // 0x3C is the default I2C address
    Serial.println("SSD1306 allocation failed");
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Initializing...");
  display.display();

  if (!SD.begin(SD_CS)) {
    Serial.println("SD card mount failed.");
    display.clearDisplay();
    display.println("SD card mount failed.");
    display.display();
    return;
  }

  sqlite3_initialize();

  rc = sqlite3_open("/sd/credentials.db", &db);
  if (rc) {
    Serial.print("Can't open DB: ");
    Serial.println(sqlite3_errmsg(db));
    display.clearDisplay();
    display.println("DB open failed.");
    display.display();
    return;
  }

  const char *sql = "CREATE TABLE IF NOT EXISTS credentials ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "site TEXT, username TEXT, password TEXT);";
  sqlite3_exec(db, sql, 0, 0, &zErrMsg);

  BLEDevice::init("ESP32-GATT-Manager");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create rxCharacteristic
  BLECharacteristic *rxCharacteristic = pService->createCharacteristic(
    "6E400002-B5A3-F393-E0A9-E50E24DCCA9E",
    BLECharacteristic::PROPERTY_WRITE
  );
  rxCharacteristic->setCallbacks(new CommandCallback());

  // Create txCharacteristic
  BLECharacteristic *txCharacteristic = pService->createCharacteristic(
    "6E400003-B5A3-F393-E0A9-E50E24DCCA9E",
    BLECharacteristic::PROPERTY_NOTIFY
  );

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  BLEDevice::setMTU(512); // Maximum MTU size supported by BLE

  Serial.println("BLE GATT running. Waiting for command...");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("BLE GATT running...");
  display.println("Waiting for client...");
  display.display();
}

void loop() {
  // Main logic handled in callbacks
}
