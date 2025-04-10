#include <BleKeyboard.h>
#include <SPI.h>
#include <SD.h>

extern "C" {
  #include "sqlite3.h"
}

#define SD_CS 5

sqlite3 *db;
sqlite3_stmt *res;
BleKeyboard bleKeyboard("Physical_Password_Manager", "ppm", 100);

bool wasConnected = false;
String storedUsername = "";
String storedPassword = "";

void setup() {
  Serial.begin(115200);
  delay(1000);

  bleKeyboard.begin();
  Serial.println("BLE Keyboard Started.");

  if (!SD.begin(SD_CS)) {
    Serial.println("SD card failed.");
    return;
  }

  sqlite3_initialize();

  if (sqlite3_open("/sd/credentials.db", &db)) {
    Serial.printf("Database open failed: %s\n", sqlite3_errmsg(db));
    return;
  }

  Serial.println("Database ready.");

  // Read the first credential once at startup
  const char *select_sql = "SELECT username, password FROM credentials LIMIT 1;";
  if (sqlite3_prepare_v2(db, select_sql, -1, &res, NULL) == SQLITE_OK) {
    if (sqlite3_step(res) == SQLITE_ROW) {
      storedUsername = (const char *)sqlite3_column_text(res, 0);
      storedPassword = (const char *)sqlite3_column_text(res, 1);

      Serial.printf("Loaded credentials: %s / %s\n", storedUsername.c_str(), storedPassword.c_str());
    } else {
      Serial.println("No data found.");
    }
    sqlite3_finalize(res);
  } else {
    Serial.println("Select query failed.");
  }
}

void loop() {
  if (bleKeyboard.isConnected()) {
    if (!wasConnected) {
      Serial.println("Device connected.");
      wasConnected = true;
    }

    // Listen for serial command while connected
    if (Serial.available()) {
      String input = Serial.readStringUntil('\n');
      input.trim();

      if (input.equalsIgnoreCase("SEND")) {
        bleKeyboard.print(storedUsername);
        bleKeyboard.write(KEY_TAB);
        delay(100);
        bleKeyboard.print(storedPassword);
        bleKeyboard.write(KEY_RETURN);

        Serial.println("Credentials sent.");
      }
    }

  } else {
    if (wasConnected) {
      Serial.println("Device disconnected.");
      wasConnected = false;
    }
  }

  delay(100);
}
