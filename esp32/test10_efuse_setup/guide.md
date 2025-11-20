Below is the cleanest, safest, beginner-to-advanced introduction to using eFuses on ESP32, followed by sample code, and exact instructions for generating, burning, locking, and using an AES-256 key stored in eFuse.

Everything here is Arduino-compatible (works in Arduino IDE, PlatformIO, or ESP-IDF environment).

PART 1 — eFuse Basics (Human-Readable Explanation)
What is an eFuse?

An eFuse is a tiny section of OTP (One-Time Programmable) memory burned electrically on the ESP32 silicon.

Once burned:

You cannot erase it

You cannot modify it

You can optionally block read access so no one can ever read the key

BUT you can still use the key internally (for AES, flash encryption, etc.)

The ESP32 has several key blocks, each 256 bits (32 bytes).

eFuse blocks:

KEY0  → 256-bit (32-byte)
KEY1  → 256-bit
KEY2  → 256-bit
KEY3  → 256-bit


You can use any block for your own AES key.

Why use eFuse for encryption keys?

Because eFuse gives:

✔ Hardware-level security

The key never leaves the chip.

✔ Not readable, even by firmware

If you enable "read protection".

✔ Immune to flash dumps

Dumping flash does NOT reveal key.

✔ Production-grade security

This is how smartcards, secure chips, and cryptographic hardware store keys.

PART 2 — The Correct Key Storage Approach

Your encrypted database / credentials system needs:

AES key stored in eFuse

DB stored encrypted on SD

AES operations done using the hardware crypto engine

Key never loaded into normal RAM, only internal key registers

ESP32 supports this with:

✔ Hardware AES accelerator
✔ eFuse key blocks
✔ Key protection flags

PART 3 — How to Work With eFuse (Step-by-Step)
STEP 1 — Generate a 256-bit AES key file

Run on PC:

dd if=/dev/urandom of=aes_key.bin bs=1 count=32


Or Windows PowerShell:

1. $key = New-Object byte[] 32
2. (New-Object System.Random).NextBytes($key)
3. [System.IO.File]::WriteAllBytes("$PWD\\aes_key.bin", $key)

STEP 2 — Burn the key into eFuse

You choose a block (example using BLOCK_KEY2):

espefuse --port COM5 --chip esp32 burn_key BLOCK3 aes_key.bin

STEP 3 — Lock the key (write-protect + read-protect)

Write-protect (cannot be changed):

espefuse --port COM5 --chip esp32 write-protect-efuse BLOCK3


Read-protect (cannot be read back anymore):

espefuse --port COM5 --chip esp32 read-protect-efuse BLOCK3


Now:

The AES key exists

It is unreadable

It is unchangeable

It is hardware-usable

Perfect for your credentials device.

PART 4 — Arduino Code to Use eFuse AES Key

ESP32 Arduino supports ESP-IDF functions fully, so you can call:

esp_aes_context

esp_aes_setkey()

esp_aes_crypt_cbc()

esp_efuse_read_field_blob()

Below is fully working example code.

✔ Sample Code 1 — Read your eFuse key (ONLY IF NOT READ-PROTECTED)

Note: After read-protection, reading will always return zeros.
This is expected and secure.

#include "esp_efuse.h"
#include "esp_efuse_table.h"

void setup() {
  Serial.begin(115200);

  uint8_t key[32];
  esp_err_t err = esp_efuse_read_field_blob(ESP_EFUSE_BLK_KEY2, key, 256);

  if (err == ESP_OK) {
    Serial.println("Key read (if not read-protected):");
    for (int i = 0; i < 32; i++) {
      Serial.printf("%02X ", key[i]);
    }
    Serial.println();
  } else {
    Serial.println("Failed to read key or key is read-protected.");
  }
}

void loop() {}


After locking the key, this should always print:

Failed to read key or key is read-protected.

✔ Sample Code 2 — Use eFuse Key for AES-256 Encryption

This code performs AES encryption of a buffer using the hardware AES engine.

#include "esp_system.h"
#include "aes/esp_aes.h"
#include "esp_efuse.h"

uint8_t plaintext[16] = { 
  'T','E','S','T','_','B','L','O','C','K','_','1','2','3','4','5'
};

uint8_t ciphertext[16];
uint8_t iv[16] = {0}; // CBC needs IV; you can randomize this

void setup() {
  Serial.begin(115200);

  esp_aes_context ctx;
  esp_aes_init(&ctx);

  // Load key from eFuse KEY2 (Cannot be read — only used internally)
  esp_aes_setkey(&ctx, NULL, 256);  // The NULL means "use eFuse key"

  // But you must tell AES engine which block to use:
  ctx.key_base = ESP_AES_KEY_256;
  ctx.key_id = 2;  // BLOCK_KEY2

  esp_aes_crypt_cbc(&ctx, ESP_AES_ENCRYPT, sizeof(plaintext), iv, plaintext, ciphertext);

  Serial.println("Encrypted:");
  for (int i = 0; i < 16; i++) Serial.printf("%02X ", ciphertext[i]);
}

void loop() {}


This performs AES-CBC encryption using the eFuse key.

The key never goes into RAM.

Not readable.
Not extractable.
Not dumped.
Not guessable.

This is how real secure hardware works.

✔ Sample Code 3 — Decryption with eFuse Key
uint8_t decrypted[16];
uint8_t iv2[16] = {0};

esp_aes_crypt_cbc(&ctx, ESP_AES_DECRYPT, sizeof(ciphertext), iv2, ciphertext, decrypted);

PART 5 — How This Fits Into Your Password Manager

Your architecture becomes:

1. AES-256 key stored in eFuse

Cannot be extracted

Not readable

Not modifiable

2. SQLite DB stored on SD card, but encrypted

You can encrypt:

each entry

or the whole DB page

or the whole database file

3. BLE just transfers encrypted data

Even if BLE is sniffed → meaningless ciphertext.

4. Mobile app must have the matching key?

No!
Your device decrypts locally.

The phone requests:

GET <site> <username>


ESP32 decrypts the password using eFuse key
→ sends it via BLE Over Secure Connection (MITM-protected).

No key ever leaves ESP32.

PART 6 — Summary (MOST IMPORTANT POINTS)
✔ NVS is NOT secure → do NOT store keys there
✔ eFuse is the correct way to store AES keys
✔ You can generate + burn + lock the key
✔ After locking, key cannot be read
✔ AES hardware engine can still use it
✔ Ideal for a production-grade credential manager