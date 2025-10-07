#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

const uint32_t DEV_PASSKEY = 123456;
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NOTIFY_CHAR_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;

// ---------------------- Helper ----------------------
void updateOutput(const String &msg) {
  Serial.println(msg);
}

// ------------------ Security Callbacks ------------------
class MySecurityCallbacks : public BLESecurityCallbacks {
public:
  uint32_t onPassKeyRequest() override {
    updateOutput("onPassKeyRequest called, returning static passkey");
    return DEV_PASSKEY;
  }

  void onPassKeyNotify(uint32_t pass_key) override {
    updateOutput("onPassKeyNotify: passkey=" + String(pass_key));
  }

  bool onConfirmPIN(uint32_t pass_key) override {
    updateOutput("onConfirmPIN called, auto-accept " + String(pass_key));
    return true;
  }

  bool onSecurityRequest() override {
    updateOutput("onSecurityRequest called -> allowing bonding");
    return true;
  }

  void onAuthenticationComplete(esp_ble_auth_cmpl_t param) override {
    if (param.success) {
      updateOutput("Authentication complete: SUCCESS, device bonded.");
    } else {
      updateOutput("Authentication complete: FAILED, reason=" + String(param.fail_reason));
    }
  }
};

// ------------------ Server Callbacks ------------------
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    updateOutput("Client connected");
  }
  void onDisconnect(BLEServer* server) override {
    updateOutput("Client disconnected, restarting advertising...");
    server->getAdvertising()->start();
  }
};

// ------------------ Characteristic Callbacks ------------------
class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) override {
    String value = String(pChar->getValue().c_str());
    updateOutput("Received write: " + value);
    pChar->setValue("ACK: " + value);
    pChar->notify();
  }
};

// ------------------ Setup ------------------
void setup() {
  Serial.begin(115200);
  delay(500);

  updateOutput("Initializing BLE...");
  BLEDevice::init("ESP32-GATT-Manager");

  // ---------------- Security ----------------
  // Set security callbacks first
  BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());
  
  // Configure security using BLEDevice methods (more compatible)
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
  
  // Set static passkey using GAP API
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, (void*)&DEV_PASSKEY, sizeof(uint32_t));
  
  // Configure security parameters
  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
  esp_ble_io_cap_t iocap = ESP_IO_CAP_OUT;
  uint8_t key_size = 16;
  uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  
  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

  // ---------------- BLE Server ----------------
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  BLECharacteristic* rxChar = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  rxChar->setCallbacks(new MyCharacteristicCallbacks());

  pCharacteristic = pService->createCharacteristic(
    NOTIFY_CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
  );
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setValue("Ready");

  pService->start();

  BLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
  BLEDevice::getAdvertising()->start();

  BLEDevice::setMTU(256); // Match MTU
  updateOutput("BLE GATT running, waiting for client...");
}

// ------------------ Loop ------------------
void loop() {
  delay(1000);
}
