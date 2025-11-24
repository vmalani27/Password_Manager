#include "ble_manager.h"
#include "crypto_manager.h"
#include "ui_manager.h"
#include "db_manager.h"
#include "esp_system.h"
#include <vector>

// ============================================================================
// BLE CALLBACK IMPLEMENTATIONS
// ============================================================================

uint32_t MySecurityCallbacks::onPassKeyRequest() {
    UIManager::getInstance().updateOutput("BLE: Passkey requested\\nPIN: 123456");
    return 123456;
}

void MySecurityCallbacks::onPassKeyNotify(uint32_t pass_key) {
    UIManager::getInstance().updateOutput("BLE: Passkey " + String(pass_key));
}

bool MySecurityCallbacks::onConfirmPIN(uint32_t pass_key) {
    UIManager::getInstance().updateOutput("BLE: Confirm PIN " + String(pass_key));
    return true;
}

bool MySecurityCallbacks::onSecurityRequest() {
    UIManager::getInstance().updateOutput("BLE: Security request");
    return true;
}

void MySecurityCallbacks::onAuthenticationComplete(esp_ble_auth_cmpl_t param) {
    if (param.success) {
        UIManager::getInstance().updateOutput("BLE: Bonded successfully!");
    } else {
        if (BLEManager::getInstance().isSessionAuthorized()) {
            UIManager::getInstance().updateOutput("BLE: Re-bonding failed (code " + String(param.fail_reason) + ") - continuing session");
        } else {
            UIManager::getInstance().updateOutput("BLE: Initial bonding failed (code " + String(param.fail_reason) + ")");
        }
    }
}

void CommandCallback::onWrite(BLECharacteristic* pChar) {
    if (pChar) {
        String val = pChar->getValue().c_str();
        if (val.length() == 0) return;
        BLEManager::getInstance().handleCommand(val);
    }
}

void EcdhCallback::onWrite(BLECharacteristic* pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    CryptoManager& crypto = CryptoManager::getInstance();
    BLEManager& ble = BLEManager::getInstance();
    
    // Clear any existing session before ECDH
    ble.clearSession();
    Serial.println("ECDH: Cleared previous session state");
    
    if (rxValue.length() == 64) {
        Serial.println("ECDH: Received client public key");
        
        // Check if device is already paired
        if (crypto.isDevicePaired()) {
            // Verify this is the paired device
            if (crypto.hasStoredClientKey() && memcmp(rxValue.c_str(), crypto.getStoredClientPublicKey(), 64) == 0) {
                Serial.println("ECDH: Recognized paired device");
                
                // Compute shared secret with stored keys
                if (crypto.computeSharedSecret((const uint8_t*)rxValue.c_str())) {
                    ble.sendNotification("ECDH_OK");
                    Serial.println("ECDH: Paired device authenticated");
                } else {
                    ble.sendNotification("ECDH_FAIL");
                    Serial.println("ECDH: Paired device auth failed");
                }
            } else {
                // Different device trying to connect
                Serial.println("ECDH: Rejected - device already paired to another client");
                ble.sendNotification("ECDH_ALREADY_PAIRED");
            }
        } else {
            // Device is unpaired - accept new pairing
            Serial.println("ECDH: New pairing initiated");
            
            // Compute shared secret
            if (crypto.computeSharedSecret((const uint8_t*)rxValue.c_str())) {
                // Save pairing to NVS
                if (crypto.savePairing((const uint8_t*)rxValue.c_str())) {
                    ble.sendNotification("ECDH_OK_PAIRED");
                    Serial.println("ECDH: New device paired successfully");
                } else {
                    ble.sendNotification("ECDH_OK");
                    Serial.println("ECDH: Handshake OK but pairing save failed");
                }
            } else {
                ble.sendNotification("ECDH_FAIL");
                Serial.println("ECDH: Handshake failed");
            }
        }
    } else {
        Serial.printf("ECDH: Invalid key length: %d\n", rxValue.length());
        ble.sendNotification("ECDH_INVALID");
    }
}

