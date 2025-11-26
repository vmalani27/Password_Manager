#pragma once
#include <Arduino.h>

// Key management - synchronous initialization
bool initKeyManager();
bool deriveRuntimeKey();
bool isRuntimeKeyReady();

// Encryption/Decryption for password storage (database - AES-256-CBC)
bool encrypt_password(const String& plaintext, uint8_t* ciphertext, size_t* ciphertext_len, uint8_t* iv);
bool decrypt_password(const uint8_t* ciphertext, size_t ciphertext_len, const uint8_t* iv, String& plaintext);

// Encryption/Decryption for BLE session traffic (AES-256-CTR)
bool encrypt_session(const uint8_t* session_key, const uint8_t* plaintext, size_t plaintext_len, 
                     uint8_t* ciphertext, size_t* ciphertext_len, uint8_t* nonce);
bool decrypt_session(const uint8_t* session_key, const uint8_t* ciphertext, size_t ciphertext_len, 
                     const uint8_t* nonce, uint8_t* plaintext, size_t* plaintext_len);

// IV/Nonce generation
void generate_iv(uint8_t* iv);
void generate_nonce(uint8_t* nonce);

// Constants
#define IV_SIZE 16
#define NONCE_SIZE 16
#define MAX_PASSWORD_ENCRYPTED_SIZE 128
#define MAX_SESSION_ENCRYPTED_SIZE 512  // Max BLE packet size