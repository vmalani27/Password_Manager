#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

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
    Serial.println(rc == SQLITE_OK ? "Added." : zErrMsg);
  }

  else if (cmd == "get" && tokens.size() == 3) {
    String site = tokens[1], user = tokens[2];
    String sql = "SELECT password FROM credentials WHERE site='" + site +
                 "' AND username='" + user + "';";
    rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &res, 0);
    if (rc == SQLITE_OK && sqlite3_step(res) == SQLITE_ROW) {
      Serial.println("Password: " + String((const char*)sqlite3_column_text(res, 0)));
    } else Serial.println("Not found.");
    sqlite3_finalize(res);
  }

  else if (cmd == "update" && tokens.size() == 4) {
    String site = tokens[1], user = tokens[2], pass = tokens[3];
    String sql = "UPDATE credentials SET password='" + pass +
                 "' WHERE site='" + site + "' AND username='" + user + "';";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
    Serial.println(rc == SQLITE_OK ? "Updated." : zErrMsg);
  }

  else if (cmd == "delete" && tokens.size() == 3) {
    String site = tokens[1], user = tokens[2];
    String sql = "DELETE FROM credentials WHERE site='" + site +
                 "' AND username='" + user + "';";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
    Serial.println(rc == SQLITE_OK ? "Deleted." : zErrMsg);
  }

  else if (cmd == "list") {
    String sql = "SELECT site, username FROM credentials;";
    rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &res, 0);
    while (sqlite3_step(res) == SQLITE_ROW) {
      Serial.print("Site: ");
      Serial.print((const char*)sqlite3_column_text(res, 0));
      Serial.print(" | User: ");
      Serial.println((const char*)sqlite3_column_text(res, 1));
    }
    sqlite3_finalize(res);
  }

  else {
    Serial.println("Invalid command or wrong argument count.");
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
    pServer->updateConnParams(pServer->getConnId(), 12, 12, 0, 100); // Optional: Adjust connection parameters
    pServer->getAdvertising()->stop(); // Stop advertising when connected
  }

  void onDisconnect(BLEServer* pServer) {
    pServer->getAdvertising()->start(); // Restart advertising when disconnected
  }
};

void sendNotification(String data) {
  const int chunkSize = 20; // Adjust based on MTU size
  for (size_t i = 0; i < data.length(); i += chunkSize) {
    String chunk = data.substring(i, i + chunkSize);
    pCharacteristic->setValue(chunk.c_str());
    pCharacteristic->notify();
    delay(10); // Small delay to ensure the client processes the notification
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD card mount failed.");
    return;
  }

  sqlite3_initialize();

  rc = sqlite3_open("/sd/credentials.db", &db);
  if (rc) {
    Serial.print("Can't open DB: ");
    Serial.println(sqlite3_errmsg(db));
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

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE
  );
  pCharacteristic->setCallbacks(new CommandCallback());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  // Request a larger MTU size
  BLEDevice::setMTU(512); // Maximum MTU size supported by BLE

  Serial.println("BLE GATT running. Waiting for command...");
}

void loop() {
  // Main logic handled in callbacks
}