void ServerCallbacks::onConnect(BLEServer* server) {
    BLEManager& ble = BLEManager::getInstance();
    UIManager::getInstance().updateOutput("Client connected.");
    ble.updateActivity();
    ble.setWasConnected(true);
}

void ServerCallbacks::onDisconnect(BLEServer* server) {
    BLEManager& ble = BLEManager::getInstance();
    CryptoManager& crypto = CryptoManager::getInstance();
    
    UIManager::getInstance().updateOutput("Client disconnected.");
    server->getAdvertising()->start();
    
    ble.clearSession();
    ble.setWasConnected(false);
    crypto.clearECDH();
    
    UIManager::getInstance().updateOutput("Session cleared on disconnect.");
}

// ============================================================================
// BLE MANAGER IMPLEMENTATION
// ============================================================================

// Singleton instance
BLEManager& BLEManager::getInstance() {
    static BLEManager instance;
    return instance;
}

// Constructor
BLEManager::BLEManager()
    : pServer(nullptr)
    , pCharacteristic(nullptr)
    , pEcdhCharacteristic(nullptr)
    , sessionToken("")
    , sessionAuthorized(false)
    , failedAuthAttempts(0)
    , lockoutUntilMs(0)
    , lastActivityMs(0)
    , wasConnected(false)
{
}

// Destructor
BLEManager::~BLEManager() {
}

// Initialize BLE
bool BLEManager::begin() {
    Serial.println("BLEManager: Initializing BLE...");
    UIManager::getInstance().updateOutput("Initializing BLE...");
    
    BLEDevice::init("ESP32-PWD-Manager");
    delay(200);
    Serial.println("BLEManager: BLE device initialized");
    
    // Set security callbacks
    Serial.println("BLEManager: Setting up BLE security...");
    BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
    
    // Set static passkey
    const uint32_t DEV_PASSKEY = 123456;
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
    
    Serial.println("BLEManager: BLE security configured (PIN: 123456)");
    
    // Create BLE server
    Serial.println("BLEManager: Creating BLE server...");
    pServer = BLEDevice::createServer();
    if (!pServer) {
        Serial.println("BLEManager: ERROR - Failed to create BLE server");
        UIManager::getInstance().updateOutput("Failed create BLE server");
        return false;
    }
    pServer->setCallbacks(new ServerCallbacks());
    Serial.println("BLEManager: BLE server created");
    
    // Create BLE service
    Serial.println("BLEManager: Creating BLE service...");
    BLEService* pService = pServer->createService(SERVICE_UUID);
    
    // RX characteristic (write from client)
    Serial.println("BLEManager: Creating RX characteristic...");
    BLECharacteristic* rxChar = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    rxChar->setCallbacks(new CommandCallback());
    
    // TX characteristic (notifications to client)
    Serial.println("BLEManager: Creating TX characteristic...");
    pCharacteristic = pService->createCharacteristic(
        NOTIFICATION_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
    );
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic->setValue("Ready");
    
    // ECDH characteristic (read for ESP32 public key, write for client public key)
    Serial.println("BLEManager: Creating ECDH characteristic...");
    pEcdhCharacteristic = pService->createCharacteristic(
        ECDH_PUBLIC_KEY_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );
    pEcdhCharacteristic->setCallbacks(new EcdhCallback());
    
    // Set ESP32 public key in characteristic
    CryptoManager& crypto = CryptoManager::getInstance();
    pEcdhCharacteristic->setValue(const_cast<uint8_t*>(crypto.getEsp32PublicKey()), ECDH_PUBLIC_KEY_SIZE);
    
    if (crypto.isDevicePaired()) {
        Serial.println("BLEManager: Device is PAIRED - public key set");
        UIManager::getInstance().updateOutput("Device PAIRED");
    } else {
        Serial.println("BLEManager: Device is UNPAIRED - public key set");
        UIManager::getInstance().updateOutput("Device UNPAIRED");
    }
    
    Serial.println("BLEManager: Starting BLE service...");
    pService->start();
    
    Serial.println("BLEManager: Starting BLE advertising...");
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    BLEAdvertisementData advData;
    BLEAdvertisementData scanResp;
    
    // Primary advertisement packet
    advData.setName("ESP32-PWD-Manager");
    advData.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
    advData.setCompleteServices(BLEUUID(SERVICE_UUID));
    
    // Scan response
    scanResp.setName("ESP32-PWD-Manager");
    
    // Apply packets
    pAdvertising->setAdvertisementData(advData);
    pAdvertising->setScanResponseData(scanResp);
    
    // Connection params
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    
    // Start advertising
    BLEDevice::startAdvertising();
    Serial.println("BLEManager: BLE advertising started");
    
    Serial.println("BLEManager: Setting BLE MTU...");
    BLEDevice::setMTU(256);
    
    lastActivityMs = millis();
    Serial.println("BLEManager: Initialization complete");
    
    return true;
}

