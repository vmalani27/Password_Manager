#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

#define MAX_CREDENTIALS 50
#define CREDENTIALS_FILE "/credentials.json"

// Wi-Fi credentials - these should match your Flask app's network
const char* ssid = "ESP32-Network";
const char* password = "12345678";

// Create an AsyncWebServer object on port 80
AsyncWebServer server(80);

// Function to send JSON response
void sendJsonResponse(AsyncWebServerRequest *request, int statusCode, const char* message, bool success = true) {
  StaticJsonDocument<200> doc;
  doc["success"] = success;
  doc["message"] = message;
  
  String response;
  serializeJson(doc, response);
  request->send(statusCode, "application/json", response);
}

// Function to load credentials from SPIFFS
bool loadCredentials(JsonDocument& doc) {
  if (!SPIFFS.exists(CREDENTIALS_FILE)) {
    // Create empty credentials file if it doesn't exist
    File file = SPIFFS.open(CREDENTIALS_FILE, "w");
    if (!file) {
      Serial.println("Failed to create credentials file");
      return false;
    }
    file.print("{\"credentials\":[]}");
    file.close();
  }
  
  File file = SPIFFS.open(CREDENTIALS_FILE, "r");
  if (!file) {
    Serial.println("Failed to open credentials file");
    return false;
  }
  
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    Serial.println("Failed to parse credentials file");
    return false;
  }
  
  return true;
}

// Function to save credentials to SPIFFS
bool saveCredentials(JsonDocument& doc) {
  File file = SPIFFS.open(CREDENTIALS_FILE, "w");
  if (!file) {
    Serial.println("Failed to open credentials file for writing");
    return false;
  }
  
  if (serializeJson(doc, file) == 0) {
    Serial.println("Failed to write to credentials file");
    file.close();
    return false;
  }
  
  file.close();
  return true;
}

// Function to add or update a credential
bool addCredential(const char* site, const char* username, const char* password) {
  StaticJsonDocument<4096> doc;
  if (!loadCredentials(doc)) {
    return false;
  }
  
  JsonArray credentials = doc["credentials"];
  bool found = false;
  
  // Check if credential already exists
  for (JsonObject credential : credentials) {
    if (credential["site"] == site && credential["username"] == username) {
      credential["password"] = password;
      found = true;
      break;
    }
  }
  
  // Add new credential if not found
  if (!found) {
    JsonObject newCredential = credentials.createNestedObject();
    newCredential["site"] = site;
    newCredential["username"] = username;
    newCredential["password"] = password;
  }
  
  return saveCredentials(doc);
}

// Function to get a credential
bool getCredential(const char* site, const char* username, char* password, size_t maxLen) {
  StaticJsonDocument<4096> doc;
  if (!loadCredentials(doc)) {
    return false;
  }
  
  JsonArray credentials = doc["credentials"];
  
  for (JsonObject credential : credentials) {
    if (credential["site"] == site && credential["username"] == username) {
      strlcpy(password, credential["password"] | "", maxLen);
      return true;
    }
  }
  
  return false;
}

// Function to delete a credential
bool deleteCredential(const char* site, const char* username) {
  StaticJsonDocument<4096> doc;
  if (!loadCredentials(doc)) {
    return false;
  }
  
  JsonArray credentials = doc["credentials"];
  bool found = false;
  
  // Create a new array without the credential to delete
  JsonArray newCredentials = doc.to<JsonArray>();
  newCredentials.clear();
  
  for (JsonObject credential : credentials) {
    if (credential["site"] == site && credential["username"] == username) {
      found = true;
      continue;
    }
    
    JsonObject newCredential = newCredentials.createNestedObject();
    newCredential["site"] = credential["site"];
    newCredential["username"] = credential["username"];
    newCredential["password"] = credential["password"];
  }
  
  if (found) {
    doc["credentials"] = newCredentials;
    return saveCredentials(doc);
  }
  
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Give time for serial to initialize
  
  Serial.println("Starting ESP32...");
  
  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed");
    return;
  }
  Serial.println("SPIFFS initialized successfully");
  
  // Start Wi-Fi in Access Point mode
  WiFi.softAP(ssid, password);
  Serial.println("Wi-Fi Access Point started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Define a simple web server route
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "ESP32 is running!");
  });
  
  // Define API endpoints
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "ESP32 is online");
  });

  // Endpoint to receive password from Flask app
  server.on("/api/receive_password", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, (char*)data);
      
      if (error) {
        sendJsonResponse(request, 400, "Invalid JSON data", false);
        return;
      }

      const char* site = doc["site"];
      const char* username = doc["username"];
      const char* password = doc["password"];

      if (!site || !username || !password) {
        sendJsonResponse(request, 400, "Missing required fields", false);
        return;
      }

      if (addCredential(site, username, password)) {
        sendJsonResponse(request, 200, "Password received and stored successfully");
      } else {
        sendJsonResponse(request, 500, "Failed to store credential", false);
      }
  });

  // Endpoint to get stored credentials
  server.on("/api/credentials", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<4096> doc;
    if (!loadCredentials(doc)) {
      sendJsonResponse(request, 500, "Failed to load credentials", false);
      return;
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // Start the server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // Print memory info every 5 seconds
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 5000) {
    lastPrint = millis();
    Serial.printf("Free heap: %d, Largest block: %d\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  }
  
  delay(100);
}
