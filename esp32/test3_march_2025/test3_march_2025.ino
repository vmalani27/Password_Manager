#include <BleKeyboard.h>

BleKeyboard bleKeyboard("ESP32 Password Manager", "MyCompany", 100);

bool wasConnected = false;

void setup() {
    Serial.begin(115200);
    bleKeyboard.begin();
    Serial.println("BLE Keyboard Started.");
}

void loop() {
    bool isConnected = bleKeyboard.isConnected();

    if (isConnected && !wasConnected) {
        Serial.println("✅ Device connected!");
        delay(2000); // Ensure stable connection before sending
        bleKeyboard.print("MySecurePassword123!");
        bleKeyboard.write(KEY_RETURN);
        Serial.println("Password sent!");
    } 
    else if (!isConnected && wasConnected) {
        Serial.println("❌ Device disconnected!");
    }

    wasConnected = isConnected;  // Update previous state
    delay(1000);  // Prevent rapid execution
}
