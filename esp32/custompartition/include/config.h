#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// HARDWARE CONFIGURATION
// ============================================================================

// OLED Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1

// SD Card
#define SD_CS 5

// Unpair Button (GPIO0 = BOOT button on most ESP32 dev boards)
#define UNPAIR_BUTTON_PIN 0
#define BUTTON_HOLD_TIME_MS 3000  // Hold for 3 seconds to unpair

// ============================================================================
// BLE CONFIGURATION
// ============================================================================

// BLE Service and Characteristic UUIDs
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NOTIFICATION_CHARACTERISTIC_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define ECDH_PUBLIC_KEY_CHARACTERISTIC_UUID "6E400005-B5A3-F393-E0A9-E50E24DCCA9E"

// ============================================================================
// SECURITY CONFIGURATION
// ============================================================================

// Session Management
#define CONNECTION_TIMEOUT_MS 300000      // 5 minutes - force disconnect
#define SESSION_TIMEOUT_MS 180000         // 3 minutes - clear session but stay connected
#define INACTIVITY_CHECK_INTERVAL 30000   // Check every 30 seconds
#define MAX_FAILED_ATTEMPTS 5
#define LOCKOUT_DURATION_MS (60UL * 1000UL)
#define SESSION_ID_LENGTH 16              // 128-bit session ID

// Cryptographic Key Sizes
#define ECDH_PRIVATE_KEY_SIZE 32
#define ECDH_PUBLIC_KEY_SIZE 64
#define SHARED_SECRET_SIZE 32
#define SESSION_KEY_SIZE 32
#define CHALLENGE_SIZE 16

// ============================================================================
// NVS CONFIGURATION
// ============================================================================

// NVS Namespace and Keys for Device Binding
#define NVS_NAMESPACE "pwmgr"
#define NVS_KEY_ESP_PRIVATE "esp_priv"
#define NVS_KEY_ESP_PUBLIC "esp_pub"
#define NVS_KEY_CLIENT_PUBLIC "client_pub"
#define NVS_KEY_PAIRED "paired"

// Pairing State
typedef enum {
    PAIRING_STATE_UNPAIRED = 0,
    PAIRING_STATE_PAIRED = 1
} pairing_state_t;

#endif // CONFIG_H
