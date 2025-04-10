#include <BleKeyboard.h>

// Advertise as "Physical Password Manager"
BleKeyboard bleKeyboard("Physical Password Manager", "MyCompany", 100);

bool wasConnected = false; // Track previous connection state

void setup() {
    Serial.begin(115200);
    bleKeyboard.begin();
    Serial.println("BLE Keyboard Started.");
}

void loop() {
    if (bleKeyboard.isConnected()) {
        if (!wasConnected) {
            Serial.println("✅ Device connected.");
            wasConnected = true;
        }
    } else {
        if (wasConnected) {
            Serial.println("❌ Device disconnected.");
            wasConnected = false;
        }
        Serial.println("Waiting for connection...");
    }
    delay(1000);
}