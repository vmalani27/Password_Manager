#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLECharacteristic.h>

// HID report descriptor using LED and 6 keys
static const uint8_t hidReportDescriptor[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xA1, 0x01,  // Collection (Application)
    0x85, 0x01,  // Report ID (1)
    0x05, 0x07,  // Usage Page (Key Codes)
    0x19, 0x00,  // Usage Minimum (0)
    0x29, 0xFF,  // Usage Maximum (255)
    0x15, 0x00,  // Logical Minimum (0)
    0x25, 0xFF,  // Logical Maximum (255)
    0x75, 0x08,  // Report Size (8)
    0x95, 0x06,  // Report Count (6)
    0x81, 0x00,  // Input (Data, Array)
    0xC0         // End Collection
};

BLEHIDDevice* hid;
BLECharacteristic* input;
bool connected = false;
const int LED_PIN = 2;

// ASCII to HID keycode mapping
const uint8_t asciiToHid[] = {
    0x00,  // NUL
    0x00,  // SOH
    0x00,  // STX
    0x00,  // ETX
    0x00,  // EOT
    0x00,  // ENQ
    0x00,  // ACK
    0x00,  // BEL
    0x2a,  // BS  Backspace
    0x2b,  // TAB Tab
    0x28,  // LF  Enter
    0x00,  // VT
    0x00,  // FF
    0x00,  // CR
    0x00,  // SO
    0x00,  // SI
    0x00,  // DEL
    0x00,  // DC1
    0x00,  // DC2
    0x00,  // DC3
    0x00,  // DC4
    0x00,  // NAK
    0x00,  // SYN
    0x00,  // ETB
    0x00,  // CAN
    0x00,  // EM
    0x00,  // SUB
    0x00,  // ESC
    0x00,  // FS
    0x00,  // GS
    0x00,  // RS
    0x00,  // US
    0x2c,  // ' '
    0x1e,  // !
    0x34,  // "
    0x20,  // #
    0x21,  // $
    0x22,  // %
    0x24,  // &
    0x34,  // '
    0x26,  // (
    0x27,  // )
    0x25,  // *
    0x2e,  // +
    0x36,  // ,
    0x2d,  // -
    0x37,  // .
    0x38,  // /
    0x27,  // 0
    0x1e,  // 1
    0x1f,  // 2
    0x20,  // 3
    0x21,  // 4
    0x22,  // 5
    0x23,  // 6
    0x24,  // 7
    0x25,  // 8
    0x26,  // 9
    0x33,  // :
    0x33,  // ;
    0x36,  // <
    0x2e,  // =
    0x37,  // >
    0x38,  // ?
    0x2f,  // @
    0x04,  // A
    0x05,  // B
    0x06,  // C
    0x07,  // D
    0x08,  // E
    0x09,  // F
    0x0a,  // G
    0x0b,  // H
    0x0c,  // I
    0x0d,  // J
    0x0e,  // K
    0x0f,  // L
    0x10,  // M
    0x11,  // N
    0x12,  // O
    0x13,  // P
    0x14,  // Q
    0x15,  // R
    0x16,  // S
    0x17,  // T
    0x18,  // U
    0x19,  // V
    0x1a,  // W
    0x1b,  // X
    0x1c,  // Y
    0x1d,  // Z
    0x2f,  // [
    0x31,  // bslash
    0x30,  // ]
    0x23,  // ^
    0x2d,  // _
    0x35,  // `
    0x04,  // a
    0x05,  // b
    0x06,  // c
    0x07,  // d
    0x08,  // e
    0x09,  // f
    0x0a,  // g
    0x0b,  // h
    0x0c,  // i
    0x0d,  // j
    0x0e,  // k
    0x0f,  // l
    0x10,  // m
    0x11,  // n
    0x12,  // o
    0x13,  // p
    0x14,  // q
    0x15,  // r
    0x16,  // s
    0x17,  // t
    0x18,  // u
    0x19,  // v
    0x1a,  // w
    0x1b,  // x
    0x1c,  // y
    0x1d,  // z
    0x2f,  // {
    0x31,  // |
    0x30,  // }
    0x35,  // ~
    0x4c   // DEL
};

