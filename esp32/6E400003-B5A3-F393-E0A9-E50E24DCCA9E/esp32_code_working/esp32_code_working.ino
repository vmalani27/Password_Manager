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

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NOTIFICATION_CHARACTERISTIC_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define SD_CS 13

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
sqlite3 *db;
sqlite3_stmt *res; // Declare globally
BLECharacteristic *pCharacteristic;
BLEServer *pServer; // Define the BLE server globally
char *zErrMsg = 0;
int rc;

// Function to update both the display and serial monitor
void updateOutput(const String &msg) {
  Serial.println(msg); // Print to serial monitor

  // Display logic
  display.clearDisplay(); // Clear the OLED display
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(msg); // Print to OLED display
  display.display();
}

void sendNotification(const String &data) {
  if (pCharacteristic == nullptr) {
    updateOutput("pCharacteristic is null.");
    return;
  }
  if (pServer == nullptr) {
    updateOutput("pServer is null.");
    return;
  }
  if (pServer->getConnectedCount() > 0) { // Check if a client is connected
    pCharacteristic->setValue(data.c_str());
    pCharacteristic->notify();
    delay(10);
  } else {
    updateOutput("No BLE client connected.");
  }
}

void executeSQL(const String &sql, const String &successMsg, const String &errorMsg) {
  rc = sqlite3_exec(db, sql.c_str(), 0, 0, &zErrMsg);
  String response = rc == SQLITE_OK ? successMsg : errorMsg + String(zErrMsg);
  updateOutput(response);
  sendNotification(response);
}

void handleCommand(String cmdLine) {
  updateOutput("Processing command: " + cmdLine);

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
    executeSQL("INSERT INTO credentials (site, username, password) VALUES ('" + tokens[1] + "', '" + tokens[2] + "', '" + tokens[3] + "');",
               "Added successfully", "Error: ");
  }

  else if (cmd == "get" && tokens.size() == 3) {
    String sql = "SELECT password FROM credentials WHERE site='" + tokens[1] + "' AND username='" + tokens[2] + "';";
    updateOutput("Before SQLite prepare...");
    rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &res, 0);
    updateOutput("After SQLite prepare...");

    if (rc != SQLITE_OK) {
      updateOutput("SQLite prepare failed: " + String(sqlite3_errmsg(db)));
      return;
    }

    updateOutput("Before SQLite step...");
    if (sqlite3_step(res) == SQLITE_ROW) {
      updateOutput("Row fetched successfully.");
      response = "Password: " + String((const char*)sqlite3_column_text(res, 0));
    } else {
      response = "Entry not found";
    }
    sqlite3_finalize(res);
    updateOutput("After SQLite finalize...");
    updateOutput(response);
    sendNotification(response);
  }

  else if (cmd == "update" && tokens.size() == 4) {
    executeSQL("UPDATE credentials SET password='" + tokens[3] + "' WHERE site='" + tokens[1] + "' AND username='" + tokens[2] + "';",
               "Updated successfully", "Error: ");
  }

  else if (cmd == "delete" && tokens.size() == 3) {
    executeSQL("DELETE FROM credentials WHERE site='" + tokens[1] + "' AND username='" + tokens[2] + "';",
               "Deleted successfully", "Error: ");
  }

  else if (cmd == "list") {
    String sql = "SELECT site, username FROM credentials;";
    updateOutput("Before SQLite prepare...");
    rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &res, 0);
    updateOutput("After SQLite prepare...");

    if (rc != SQLITE_OK) {
      updateOutput("SQLite prepare failed: " + String(sqlite3_errmsg(db)));
      return;
    }

    updateOutput("Before SQLite step...");
    response = "Listing credentials:\n";
    while (sqlite3_step(res) == SQLITE_ROW) {
      updateOutput("Row fetched successfully.");
      const char *site = (const char*)sqlite3_column_text(res, 0);
      const char *user = (const char*)sqlite3_column_text(res, 1);
      if (site != nullptr && user != nullptr) {
        response += "Site: " + String(site) + " | User: " + String(user) + "\n";
      } else {
        updateOutput("Null value encountered in SQLite result.");
      }
    }
    sqlite3_finalize(res);
    updateOutput("After SQLite finalize...");
    updateOutput(response);
    sendNotification(response);
  }

  else {
    response = "Invalid command or wrong argument count";
    updateOutput(response);
    sendNotification(response);
  }
}

class CommandCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    if (pChar) handleCommand(String(pChar->getValue().c_str()));
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    updateOutput("Client connected.");
  }
  void onDisconnect(BLEServer* pServer) {
    updateOutput("Client disconnected.");
    pServer->getAdvertising()->start();
  }
};

void setup() {
  Serial.begin(115200);

  // Initialize the display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Halt execution if display initialization fails
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Initializing..."));
  display.display();

  if (!SD.begin(SD_CS)) {
    updateOutput("SD card initialization failed.");
    while (true); // Halt execution if SD card fails
  }
  updateOutput("SD card initialized.");
  delay(500); // Add delay for stability

  sqlite3_initialize();
  rc = sqlite3_open("/sd/credentials.db", &db);
  if (rc != SQLITE_OK) {
    updateOutput("DB open failed.");
    return;
  }
  sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS credentials (id INTEGER PRIMARY KEY AUTOINCREMENT, site TEXT, username TEXT, password TEXT);", 0, 0, &zErrMsg);

  updateOutput("Initializing BLE...");
  BLEDevice::init("ESP32-GATT-Manager");
  delay(500); // Add delay for BLE initialization
  updateOutput("BLE initialized.");

  // Assign the BLE server to the global pServer
  pServer = BLEDevice::createServer();
  if (pServer == nullptr) {
    updateOutput("Failed to create BLE server.");
    while (true); // Halt execution if server creation fails
  }
  updateOutput("BLE server created.");
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *rxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
  rxCharacteristic->setCallbacks(new CommandCallback());

  pCharacteristic = pService->createCharacteristic(NOTIFICATION_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setValue("Ready");

  pService->start();
  BLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
  BLEDevice::getAdvertising()->start();
  BLEDevice::setMTU(256); // Use a smaller MTU size

  updateOutput("BLE GATT running...\nWaiting for client...");
}

void loop() {}