#include "esp_system.h"
#include "esp_log.h"
#include "mbedtls/aes.h"
#include "esp_aes.h"

void encrypt_using_block3_key(const uint8_t *plaintext, uint8_t *ciphertext, size_t len) {
    esp_aes_context ctx;
    esp_aes_init(&ctx);

    // Tell ESP32 AES engine to use Efuse KEY3
    esp_aes_setkey_efuse(&ctx, ESP_AES_ENCRYPT, ESP_EFUSE_KEY_PURPOSE_USER, EFUSE_BLK_KEY3);

    uint8_t iv[16] = {0};  // Use your own IV
    esp_aes_crypt_cbc(&ctx, ESP_AES_ENCRYPT, len, iv, plaintext, ciphertext);

    esp_aes_free(&ctx);
}

void decrypt_using_block3_key(const uint8_t *ciphertext, uint8_t *plaintext, size_t len) {
    esp_aes_context ctx;
    esp_aes_init(&ctx);

    // Same key slot, but decryption mode
    esp_aes_setkey_efuse(&ctx, ESP_AES_DECRYPT, ESP_EFUSE_KEY_PURPOSE_USER, EFUSE_BLK_KEY3);

    uint8_t iv[16] = {0};  
    esp_aes_crypt_cbc(&ctx, ESP_AES_DECRYPT, len, iv, ciphertext, plaintext);

    esp_aes_free(&ctx);
}

void setup() {
    Serial.begin(115200);

    const char *msg = "HelloWorld123456";  // 16 bytes
    uint8_t encrypted[16];
    uint8_t decrypted[16];

    encrypt_using_block3_key((uint8_t*)msg, encrypted, 16);

    Serial.println("Encrypted:");
    for (int i = 0; i < 16; i++) Serial.printf("%02X ", encrypted[i]);
    Serial.println();

    decrypt_using_block3_key(encrypted, decrypted, 16);

    Serial.print("Decrypted: ");
    for (int i = 0; i < 16; i++) Serial.print((char)decrypted[i]);
    Serial.println();
}

void loop() {}
