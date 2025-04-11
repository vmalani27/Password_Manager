// Libraries for HID and NimBLE
#include <BleKeyboard.h>
#include <NimBLEDevice.h>

// BLE HID Keyboard setup
BleKeyboard bleKeyboard("ESP32 Password", "ppm", 100);

// Custom UUIDs
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

// GATT Characteristic callback class
class CommandCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    String command = String(rxValue.c_str());

    Serial.print("Received GATT command: ");
    Serial.println(command);

    if (bleKeyboard.isConnected()) {
      if (command == "send") {
        Serial.println("Typing dummy credentials via HID...");
        bleKeyboard.print("username");
        bleKeyboard.write(KEY_TAB);
        delay(300);
        bleKeyboard.print("password123");
        bleKeyboard.write(KEY_RETURN);
      }
    } else {
      Serial.println("[ERROR] HID not connected");
    }
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Start BLE HID
  bleKeyboard.begin();
  Serial.println("BLE HID started");

  // Start NimBLE
  NimBLEDevice::init("ESP32 Dual BLE");
  NimBLEServer *pServer = NimBLEDevice::createServer();
  NimBLEService *pService = pServer->createService(SERVICE_UUID);

  NimBLECharacteristic *pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::WRITE
  );
  pCharacteristic->setCallbacks(new CommandCallback());

  pService->start();
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  Serial.println("GATT service started, waiting for trigger...");
}

void loop() {
  // Nothing in main loop
}
