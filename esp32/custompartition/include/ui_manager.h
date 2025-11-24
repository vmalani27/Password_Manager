#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include "config.h"

// ============================================================================
// UI/DISPLAY MANAGEMENT
// ============================================================================

class UIManager {
public:
    // Singleton access
    static UIManager& getInstance();
    
    // Initialization
    bool begin();
    
    // Display operations
    void updateOutput(const String& msg);
    void clearDisplay();
    void showMessage(const String& msg);
    
    // Direct display access (if needed)
    Adafruit_SSD1306& getDisplay() { return display; }
    
private:
    UIManager();
    ~UIManager();
    UIManager(const UIManager&) = delete;
    UIManager& operator=(const UIManager&) = delete;
    
    Adafruit_SSD1306 display;
};

#endif // UI_MANAGER_H
