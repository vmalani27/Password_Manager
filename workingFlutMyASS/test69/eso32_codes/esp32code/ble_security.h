#pragma once

// Order: Arduino core first, then NimBLE
#include <Arduino.h>
#include <NimBLEDevice.h>

// Forward-declare helper from your sketch (you already have this)
extern void updateOutput(const String &msg);

// Fallbacks for different core versions (some cores use different macro names)
#ifndef BLE_HS_IO_DISPLAY_ONLY
  #ifndef NIMBLE_IO_CAP_OUT
    // if neither is present, define something harmless (Display only behaviour)
    #define BLE_HS_IO_DISPLAY_ONLY 0
  #else
    #define BLE_HS_IO_DISPLAY_ONLY NIMBLE_IO_CAP_OUT
  #endif
#endif

#ifndef ESP_PWR_LVL_P7
  // If not defined in your core, use a default power level (0..7 typical)
  #define ESP_PWR_LVL_P7 7
#endif


// Configure NimBLE security. Call this once after NimBLEDevice::init(...)
inline void enableBleSecurity() {
  // Serial.println("[BLE] Enabling security: bonding, MITM, secure connections");
  // NimBLEDevice::setSecurityAuth(true, true, true); // bonding, MITM, secure
  // Serial.println("[BLE] Setting fixed passkey: 123456");
  // NimBLEDevice::setSecurityPasskey(123456);       // optional fixed passkey
  // Serial.println("[BLE] Setting IO capability: DISPLAY_ONLY");
  // NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY); 
  // Serial.println("[BLE] Setting TX power: P7 (max)");
  // NimBLEDevice::setPower(ESP_PWR_LVL_P7);         // strong signal for pairing
  // Serial.println("[BLE] Security setup complete");
}
