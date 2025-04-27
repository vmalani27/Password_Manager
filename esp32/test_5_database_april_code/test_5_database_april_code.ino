#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

extern "C" {
  #include "sqlite3.h"
}

#define SD_CS 13

sqlite3 *db;
char *zErrMsg = 0;
int rc;
sqlite3_stmt *res;

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!SD.begin(SD_CS))// High SPI speed for stability
 {
    Serial.println("SD card mount failed.");
    return;
  }
  Serial.println("SD card initialized.");

  // Mount SD card path for SQLite3ESP32
  sqlite3_initialize();

  // Open database (file path should begin with /sd/)
  rc = sqlite3_open("/sd/credentials.db", &db);
  if (rc) {
    Serial.print("Can't open database: ");
    Serial.println(sqlite3_errmsg(db));
    return;
  }
  Serial.println("Opened database successfully");

  // Create table
  const char *sql = "CREATE TABLE IF NOT EXISTS credentials ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "site TEXT, "
                    "username TEXT, "
                    "password TEXT);";

  rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);
  if (rc != SQLITE_OK) {
    Serial.print("SQL error: ");
    Serial.println(zErrMsg);
    sqlite3_free(zErrMsg);
  } else {
    Serial.println("Table created successfully");
  }

  // Insert sample data
  const char *insert_sql = "INSERT INTO credentials (site, username, password) "
                           "VALUES ('example.com', 'admin', '123456');";

  rc = sqlite3_exec(db, insert_sql, 0, 0, &zErrMsg);
  if (rc != SQLITE_OK) {
    Serial.print("Insert error: ");
    Serial.println(zErrMsg);
    sqlite3_free(zErrMsg);
  } else {
    Serial.println("Sample credentials inserted.");
  }

  // Select and print data
  const char *select_sql = "SELECT * FROM credentials;";
  rc = sqlite3_prepare_v2(db, select_sql, -1, &res, 0);
  if (rc == SQLITE_OK) {
    while (sqlite3_step(res) == SQLITE_ROW) {
      Serial.printf("ID: %d\n", sqlite3_column_int(res, 0));
      Serial.printf("Site: %s\n", sqlite3_column_text(res, 1));
      Serial.printf("Username: %s\n", sqlite3_column_text(res, 2));
      Serial.printf("Password: %s\n", sqlite3_column_text(res, 3));
      Serial.println("-----------------------");
    }
    sqlite3_finalize(res);
  } else {
    Serial.print("Failed to fetch data: ");
    Serial.println(sqlite3_errmsg(db));
  }

  sqlite3_close(db);
}

void loop() {
  // Nothing here
}
