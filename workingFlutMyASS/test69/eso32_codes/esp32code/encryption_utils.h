#ifndef ENCRYPTION_UTILS_H
#define ENCRYPTION_UTILS_H

#include <Arduino.h>
#include "mbedtls/gcm.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/sha256.h"
#include "esp_system.h"

// Encryption configuration constants
#define AES_KEY_SIZE 32        // 256-bit keys
#define AES_IV_SIZE 12         // 96-bit IV for GCM (optimal)
#define AES_TAG_SIZE 16        // 128-bit authentication tag
#define MASTER_KEY_SIZE 32     // 256-bit master key

// Key derivation contexts for different data types
#define CONTEXT_SITE "SITE_ENCRYPTION_V1"
#define CONTEXT_USERNAME "USERNAME_ENCRYPTION_V1"  
#define CONTEXT_PASSWORD "PASSWORD_ENCRYPTION_V1"
#define CONTEXT_AUDIT "AUDIT_ENCRYPTION_V1"

/**
 * Initialize the encryption system
 * Must be called once during setup before any encryption operations
 */
bool initEncryption();

/**
 * Generate a cryptographically secure master key using ESP32 hardware RNG
 * @param key Buffer to store the generated key (must be MASTER_KEY_SIZE bytes)
 * @return true if generation successful, false otherwise
 */
bool generateMasterKey(uint8_t* key);

/**
 * Generate a cryptographically secure initialization vector
 * @param iv Buffer to store the generated IV (must be AES_IV_SIZE bytes)
 * @return true if generation successful, false otherwise
 */
bool generateIV(uint8_t* iv);

/**
 * Derive a context-specific key from the master key using HKDF-SHA256
 * @param masterKey The master encryption key (MASTER_KEY_SIZE bytes)
 * @param context Context string to derive unique keys for different purposes
 * @param derivedKey Buffer to store derived key (must be AES_KEY_SIZE bytes)
 * @return true if derivation successful, false otherwise
 */
bool deriveKeyFromMaster(const uint8_t* masterKey, const char* context, uint8_t* derivedKey);

/**
 * Encrypt plaintext using AES-256-GCM
 * @param plaintext The data to encrypt
 * @param key Encryption key (AES_KEY_SIZE bytes)
 * @param iv Initialization vector (AES_IV_SIZE bytes)
 * @param ciphertext Output buffer for encrypted data
 * @param tag Output buffer for authentication tag (AES_TAG_SIZE bytes)
 * @return true if encryption successful, false otherwise
 */
bool encryptData(const String& plaintext, const uint8_t* key, const uint8_t* iv, 
                 String& ciphertext, uint8_t* tag);

/**
 * Decrypt ciphertext using AES-256-GCM with authentication
 * @param ciphertext The encrypted data to decrypt
 * @param key Decryption key (AES_KEY_SIZE bytes)
 * @param iv Initialization vector (AES_IV_SIZE bytes)
 * @param tag Authentication tag for verification (AES_TAG_SIZE bytes)
 * @param plaintext Output buffer for decrypted data
 * @return true if decryption and authentication successful, false otherwise
 */
bool decryptData(const String& ciphertext, const uint8_t* key, const uint8_t* iv,
                 const uint8_t* tag, String& plaintext);

/**
 * Convert binary data to hexadecimal string for storage
 * @param data Binary data buffer
 * @param len Length of binary data
 * @return Hexadecimal string representation
 */
String bytesToHex(const uint8_t* data, size_t len);

/**
 * Convert hexadecimal string back to binary data
 * @param hex Hexadecimal string
 * @param data Output buffer for binary data
 * @param maxLen Maximum length of output buffer
 * @return Number of bytes written, or 0 on error
 */
size_t hexToBytes(const String& hex, uint8_t* data, size_t maxLen);

/**
 * Securely zero memory to prevent sensitive data from remaining in RAM
 * @param ptr Pointer to memory to clear
 * @param len Number of bytes to clear
 */
void secureZero(void* ptr, size_t len);

/**
 * Encrypt a credential field with automatic IV generation
 * Convenience function that combines IV generation and encryption
 * @param plaintext The credential data to encrypt
 * @param masterKey The master encryption key
 * @param context Context string for key derivation
 * @param ciphertext Output encrypted data (hex encoded)
 * @param ivHex Output initialization vector (hex encoded)
 * @param tagHex Output authentication tag (hex encoded)
 * @return true if encryption successful, false otherwise
 */
bool encryptCredentialField(const String& plaintext, const uint8_t* masterKey,
                           const char* context, String& ciphertext, 
                           String& ivHex, String& tagHex);

/**
 * Decrypt a credential field with verification
 * Convenience function that combines decryption and authentication
 * @param ciphertext The encrypted data (hex encoded)
 * @param ivHex The initialization vector (hex encoded)
 * @param tagHex The authentication tag (hex encoded)
 * @param masterKey The master encryption key
 * @param context Context string for key derivation
 * @param plaintext Output decrypted data
 * @return true if decryption and authentication successful, false otherwise
 */
bool decryptCredentialField(const String& ciphertext, const String& ivHex,
                           const String& tagHex, const uint8_t* masterKey,
                           const char* context, String& plaintext);

#endif // ENCRYPTION_UTILS_H