// Send notification to client
void BLEManager::sendNotification(const String& data) {
    Serial.println("DEBUG: sendNotification called with: " + data);
    
    if (!pCharacteristic) {
        UIManager::getInstance().updateOutput("pCharacteristic is null.");
        return;
    }
    if (!pServer) {
        UIManager::getInstance().updateOutput("pServer is null.");
        return;
    }
    
    int connectedCount = pServer->getConnectedCount();
    Serial.println("DEBUG: Connected clients: " + String(connectedCount));
    
    if (connectedCount > 0) {
        pCharacteristic->setValue(data.c_str());
        pCharacteristic->notify();
        Serial.println("DEBUG: Notification sent: " + data);
        delay(10);
    } else {
        UIManager::getInstance().updateOutput("No BLE client connected.");
    }
}

// Generate session token
String BLEManager::generateSessionToken() {
    uint32_t r = esp_random();
    char buf[16];
    snprintf(buf, sizeof(buf), "%08X", r);
    sessionToken = String(buf);
    return sessionToken;
}

// Clear session
void BLEManager::clearSession() {
    sessionAuthorized = false;
    sessionToken = "";
}

// Increment failed auth attempts
void BLEManager::incrementFailedAuth() {
    failedAuthAttempts++;
}

// Set lockout
void BLEManager::setLockout() {
    lockoutUntilMs = millis() + LOCKOUT_DURATION_MS;
    failedAuthAttempts = 0;
}

