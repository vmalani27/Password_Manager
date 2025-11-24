#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "esp_gap_ble_api.h"
#include "config.h"

// Forward declarations
class CryptoManager;
class UIManager;
class DBManager;

// ============================================================================
// BLE CALLBACK CLASSES
// ============================================================================

class MySecurityCallbacks : public BLESecurityCallbacks {
public:
    uint32_t onPassKeyRequest() override;
    void onPassKeyNotify(uint32_t pass_key) override;
    bool onConfirmPIN(uint32_t pass_key) override;
    bool onSecurityRequest() override;
    void onAuthenticationComplete(esp_ble_auth_cmpl_t param) override;
};

class CommandCallback : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic* pChar) override;
};

class EcdhCallback : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic* pCharacteristic) override;
};

class ServerCallbacks : public BLEServerCallbacks {
public:
    void onConnect(BLEServer* server) override;
    void onDisconnect(BLEServer* server) override;
};

// ============================================================================
// BLE MANAGER
// ============================================================================

class BLEManager {
public:
    // Singleton access
    static BLEManager& getInstance();
    
    // Initialization
    bool begin();
    
    // Notification
    void sendNotification(const String& data);
    
    // Session management
    String generateSessionToken();
    bool isSessionAuthorized() const { return sessionAuthorized; }
    void setSessionAuthorized(bool authorized) { sessionAuthorized = authorized; }
    String getSessionToken() const { return sessionToken; }
    void setSessionToken(const String& token) { sessionToken = token; }
    void clearSession();
    
    // Authentication tracking
    int getFailedAuthAttempts() const { return failedAuthAttempts; }
    void incrementFailedAuth();
    void resetFailedAuth() { failedAuthAttempts = 0; }
    unsigned long getLockoutUntil() const { return lockoutUntilMs; }
    void setLockout();
    bool isLockedOut() const { return millis() < lockoutUntilMs; }
    
    // Activity tracking
    void updateActivity() { lastActivityMs = millis(); }
    unsigned long getLastActivity() const { return lastActivityMs; }
    bool wasClientConnected() const { return wasConnected; }
    void setWasConnected(bool connected) { wasConnected = connected; }
    
    // BLE access
    BLEServer* getServer() { return pServer; }
    BLECharacteristic* getNotificationCharacteristic() { return pCharacteristic; }
    BLECharacteristic* getEcdhCharacteristic() { return pEcdhCharacteristic; }
    
    // Command handling
    void handleCommand(String cmdLine);
    
private:
    BLEManager();
    ~BLEManager();
    BLEManager(const BLEManager&) = delete;
    BLEManager& operator=(const BLEManager&) = delete;
    
    // BLE components
    BLEServer* pServer;
    BLECharacteristic* pCharacteristic;
    BLECharacteristic* pEcdhCharacteristic;
    
    // Session state
    String sessionToken;
    bool sessionAuthorized;
    int failedAuthAttempts;
    unsigned long lockoutUntilMs;
    unsigned long lastActivityMs;
    bool wasConnected;
};

#endif // BLE_MANAGER_H
