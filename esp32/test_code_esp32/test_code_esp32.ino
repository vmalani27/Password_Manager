#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
extern "C" { #include "sqlite3.h" }

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NOTIFICATION_CHARACTERISTIC_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define SD_CS 5

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
sqlite3 *db;
BLECharacteristic *pCharacteristic;
char *zErrMsg = 0;
int rc;

void updateDisplay(const String &msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(msg);
  display.display();
}

void sendNotification(const String &data) {
  const int chunkSize = 20;
  for (size_t i = 0; i < data.length(); i += chunkSize) {
    pCharacteristic->setValue(data.substring(i, i + chunkSize).c_str());
    pCharacteristic->notify();
    delay(10);
  }
}

void handleCommand(String cmdLine) {
  std::vector<String> tokens;
  char *token = strtok((char*)cmdLine.c_str(), " ");
  while (token) tokens.push_back(String(token)), token = strtok(NULL, " ");
  if (tokens.empty()) return;

  String cmd = tokens[0], response;
  if (cmd == "add" && tokens.size() == 4) {
    String sql = "INSERT INTO credentials (site, username, password) VALUES ('" + tokens[1] + "', '" + tokens[2] + "', '" + tokens[3] + "');";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
    response = rc == SQLITE_OK ? "Added successfully" : String("Error: ") + zErrMsg;
  } else if (cmd == "get" && tokens.size() == 3) {
    String sql = "SELECT password FROM credentials WHERE site='" + tokens[1] + "' AND username='" + tokens[2] + "';";
    rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &res, 0);
    response = (rc == SQLITE_OK && sqlite3_step(res) == SQLITE_ROW) ? "Password: " + String((const char*)sqlite3_column_text(res, 0)) : "Entry not found";
    sqlite3_finalize(res);
  } else if (cmd == "update" && tokens.size() == 4) {
    String sql = "UPDATE credentials SET password='" + tokens[3] + "' WHERE site='" + tokens[1] + "' AND username='" + tokens[2] + "';";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
    response = rc == SQLITE_OK ? "Updated successfully" : String("Error: ") + zErrMsg;
  } else if (cmd == "delete" && tokens.size() == 3) {
    String sql = "DELETE FROM credentials WHERE site='" + tokens[1] + "' AND username='" + tokens[2] + "';";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
    response = rc == SQLITE_OK ? "Deleted successfully" : String("Error: ") + zErrMsg;
  } else if (cmd == "list") {
    String sql = "SELECT site, username FROM credentials;";
    rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &res, 0);
    response = "Listing credentials:\n";
    while (sqlite3_step(res) == SQLITE_ROW) {
      response += "Site: " + String((const char*)sqlite3_column_text(res, 0)) + " | User: " + String((const char*)sqlite3_column_text(res, 1)) + "\n";
    }
    sqlite3_finalize(res);
  } else {
    response = "Invalid command or wrong argument count";
  }
  Serial.println(response);
  updateDisplay(response);
  sendNotification(response);
}

class CommandCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    String value = String(pChar->getValue().c_str());
    if (!value.isEmpty()) handleCommand(value);
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    pServer->getAdvertising()->stop();
    updateDisplay("BLE Status: Connected");
  }
  void onDisconnect(BLEServer* pServer) {
    pServer->getAdvertising()->start();
    updateDisplay("BLE Status: Disconnected");
  }
};

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) for (;;);
  updateDisplay("Initializing...");
  if (!SD.begin(SD_CS)) { updateDisplay("SD card mount failed."); return; }

  sqlite3_initialize();
  rc = sqlite3_open("/sd/credentials.db", &db);
  if (rc) { updateDisplay("DB open failed."); return; }
  sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS credentials (id INTEGER PRIMARY KEY AUTOINCREMENT, site TEXT, username TEXT, password TEXT);", 0, 0, &zErrMsg);

  BLEDevice::init("ESP32-GATT-Manager");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *rxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
  rxCharacteristic->setCallbacks(new CommandCallback());
  pCharacteristic = pService->createCharacteristic(NOTIFICATION_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setValue("Ready");
  pService->start();

  BLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
  BLEDevice::getAdvertising()->start();
  BLEDevice::setMTU(512);

  updateDisplay("BLE GATT running...\nWaiting for client...");
}

void loop() {}