// Shift key states for ASCII characters
const bool needsShift[] = {
    false, false, false, false, false, false, false, false,  // 0x00 - 0x07
    false, false, false, false, false, false, false, false,  // 0x08 - 0x0F
    false, false, false, false, false, false, false, false,  // 0x10 - 0x17
    false, false, false, false, false, false, false, false,  // 0x18 - 0x1F
    false, true,  true,  true,  true,  true,  true,  false, // 0x20 - 0x27  !"#$%&'
    true,  true,  true,  true,  false, false, false, false, // 0x28 - 0x2F  ()*+,-./
    false, false, false, false, false, false, false, false, // 0x30 - 0x37  01234567
    false, false, true,  false, true,  true,  true,  true,  // 0x38 - 0x3F  89:;<=>?
    true,  true,  true,  true,  true,  true,  true,  true,  // 0x40 - 0x47  @ABCDEFG
    true,  true,  true,  true,  true,  true,  true,  true,  // 0x48 - 0x4F  HIJKLMNO
    true,  true,  true,  true,  true,  true,  true,  true,  // 0x50 - 0x57  PQRSTUVW
    true,  true,  true,  false, false, false, true,  true,  // 0x58 - 0x5F  XYZ[\]^_
    false, false, false, false, false, false, false, false, // 0x60 - 0x67  `abcdefg
    false, false, false, false, false, false, false, false, // 0x68 - 0x6F  hijklmno
    false, false, false, false, false, false, false, false, // 0x70 - 0x77  pqrstuvw
    false, false, false, true,  true,  true,  true,  false  // 0x78 - 0x7F  xyz{|}~
};

class MyCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* server) {
        connected = true;
        digitalWrite(LED_PIN, HIGH);
        Serial.println("Connected");
    }

    void onDisconnect(BLEServer* server) {
        connected = false;
        digitalWrite(LED_PIN, LOW);
        Serial.println("Disconnected");
        server->getAdvertising()->start();
    }
};

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    Serial.println("Starting BLE Keyboard...");
    
    BLEDevice::init("Password Manager");
    BLEServer* server = BLEDevice::createServer();
    server->setCallbacks(new MyCallbacks());

    hid = new BLEHIDDevice(server);
    input = hid->inputReport(1);
    
    hid->manufacturer()->setValue("Vansh");
    hid->pnp(0x02, 0x05ac, 0x820a, 0x0001);
    hid->hidInfo(0x00, 0x01);
    hid->reportMap((uint8_t*)hidReportDescriptor, sizeof(hidReportDescriptor));
    hid->startServices();

    BLEAdvertising* advertising = server->getAdvertising();
    advertising->setAppearance(HID_KEYBOARD);
    advertising->addServiceUUID(hid->hidService()->getUUID());
    advertising->start();
    
    Serial.println("BLE Keyboard ready");
}

void loop() {
    if (connected && Serial.available()) {
        String command = Serial.readStringUntil('\n');
        if (command.startsWith("TYPE:")) {
            String text = command.substring(5);
            typeText(text);
        }
    }
    delay(50);
}

void typeKey(uint8_t key, bool shift) {
    if (!connected) return;
    
    uint8_t report[8] = {0};
    
    if (shift) {
        report[0] = 0x02;  // Left shift modifier
    }
    
    report[2] = key;
    input->setValue(report, sizeof(report));
    input->notify();
    delay(5);
    
    // Release all keys
    memset(report, 0, sizeof(report));
    input->setValue(report, sizeof(report));
    input->notify();
    delay(5);
}

void typeText(String text) {
    if (!connected) {
        Serial.println("Not connected");
        return;
    }

    for (int i = 0; i < text.length(); i++) {
        char c = text[i];
        if (c >= 0 && c <= 127) {  // Valid ASCII range
            uint8_t key = asciiToHid[c];
            bool shift = needsShift[c];
            if (key != 0) {
                typeKey(key, shift);
            }
        }
    }
    
    Serial.println("Text typed!");
} 