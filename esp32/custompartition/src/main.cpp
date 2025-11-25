#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <secure_core.h>
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "config.h"
#include "crypto_manager.h"
#include "ble_manager.h"
#include "db_manager.h"
#include "ui_manager.h"

// ============================================================================
// PARTITION INFO
// ============================================================================

void printPartitionInfo() {
    Serial.println("\n=== Flash Partition Scheme ===");
    
    // Get running partition
    const esp_partition_t* running = esp_ota_get_running_partition();
    Serial.printf("Running from: %s (type=%d, subtype=%d, 0x%x, size=%d KB)\n",
                  running->label, running->type, running->subtype, 
                  running->address, running->size / 1024);
    
    // Iterate all partitions
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, 
                                                      ESP_PARTITION_SUBTYPE_ANY, NULL);
    Serial.println("\nAll Partitions:");
    Serial.println("Label         Type    SubType   Address    Size       Usage");
    Serial.println("-------------------------------------------------------------");
    
    while (it != NULL) {
        const esp_partition_t* part = esp_partition_get(it);
        
        // Calculate usage for data partitions
        String usage = "N/A";
        if (part->type == ESP_PARTITION_TYPE_DATA) {
            if (part->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS) {
                // NVS usage would require nvs_get_stats which is more complex
                usage = "NVS";
            } else if (part->subtype == ESP_PARTITION_SUBTYPE_DATA_FAT) {
                usage = "FAT";
            }
        }
        
        Serial.printf("%-12s  0x%02x    0x%02x      0x%06x  %7d KB  %s\n",
                      part->label,
                      part->type,
                      part->subtype,
                      part->address,
                      part->size / 1024,
                      usage.c_str());
        
        it = esp_partition_next(it);
    }
    esp_partition_iterator_release(it);
    
    // Show free heap
    Serial.println("\n=== Memory Info ===");
    Serial.printf("Free Heap: %d bytes (%.1f KB)\n", 
                  ESP.getFreeHeap(), ESP.getFreeHeap() / 1024.0);
    Serial.printf("Heap Size: %d bytes (%.1f KB)\n", 
                  ESP.getHeapSize(), ESP.getHeapSize() / 1024.0);
    Serial.printf("PSRAM: %s\n", psramFound() ? "Found" : "Not found");
    if (psramFound()) {
        Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
    }
    Serial.println("==============================\n");
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== ESP32 Password Manager Starting ===");
    
    // Print partition information
    printPartitionInfo();
    
    // Initialize security system FIRST (SYNCHRONOUS)
    Serial.println("Initializing key manager...");
    if (!initKeyManager()) {
        Serial.println("ERROR: Key manager init failed!");
        while(1) delay(1000);
    }
    
    Serial.println("Deriving runtime encryption key...");
    if (!deriveRuntimeKey()) {
        Serial.println("ERROR: Runtime key derivation failed!");
        while(1) delay(1000);
    }
    Serial.println("Runtime key ready");
    
    // Initialize UI Manager (OLED display)
    if (!UIManager::getInstance().begin()) {
        Serial.println("ERROR: UI Manager initialization failed!");
        while(1) delay(1000);
    }
    
    // Initialize SD card
    SPI.begin(18, 19, 23, 5);  // SCK, MISO, MOSI, CS
    Serial.println("Initializing SD card...");
    if (!SD.begin(SD_CS, SPI, 8000000)) {
        UIManager::getInstance().updateOutput("SD init failed.");
        Serial.println("ERROR: SD card initialization failed");
        while (true) delay(1000);
    }
    UIManager::getInstance().updateOutput("SD initialized.");
    Serial.println("SD card initialized successfully");
    delay(200);
    
    // Initialize Database Manager (SQLite)
    if (!DBManager::getInstance().begin()) {
        UIManager::getInstance().updateOutput("DB init failed.");
        Serial.println("ERROR: Database initialization failed");
        while(1) delay(1000);
    }
    
    // Initialize Crypto Manager (ECDH + NVS)
    if (!CryptoManager::getInstance().begin()) {
        UIManager::getInstance().updateOutput("Crypto init failed.");
        Serial.println("ERROR: Crypto Manager initialization failed");
        while(1) delay(1000);
    }
    
    // Initialize BLE Manager (BLE Server + Characteristics)
    if (!BLEManager::getInstance().begin()) {
        UIManager::getInstance().updateOutput("BLE init failed.");
        Serial.println("ERROR: BLE Manager initialization failed");
        while(1) delay(1000);
    }
    
    // Start database worker task
    if (!DBManager::getInstance().startWorkerTask()) {
        UIManager::getInstance().updateOutput("DB worker failed.");
        Serial.println("ERROR: Database worker task failed to start");
        while(1) delay(1000);
    }
    
    UIManager::getInstance().updateOutput("System ready with encryption!");
    Serial.println("\n=== System Ready ===");
    Serial.println("BLE Name: ESP32-PWD-Manager");
    Serial.println("PIN: 123456");
    Serial.println("Database worker: Running");
    Serial.println("Waiting for connections...\n");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
    BLEManager& ble = BLEManager::getInstance();
    CryptoManager& crypto = CryptoManager::getInstance();
    UIManager& ui = UIManager::getInstance();
    
    // Check for timeouts (session timeout, connection timeout)
    ble.loop();
    
    // Handle disconnect and session cleanup
    if (ble.wasClientConnected() && ble.getServer()) {
        int connectedCount = ble.getServer()->getConnectedCount();
        
        if (connectedCount == 0) {
            ui.updateOutput("Connection lost - resetting session");
            ble.clearSession();
            ble.setWasConnected(false);
        }
    }
    
    delay(100);  // Reduced from 1000ms for better responsiveness
}
