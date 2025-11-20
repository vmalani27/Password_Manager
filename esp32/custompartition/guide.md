Understood — you want a clean, section-wise breakdown, in pseudo-code, showing:

✔ What changes you must make when moving your current .ino Arduino code
→ to a PlatformIO project
→ only the necessary changes (no rewriting architecture yet)
✔ While preparing the codebase to support HMAC-derived runtime key + FreeRTOS later
✔ But without fully rewriting anything yet

This will give you a map of:
what stays exactly the same,
what must change,
what must move,
what PlatformIO expects,
and how to structure it cleanly.

I’ll give you this in sections:

✅ SECTION 1 — Folder & File Structure (PlatformIO)

Your .ino file becomes a real C++ project:

project/
│ platformio.ini
│
└── src/
     └── main.cpp     // your previous INO logic goes here
└── include/
     └── secure_core.h  // optional: crypto prototypes
└── lib/
     └── secure_core/
           └── secure_core.cpp  // optional, for key manager later

✅ SECTION 2 — Arduino .ino → PlatformIO main.cpp MINIMAL CHANGES
In .ino files:

Arduino IDE auto-generates prototypes

Automatically includes Arduino.h

Global order doesn’t matter much

In PlatformIO:

You must explicitly manage:

✔ #include <Arduino.h>
✔ Function prototypes BEFORE use (or place functions before usage)
✔ Correct include paths for all headers
✔ C++ strict syntax (Arduino IDE allows some sloppiness)

🔧 MINIMAL REQUIRED FIXES

(Not rewrite — just necessary clean-up.)

#include <Arduino.h>              // must be first
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Adafruit_SSD1306.h>

extern "C" {                      // keep your SQLite
  #include "sqlite3.h"
}


No other changes in includes are needed now.

✅ SECTION 3 — Display SETUP (Pseudo-code)
✔ No change required

PlatformIO + Arduino handles Wire + Adafruit SSD1306 exactly like Arduino IDE.

Pseudo-code:
init OLED:
    display.begin()
    display.clear()
    display.setTextSize(1)
    display.setCursor(0,0)
    display.println("Initializing...")
    display.display()


Nothing else changes.

✅ SECTION 4 — SD CARD SETUP (Pseudo-code)
✔ Stays identical

PlatformIO handles the SD library the same way.

Pseudo-code:
init SD:
    if SD.begin(CS_pin) fails:
        show error on display
        halt or retry


Only change:
Move NO SD HALT into its own function later when adding FreeRTOS.

✅ SECTION 5 — SQLite SETUP (Pseudo-code)
✔ Almost identical

Just ensure database path is correct:

PlatformIO uses /sd/... path the same way.

Pseudo-code:
sqlite3_initialize()
sqlite3_open("/sd/credentials.db", &db)
sqlite3_exec(CREATE TABLE statements)


No change needed.

✅ SECTION 6 — BLE SETUP (Pseudo-code)
✔ Same BLEDevice, BLEServer, BLEService
✔ Same callbacks
✔ Same advertising and service UUIDs
Pseudo-code:
BLEDevice.init()
BLEDevice.setSecurityCallbacks()
(set BLE security parameters)
create server
set server callbacks
create service
create write characteristic
create notify characteristic
start service
start advertising


Nothing changes for PlatformIO.

❗ But one required structural change:

In PlatformIO:

BLE callbacks MUST NOT call long/blocking DB/SD operations.

So we isolate it later (not now).

But structurally:

onWrite():
    extract command
    enqueue command (later)
    return fast


We keep your current code but later we move DB operations into a FreeRTOS task.

✅ SECTION 7 — Session Token Flow (Pseudo-code)
✔ Stays exactly as is

PlatformIO does nothing different.

if NOT authenticated:
    if cmd == request_token:
         generate token
         notify token
    if cmd == auth:
         compare with stored token
         if OK → authorized
         if fail → count attempts

✅ SECTION 8 — Command Processing Flow (Pseudo-code)

Your big handleCommand() stays EXACTLY the same for now.

Later we will modify it for:

Encrypted DB operations

Moving logic to a queue & worker task

But in “necessary changes only” mode:

It stays unchanged.

✅ SECTION 9 — Main Loop (Pseudo-code)

Same logic:

every second:
    check BLE connection
    check inactivity timeout
    reset authorization on disconnect


PlatformIO loop works the same.

⚙️ SECTION 10 — Where HMAC Runtime Key System Fits In

Here is precisely where the new security code attaches:

⏺ Add at top (global area)

(Prototypes only for now)

void startKeyManagerTask();
bool encrypt_record(...);
bool decrypt_record(...);

⏺ In setup():

Before SD / SQLite / BLE:

startKeyManagerTask()
wait for runtime key ready


Later I’ll provide the real code.

⏺ In insertCredential():

(Not implementing yet — just location)

plaintext_pw → encrypt_record → store BLOB in SQLite

⏺ In getPassword():
read blob from SQLite → decrypt_record → return plaintext


No structural change — you keep your function calls as-is but replace inner logic.

📦 SECTION 11 — PlatformIO Compilation Differences
1. Replace .ino with main.cpp
2. Add these to platformio.ini:
framework = arduino
platform = espressif32
board = esp32dev
monitor_speed = 115200
build_flags =
   -DCORE_DEBUG_LEVEL=0
   -Os

3. Add library deps:
lib_deps =
   adafruit/Adafruit SSD1306
   bblanchon/ArduinoJson     ; if needed
   espressif/esp32           ; automatically included

4. Add SQLite library manually to lib/sqlite3/ (as you already have it).
🟩 SUMMARY (SHORT VERSION)

Here’s the exact minimal changes you must make:

✔ 1. Convert .ino → main.cpp

Add #include <Arduino.h> at top

Ensure all functions are declared before use

Move all code exactly as-is into main.cpp

✔ 2. Organize into sections (no logic change)

main.cpp should look like this (pseudo):

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Ble includes>
#include <SSD1306>
#include "sqlite3.h"

/// GLOBALS (display, BLE, SQLite...)


void setup() {
    initDisplay()
    initSD()
    initSQLite()
    initBLE()
    startAdvertising()
}


void loop() {
    monitorConnections()
}

✔ 3. Add ONLY these new integrations (NOT implementation yet)

In setup(), BEFORE SD/SQLite/BLE:

startKeyManagerTask()
waitForRuntimeKeyReady()


In database functions:

encrypt before insert
decrypt after select


We don’t implement encryption yet — only mark the places.

🔥 If this breakdown is good, next I will give you:
(1) EXACT code changes for each section — real code, not pseudo
(2) minimal changes to BLE callbacks to avoid blocking
(3) HMAC-runtime-key integration with smallest possible footprint

Say "Give me real code section-by-section".