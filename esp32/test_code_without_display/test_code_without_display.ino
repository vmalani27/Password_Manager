#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

// Display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1  // Reset pin not used
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

extern "C" {
  #define SQLITE_DEFAULT_MEMSTATUS 0
  #define SQLITE_ESP32_HEAP_SIZE 20480
  #include "sqlite3.h"
}

#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NOTIFICATION_CHARACTERISTIC_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define SD_CS 13

sqlite3 *db;
sqlite3_stmt *res;
char *zErrMsg = 0;
int rc;

BLECharacteristic *pCharacteristic;

void handleCommand(String cmdLine) {
  Serial.println("Processing command: " + cmdLine);

  // Tokenize
  std::vector<String> tokens;
  char *token = strtok((char*)cmdLine.c_str(), " ");
  while (token != NULL) {
    tokens.push_back(String(token));
    token = strtok(NULL, " ");
  }

  if (tokens.size() == 0) return;

  String cmd = tokens[0];
  String response;

  if (cmd == "add" && tokens.size() == 4) {
    String site = tokens[1], user = tokens[2], pass = tokens[3];
    String sql = "INSERT INTO credentials (site, username, password) VALUES ('" +
                  site + "', '" + user + "', '" + pass + "');";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
    response = rc == SQLITE_OK ? "Added successfully" : String("Error: ") + zErrMsg;
    Serial.println(response);
    sendNotification(response);
  }

  else if (cmd == "get" && tokens.size() == 3) {
    String site = tokens[1], user = tokens[2];
    String sql = "SELECT password FROM credentials WHERE site='" + site +
                 "' AND username='" + user + "';";
    rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &res, 0);
    if (rc == SQLITE_OK && sqlite3_step(res) == SQLITE_ROW) {
      response = "Password: " + String((const char*)sqlite3_column_text(res, 0));
    } else {
      response = "Entry not found";
    }
    Serial.println(response);
    sendNotification(response);
    sqlite3_finalize(res);
  }

  else if (cmd == "update" && tokens.size() == 4) {
    String site = tokens[1], user = tokens[2], pass = tokens[3];
    String sql = "UPDATE credentials SET password='" + pass +
                 "' WHERE site='" + site + "' AND username='" + user + "';";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
    response = rc == SQLITE_OK ? "Updated successfully" : String("Error: ") + zErrMsg;
    Serial.println(response);
    sendNotification(response);
  }

  else if (cmd == "delete" && tokens.size() == 3) {
    String site = tokens[1], user = tokens[2];
    String sql = "DELETE FROM credentials WHERE site='" + site +
                 "' AND username='" + user + "';";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
    response = rc == SQLITE_OK ? "Deleted successfully" : String("Error: ") + zErrMsg;
    Serial.println(response);
    sendNotification(response);
  }

  else if (cmd == "list") {
    String sql = "SELECT site, username FROM credentials;";
    rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &res, 0);
    response = "Listing credentials:\n";
    while (sqlite3_step(res) == SQLITE_ROW) {
      String site = String((const char*)sqlite3_column_text(res, 0));
      String user = String((const char*)sqlite3_column_text(res, 1));
      response += "Site: " + site + " | User: " + user + "\n";
    }
    Serial.println(response);
    sendNotification(response);
    sqlite3_finalize(res);
  }

  else {
    response = "Invalid command or wrong argument count";
    Serial.println(response);
    sendNotification(response);
  }
}

class CommandCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    if (pChar == nullptr) return;
    String value = String(pChar->getValue().c_str());
    if (!value.isEmpty()) handleCommand(value);
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    pServer->getAdvertising()->stop(); // Stop advertising when connected
    Serial.println("Client connected.");
  }

  void onDisconnect(BLEServer* pServer) {
    pServer->getAdvertising()->start(); // Restart advertising when disconnected
    Serial.println("Client disconnected.");
  }
};

class NotificationCallbacks : public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic *pChar) override {
    // The value is already set by sendNotification, no need to set it again
    pChar->notify();
  }
};

void sendNotification(String data) {
  if (pCharacteristic != nullptr) {
    // Update the characteristic value
    pCharacteristic->setValue((uint8_t*)data.c_str(), data.length());
    
    // Send notification
    pCharacteristic->notify();
    
    delay(10); // Give some time for the notification to be sent
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed.");
    return;
  }
  Serial.println("SD card initialized.");
  delay(500); // Add delay for stability

  BLEDevice::init("ESP32-GATT-Manager");
  delay(500); // Add delay for BLE initialization

  sqlite3_initialize();

  rc = sqlite3_open("/sd/credentials.db", &db);
  if (rc) {
    Serial.print("Can't open DB: ");
    Serial.println(sqlite3_errmsg(db));
    return;
  }

  if (rc != SQLITE_OK) {
    Serial.println("Failed to open database.");
    return;
  }

  const char *sql = "CREATE TABLE IF NOT EXISTS credentials ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "site TEXT, username TEXT, password TEXT);";
  sqlite3_exec(db, sql, 0, 0, &zErrMsg);

  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create rxCharacteristic
  BLECharacteristic *rxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  rxCharacteristic->setCallbacks(new CommandCallback());

  // Create txCharacteristic for notifications
  pCharacteristic = pService->createCharacteristic(
    NOTIFICATION_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
  );
  
  // Add callbacks for the notification characteristic
  pCharacteristic->setCallbacks(new NotificationCallbacks());
  
  // Add a descriptor for notifications
  pCharacteristic->addDescriptor(new BLE2902());
  
  // Set initial value
  pCharacteristic->setValue("Ready");
  pCharacteristic->notify();

  pCharacteristic->setValue("Ready");
  pCharacteristic->notify();

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  BLEDevice::setMTU(256); // Use a smaller MTU size

  Serial.println("BLE GATT running. Waiting for command...");
}

void loop() {
  // Main logic handled in callbacks
}
