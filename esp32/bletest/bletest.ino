#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-abcdef123456"

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  // Initialize BLE
  BLEDevice::init("ESP32_BLE");

  // Create BLE Server
  BLEServer *pServer = BLEDevice::createServer();

  // Create BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                        CHARACTERISTIC_UUID,
                                        BLECharacteristic::PROPERTY_READ |
                                        BLECharacteristic::PROPERTY_WRITE
                                      );

  // Set initial value for the characteristic
  pCharacteristic->setValue("Hello World!");
  
  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->start();
  Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void loop() {
  // Add any logic you need in the main loop
}