// Handle command from client
void BLEManager::handleCommand(String cmdLine) {
    UIManager& ui = UIManager::getInstance();
    DBManager& db = DBManager::getInstance();
    CryptoManager& crypto = CryptoManager::getInstance();
    
    ui.updateOutput("Processing: " + cmdLine);
    
    // Update activity timestamp
    updateActivity();
    
    // Tokenize by spaces
    std::vector<String> tokens;
    char* tok = strtok((char*)cmdLine.c_str(), " ");
    while (tok != NULL) {
        tokens.push_back(String(tok));
        tok = strtok(NULL, " ");
    }
    if (tokens.size() == 0) return;
    
    String cmd = tokens[0];
    
    // Check lockout
    if (isLockedOut()) {
        ui.updateOutput("Locked out until " + String(lockoutUntilMs));
        sendNotification("LOCKED");
        return;
    }
    
    // Public commands (no auth required)
    if (cmd.equalsIgnoreCase("request_token")) {
        generateSessionToken();
        ui.updateOutput("Token: " + sessionToken);
        sendNotification("TOKEN:" + sessionToken);
        db.auditLog("TOKEN_REQUEST", "unknown");
        return;
    }
    
    if (cmd.equalsIgnoreCase("auth") && tokens.size() == 2) {
        if (tokens[1] == sessionToken && sessionToken.length() > 0) {
            sessionAuthorized = true;
            resetFailedAuth();
            ui.updateOutput("Authenticated");
            sendNotification("AUTH OK");
            db.auditLog("AUTH_SUCCESS", tokens[1]);
        } else {
            incrementFailedAuth();
            ui.updateOutput("AUTH FAIL (" + String(failedAuthAttempts) + ")");
            sendNotification("AUTH FAIL");
            db.auditLog("AUTH_FAIL", "unknown");
            if (failedAuthAttempts >= MAX_FAILED_ATTEMPTS) {
                setLockout();
                ui.updateOutput("Too many failed auths. Locking for 1 min.");
                sendNotification("LOCKED");
            }
        }
        return;
    }
    
    // Authorized commands (session required)
    if (!sessionAuthorized) {
        ui.updateOutput("Not authorized. Send 'request_token' then 'auth <token>'.");
        sendNotification("NOT AUTHORIZED");
        return;
    }
    
    if (cmd.equalsIgnoreCase("add") && tokens.size() == 4) {
        bool ok = db.insertCredential(tokens[1], tokens[2], tokens[3]);
        ui.updateOutput(ok ? "Added successfully" : "Error: Insert failed");
        sendNotification(ok ? "Added" : "ADD FAIL");
        db.auditLog(ok ? "ADD" : "ADD_FAIL", tokens[2]);
        return;
    }
    
    if (cmd.equalsIgnoreCase("get") && tokens.size() == 3) {
        String pw = db.getPassword(tokens[1], tokens[2]);
        if (pw.length()) {
            String resp = "Password: " + pw;
            ui.updateOutput(resp);
            sendNotification(resp);
            db.auditLog("GET", tokens[2]);
        } else {
            ui.updateOutput("Entry not found");
            sendNotification("NOT FOUND");
            db.auditLog("GET_MISS", tokens[2]);
        }
        return;
    }
    
    if (cmd.equalsIgnoreCase("update") && tokens.size() == 4) {
        bool ok = db.updateCredential(tokens[1], tokens[2], tokens[3]);
        ui.updateOutput(ok ? "Updated successfully" : "Update failed");
        sendNotification(ok ? "Updated" : "UPDATE FAIL");
        db.auditLog(ok ? "UPDATE" : "UPDATE_FAIL", tokens[2]);
        return;
    }
    
    if (cmd.equalsIgnoreCase("delete") && tokens.size() == 3) {
        bool ok = db.deleteCredential(tokens[1], tokens[2]);
        ui.updateOutput(ok ? "Deleted successfully" : "Delete failed");
        sendNotification(ok ? "Deleted" : "DELETE FAIL");
        db.auditLog(ok ? "DELETE" : "DELETE_FAIL", tokens[2]);
        return;
    }
    
    if (cmd.equalsIgnoreCase("list")) {
        String out = db.listCredentials();
        if (out.length() == 0) out = "(none)";
        ui.updateOutput(out);
        sendNotification("LIST:\\n" + out);
        db.auditLog("LIST", "client");
        return;
    }
    
    if (cmd.equalsIgnoreCase("unpair")) {
        if (crypto.unpairDevice()) {
            clearSession();
            ui.updateOutput("Device unpaired");
            sendNotification("UNPAIRED");
            db.auditLog("UNPAIR", "client");
            // Update ECDH characteristic with new key
            pEcdhCharacteristic->setValue(const_cast<uint8_t*>(crypto.getEsp32PublicKey()), ECDH_PUBLIC_KEY_SIZE);
            // Restart advertising for new pairing
            if (pServer) {
                pServer->getAdvertising()->start();
            }
        } else {
            ui.updateOutput("Unpair failed");
            sendNotification("UNPAIR_FAIL");
        }
        return;
    }
    
    if (cmd.equalsIgnoreCase("logout")) {
        clearSession();
        ui.updateOutput("Logged out");
        sendNotification("LOGOUT");
        return;
    }
    
    // Unknown command
    ui.updateOutput("Invalid command or wrong args");
    sendNotification("INVALID");
}
