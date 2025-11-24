#include "ui_manager.h"

// Singleton instance
UIManager& UIManager::getInstance() {
    static UIManager instance;
    return instance;
}

// Constructor
UIManager::UIManager() 
    : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET)
{
}

// Destructor
UIManager::~UIManager() {
}

// Initialize display
bool UIManager::begin() {
    Serial.println("UIManager: Initializing OLED display...");
    Wire.begin();
    
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("UIManager: ERROR - SSD1306 allocation failed");
        return false;
    }
    
    Serial.println("UIManager: Display initialized successfully");
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Initializing..."));
    display.display();
    
    return true;
}

// Update display with message
void UIManager::updateOutput(const String& msg) {
    Serial.println(msg);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(msg);
    display.display();
}

// Clear display
void UIManager::clearDisplay() {
    display.clearDisplay();
    display.display();
}

// Show message
void UIManager::showMessage(const String& msg) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(msg);
    display.display();
}
