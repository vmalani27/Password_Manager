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
                // Different device trying to connect - check if BLE bond exists
                int dev_num = esp_ble_get_bond_device_num();
                bool bleUnpaired = (dev_num == 0);  // No BLE bonds = user unpaired from OS
                
                if (bleUnpaired) {
                    // User unpaired from OS settings - allow re-pairing with old device
                    Serial.println("ECDH: BLE bond removed - allowing re-pair with stored device");
                    Serial.println("ECDH: Clearing NVS pairing to allow fresh start");
                    crypto.unpairDevice();
                    
                    // Now accept new pairing
                    Serial.println("ECDH: New pairing initiated");
                    if (crypto.computeSharedSecret((const uint8_t*)rxValue.c_str())) {
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
                } else {
                    // Different device trying to connect while BLE still bonded
                    Serial.println("ECDH: Rejected - device already paired to another client");
                    ble.sendNotification("ECDH_ALREADY_PAIRED");
                }
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
    
    Serial.println("Client disconnected.");
    
    // CRITICAL: Always clear ALL session state on disconnect
    ble.clearSession();
    ble.setWasConnected(false);
    ble.resetFailedAuth();
    crypto.clearECDH();
    
    Serial.println("ECDH: Session state cleared");
    Serial.println("Session cleared on disconnect.");
    
    UIManager::getInstance().updateOutput("Client disconnected.");
    UIManager::getInstance().updateOutput("Session cleared.");
    
    // Ensure advertising restarts within 2 seconds
    delay(500);
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->start();
    Serial.println("Advertising restarted after disconnect");
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
    , lastInactivityCheck(0)
    , buttonPressStartTime(0)
    , buttonPressed(false)
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
    
    // Setup unpair button
    pinMode(UNPAIR_BUTTON_PIN, INPUT_PULLUP);
    Serial.println("BLEManager: Unpair button configured on GPIO" + String(UNPAIR_BUTTON_PIN));
    
    lastActivityMs = millis();
    Serial.println("BLEManager: Initialization complete");
    Serial.println("INFO: Hold BOOT button for 3 seconds to unpair device");
    
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
    failedAuthAttempts = 0;
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

// Check for inactivity timeout
void BLEManager::checkInactivityTimeout() {
    unsigned long now = millis();
    
    // Check periodically
    if (now - lastInactivityCheck < INACTIVITY_CHECK_INTERVAL) {
        return;
    }
    lastInactivityCheck = now;
    
    if (!pServer || pServer->getConnectedCount() == 0) return;
    
    unsigned long idleTime = now - lastActivityMs;
    
    // Force disconnect after CONNECTION_TIMEOUT
    if (idleTime > CONNECTION_TIMEOUT_MS) {
        Serial.println("TIMEOUT: Connection timeout - forcing disconnect");
        clearSession();
        CryptoManager::getInstance().clearECDH();
        
        if (pServer) {
            pServer->disconnect(0);  // Disconnect all clients
        }
        
        delay(100);
        
        // Restart advertising
        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->start();
        Serial.println("TIMEOUT: Advertising restarted");
        return;
    }
    
    // Clear session after SESSION_TIMEOUT (but stay connected)
    if (sessionAuthorized && idleTime > SESSION_TIMEOUT_MS) {
        Serial.println("TIMEOUT: Session timeout - clearing authorization");
        clearSession();
        sendNotification("SESSION_TIMEOUT");
    }
}

// Check unpair button (physical button on ESP32)
void BLEManager::checkUnpairButton() {
    bool buttonState = digitalRead(UNPAIR_BUTTON_PIN) == LOW;  // Active low (pulled up)
    
    if (buttonState && !buttonPressed) {
        // Button just pressed
        buttonPressed = true;
        buttonPressStartTime = millis();
        Serial.println("UNPAIR BUTTON: Pressed (hold for 3 seconds)");
    } else if (!buttonState && buttonPressed) {
        // Button released
        buttonPressed = false;
        Serial.println("UNPAIR BUTTON: Released");
    } else if (buttonPressed) {
        // Button still held - check if held long enough
        unsigned long holdTime = millis() - buttonPressStartTime;
        if (holdTime >= BUTTON_HOLD_TIME_MS) {
            // Trigger unpair
            Serial.println("UNPAIR BUTTON: 3 seconds elapsed - unpairing device");
            UIManager::getInstance().updateOutput("UNPAIR BUTTON");
            
            // Clear session
            clearSession();
            
            // Unpair from NVS
            CryptoManager& crypto = CryptoManager::getInstance();
            if (crypto.unpairDevice()) {
                Serial.println("UNPAIR BUTTON: Device unpaired successfully");
                UIManager::getInstance().updateOutput("Device UNPAIRED");
                
                // Disconnect any connected clients
                if (pServer && pServer->getConnectedCount() > 0) {
                    sendNotification("UNPAIRED");
                    delay(100);
                    pServer->disconnect(0);
                }
                
                // Clear all bonding info from BLE stack
                int dev_num = esp_ble_get_bond_device_num();
                if (dev_num > 0) {
                    esp_ble_bond_dev_t *bond_dev = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
                    if (bond_dev) {
                        esp_ble_get_bond_device_list(&dev_num, bond_dev);
                        for (int i = 0; i < dev_num; i++) {
                            esp_ble_remove_bond_device(bond_dev[i].bd_addr);
                            Serial.println("UNPAIR BUTTON: Removed BLE bond");
                        }
                        free(bond_dev);
                    }
                }
                
                // Restart advertising
                delay(500);
                BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
                pAdvertising->start();
                Serial.println("UNPAIR BUTTON: Ready for new pairing");
            } else {
                Serial.println("UNPAIR BUTTON: Failed to unpair");
                UIManager::getInstance().updateOutput("Unpair failed");
            }
            
            buttonPressed = false;  // Reset to prevent repeated triggers
        }
    }
}

// Main loop - call from loop()
void BLEManager::loop() {
    checkInactivityTimeout();
    checkUnpairButton();
}

// Handle command from client
void BLEManager::handleCommand(String cmdLine) {
    UIManager& ui = UIManager::getInstance();
    DBManager& db = DBManager::getInstance();
    CryptoManager& crypto = CryptoManager::getInstance();
    
    ui.updateOutput("Processing: " + cmdLine);
    
    // Update activity timestamp
    updateActivity();
    
    // Tokenize by spaces, supporting quoted strings
    std::vector<String> tokens;
    bool inQuotes = false;
    String currentToken = "";
    
    for (int i = 0; i < cmdLine.length(); i++) {
        char c = cmdLine.charAt(i);
        
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (c == ' ' && !inQuotes) {
            if (currentToken.length() > 0) {
                tokens.push_back(currentToken);
                currentToken = "";
            }
        } else if (c != '\n' && c != '\r') {
            currentToken += c;
        }
    }
    
    // Add last token
    if (currentToken.length() > 0) {
        tokens.push_back(currentToken);
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
    
    // Status command - returns connection and authorization state
    if (cmd.equalsIgnoreCase("status")) {
        String statusMsg = "STATUS ";
        if (pServer && pServer->getConnectedCount() > 0) {
            statusMsg += "CONNECTED ";
            if (sessionAuthorized) {
                statusMsg += "AUTHORIZED";
            } else {
                statusMsg += "UNAUTHORIZED";
            }
            // Add pairing status
            if (crypto.isDevicePaired()) {
                statusMsg += " PAIRED";
            } else {
                statusMsg += " UNPAIRED";
            }
        } else {
            statusMsg += "NOT_CONNECTED";
        }
        sendNotification(statusMsg);
        db.auditLog("STATUS_CHECK", statusMsg);
        return;
    }
    
    // Check pairing status - allows Flutter to skip ECDH if already paired
    if (cmd.equalsIgnoreCase("check_pairing")) {
        if (crypto.isDevicePaired()) {
            // Return stored client public key so Flutter can verify it matches
            String clientKeyHex = "";
            const uint8_t* storedKey = crypto.getStoredClientPublicKey();
            if (storedKey) {
                for (int i = 0; i < 64; i++) {
                    char buf[3];
                    sprintf(buf, "%02X", storedKey[i]);
                    clientKeyHex += buf;
                }
            }
            sendNotification("PAIRED:" + clientKeyHex);
            Serial.println("CHECK_PAIRING: Device is paired");
        } else {
            sendNotification("UNPAIRED");
            Serial.println("CHECK_PAIRING: Device is unpaired");
        }
        db.auditLog("CHECK_PAIRING", crypto.isDevicePaired() ? "paired" : "unpaired");
        return;
    }
    
    // Force disconnect command - allows client to reset connection
    if (cmd.equalsIgnoreCase("force_disconnect")) {
        Serial.println("FORCE_DISCONNECT: Client requested disconnect");
        sendNotification("DISCONNECTING");
        db.auditLog("FORCE_DISCONNECT", "Client initiated");
        
        clearSession();
        crypto.clearECDH();
        
        delay(100);  // Give notification time to send
        
        if (pServer) {
            pServer->disconnect(0);
        }
        
        // Restart advertising
        delay(500);
        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->start();
        Serial.println("FORCE_DISCONNECT: Advertising restarted");
        return;
    }
    
    // Unpair command - NO AUTH REQUIRED (allows recovery if session lost)
    if (cmd.equalsIgnoreCase("unpair")) {
        Serial.println("UNPAIR: Command received (no auth required for recovery)");
        
        if (crypto.unpairDevice()) {
            clearSession();
            ui.updateOutput("Device unpaired");
            sendNotification("UNPAIRED");
            db.auditLog("UNPAIR", "client");
            
            // Update ECDH characteristic with new key
            if (pEcdhCharacteristic) {
                pEcdhCharacteristic->setValue(const_cast<uint8_t*>(crypto.getEsp32PublicKey()), ECDH_PUBLIC_KEY_SIZE);
                Serial.println("UNPAIR: ECDH characteristic updated with new key");
            }
            
            // Clear all BLE bonds
            int dev_num = esp_ble_get_bond_device_num();
            if (dev_num > 0) {
                esp_ble_bond_dev_t *bond_dev = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
                if (bond_dev) {
                    esp_ble_get_bond_device_list(&dev_num, bond_dev);
                    for (int i = 0; i < dev_num; i++) {
                        esp_ble_remove_bond_device(bond_dev[i].bd_addr);
                        Serial.println("UNPAIR: Removed BLE bond");
                    }
                    free(bond_dev);
                }
            }
            
            ui.updateOutput("Device UNPAIRED");
            
            delay(100);  // Give notification time to send
            
            // Disconnect if connected
            if (pServer && pServer->getConnectedCount() > 0) {
                pServer->disconnect(pServer->getConnId());
                Serial.println("UNPAIR: Disconnected client");
            }
            
            delay(100);
            
            // Restart advertising
            BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
            pAdvertising->start();
            Serial.println("UNPAIR: Ready for new pairing");
        } else {
            sendNotification("UNPAIR_FAIL");
        }
        return;
    }
    
    // Database recovery command (no auth required for emergency recovery)
    if (cmd.equalsIgnoreCase("db_reset")) {
        Serial.println("DB_RESET: Emergency database reset requested");
        ui.updateOutput("Resetting database...");
        
        // Close database
        db.close();
        
        // Delete database file
        if (SD.exists("/passwords.db")) {
            SD.remove("/passwords.db");
            Serial.println("DB_RESET: Old database deleted");
        }
        if (SD.exists("/passwords.db-wal")) {
            SD.remove("/passwords.db-wal");
            Serial.println("DB_RESET: WAL file deleted");
        }
        if (SD.exists("/passwords.db-shm")) {
            SD.remove("/passwords.db-shm");
            Serial.println("DB_RESET: SHM file deleted");
        }
        
        // Reinitialize database
        if (db.begin("/passwords.db")) {
            ui.updateOutput("Database reset OK");
            sendNotification("DB_RESET_OK");
            db.auditLog("DB_RESET", "emergency");
            Serial.println("DB_RESET: Database recreated successfully");
        } else {
            ui.updateOutput("Database reset FAILED");
            sendNotification("DB_RESET_FAIL");
            Serial.println("DB_RESET: Failed to recreate database");
        }
        return;
    }
    
    if (cmd.equalsIgnoreCase("auth") && tokens.size() == 2) {
        if (tokens[1] == sessionToken && sessionToken.length() > 0) {
            sessionAuthorized = true;
            resetFailedAuth();
            
            ui.updateOutput("Authenticated");
            sendNotification("AUTH OK");
            db.auditLog("AUTH_SUCCESS", "authorized");
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
