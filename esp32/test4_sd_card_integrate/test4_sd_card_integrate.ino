#include <SPI.h>
#include <SD.h>

#define SD_CS 13
File file;

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial
  delay(1000);

  Serial.println("Initializing SD card...");

  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed.");
    return;
  }
  Serial.println("SD card initialized.");
  printMenu();
}

void loop() {
  if (Serial.available()) {
    char option = Serial.read();

    switch (option) {
      case '1':
        writeCredentials();
        break;
      case '2':
        readCredentials();
        break;
      case '3':
        clearCredentials();
        break;
      case 'm':
      case 'M':
        printMenu();
        break;
      default:
        Serial.println("Invalid option. Press 'M' to show the menu again.");
    }
  }
}

void printMenu() {
  Serial.println("\n----- Menu -----");
  Serial.println("1. Write credentials to SD card");
  Serial.println("2. Read credentials from SD card");
  Serial.println("3. Clear credentials file");
  Serial.println("M. Show menu again");
  Serial.println("----------------\n");
  Serial.print("Enter your choice: ");
}

void writeCredentials() {
  Serial.println("\nEnter username:");
  while (!Serial.available());
  String username = Serial.readStringUntil('\n');
  username.trim();

  Serial.println("Enter password:");
  while (!Serial.available());
  String password = Serial.readStringUntil('\n');
  password.trim();

  file = SD.open("/credentials.txt", FILE_WRITE);
  if (file) {
    file.println("username:" + username);
    file.println("password:" + password);
    file.close();
    Serial.println("Credentials saved to SD card.");
  } else {
    Serial.println("Failed to open file for writing.");
  }
  Serial.print("Enter your choice: ");
}

void readCredentials() {
  file = SD.open("/credentials.txt");
  if (file) {
    Serial.println("\n--- Saved Credentials ---");
    while (file.available()) {
      String line = file.readStringUntil('\n');
      Serial.println(line);
    }
    file.close();
  } else {
    Serial.println("No credentials file found.");
  }
  Serial.print("Enter your choice: ");
}

void clearCredentials() {
  SD.remove("/credentials.txt");
  Serial.println("Credentials file deleted.");
  Serial.print("Enter your choice: ");
}
