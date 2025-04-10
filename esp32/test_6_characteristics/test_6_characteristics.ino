#include <BleKeyboard.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <SPI.h>
#include <SD.h>

// BLE HID keyboard
BleKeyboard bleKeyboard("ESP32 Password", "ppm", 100);

// Custom UART-like BLE Service + Characteristic UUIDs
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

BLECharacteristic *pCharacteristic;

// BLE write handler
class CommandCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      String command = String(value.c_str());
      Serial.print("Received BLE command: ");
      Serial.println(command);

      if (command.startsWith("send ")) {
        String site = command.substring(5);
        File file = SD.open("/credentials.txt");
        if (file) {
          while (file.available()) {
            String line = file.readStringUntil('\n');
            if (line.startsWith(site + "=")) {
              int sep = line.indexOf(',');
              String user = line.substring(site.length() + 1, sep);
              String pass = line.substring(sep + 1);
              Serial.println("Typing credentials...");
              if (bleKeyboard.isConnected()) {
                bleKeyboard.print(user);
                bleKeyboard.write(KEY_TAB);
                delay(300);
                bleKeyboard.print(pass);
                bleKeyboard.write(KEY_RETURN);
              }
              break;
            }
          }
          file.close();
        } else {
          Serial.println("Failed to open credentials.txt");
        }
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  bleKeyboard.begin();

  // Init SD
  if (!SD.begin()) {
    Serial.println("SD Card initialization failed!");
  }

  // BLE Setup
  BLEDevice::init("ESP32 Password");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );

  pCharacteristic->setCallbacks(new CommandCallback());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  Serial.println("BLE ready, waiting for commands...");
}

void loop() {
  // Nothing needed here
}
