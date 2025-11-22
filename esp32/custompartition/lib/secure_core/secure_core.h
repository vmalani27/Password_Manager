#pragma once
#include <Arduino.h>

// Key management - synchronous initialization
bool initKeyManager();
bool deriveRuntimeKey();
bool isRuntimeKeyReady();

// Encryption/Decryption for password storage
bool encrypt_password(const String& plaintext, uint8_t* ciphertext, size_t* ciphertext_len, uint8_t* iv);
bool decrypt_password(const uint8_t* ciphertext, size_t ciphertext_len, const uint8_t* iv, String& plaintext);

// IV generation
void generate_iv(uint8_t* iv);

// Constants
#define IV_SIZE 16
#define MAX_PASSWORD_ENCRYPTED_SIZE 128