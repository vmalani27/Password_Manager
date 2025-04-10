#include <SPI.h>
#include <SD.h>

#define SD_CS 27  // Chip Select pin

void setup() {
    Serial.begin(115200);
    
    if (!SD.begin(SD_CS)) {
        Serial.println("SD Card Initialization Failed!");
        return;
    }
    
    Serial.println("SD Card Initialized Successfully!");
}

void loop() {
}
