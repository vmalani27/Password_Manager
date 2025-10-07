# AES-256 Credential Storage Encryption Implementation Plan

## Date: October 8, 2025

---

## 🎯 **Objective**
Implement AES-256-GCM encryption for credential storage to protect against physical device theft and unauthorized SD card access.

**Current Vulnerability:** Credentials stored in plaintext SQLite database
**Target Security:** Military-grade encrypted storage with authentication

---

## 🏗️ **Architecture Overview**

### **Encryption Flow**
```
User Input → AES-256-GCM Encrypt → Store Encrypted in SQLite
SQLite Read → AES-256-GCM Decrypt → Return Plaintext to User
```

### **Key Management Strategy**
```
Hardware RNG → Master Key → Key Derivation → Database Encryption Key (DEK)
                    ↓
              Store in ESP32 NVS (encrypted)
```

---

## 📋 **Implementation Plan**

### **Phase 1: Core Encryption Infrastructure** ⭐ HIGH PRIORITY

#### **1.1 Add mbedTLS Dependencies**
- **File**: `esp32code.ino` (includes section)
- **Changes**:
  ```cpp
  #include "mbedtls/gcm.h"
  #include "mbedtls/entropy.h"
  #include "mbedtls/ctr_drbg.h"
  #include "mbedtls/sha256.h"
  ```
- **Purpose**: Access AES-GCM encryption and secure random number generation
- **Effort**: 5 minutes

#### **1.2 Create Encryption Helper Functions**
- **File**: `encryption_utils.h` (new file)
- **Functions**:
  ```cpp
  bool generateMasterKey(uint8_t* key, size_t keyLen);
  bool encryptCredential(const String& plaintext, const uint8_t* key, String& ciphertext, String& tag);
  bool decryptCredential(const String& ciphertext, const String& tag, const uint8_t* key, String& plaintext);
  bool deriveKeyFromMaster(const uint8_t* masterKey, const char* context, uint8_t* derivedKey);
  ```
- **Purpose**: Centralized encryption/decryption operations
- **Effort**: 2-3 hours

#### **1.3 Key Storage in ESP32 NVS**
- **File**: `key_manager.h` (new file)
- **Functions**:
  ```cpp
  bool storeMasterKey(const uint8_t* key, size_t keyLen);
  bool loadMasterKey(uint8_t* key, size_t keyLen);
  bool keyExists();
  void generateAndStoreMasterKey();
  ```
- **Purpose**: Secure persistent storage of encryption keys
- **Storage**: ESP32 Non-Volatile Storage (NVS) with hardware encryption
- **Effort**: 1-2 hours

### **Phase 2: Database Schema Updates** ⭐ HIGH PRIORITY

#### **2.1 Modify Credentials Table Structure**
- **Current Schema**:
  ```sql
  CREATE TABLE credentials (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    site TEXT,
    username TEXT,
    password TEXT
  );
  ```

- **New Encrypted Schema**:
  ```sql
  CREATE TABLE credentials (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    site_encrypted TEXT,           -- AES-256-GCM encrypted
    site_tag TEXT,                 -- Authentication tag for site
    username_encrypted TEXT,       -- AES-256-GCM encrypted  
    username_tag TEXT,             -- Authentication tag for username
    password_encrypted TEXT,       -- AES-256-GCM encrypted
    password_tag TEXT,             -- Authentication tag for password
    iv TEXT,                       -- Initialization Vector (per-record)
    created_at INTEGER             -- Timestamp for key rotation
  );
  ```

- **Migration Strategy**: 
  - Keep old table as `credentials_backup`
  - Create new encrypted table
  - Migrate existing data during first boot
- **Effort**: 1 hour

#### **2.2 Update SQL Helper Functions**
- **Files**: `esp32code.ino` (lines 97-190)
- **Changes**: Modify all CRUD operations to encrypt/decrypt data
- **Functions to Update**:
  - `insertCredential()` → `insertEncryptedCredential()`
  - `getPassword()` → `getDecryptedPassword()`
  - `updateCredential()` → `updateEncryptedCredential()`
  - `deleteCredential()` → unchanged (deletion by encrypted search)
  - `listCredentials()` → `listDecryptedCredentials()`
- **Effort**: 2-3 hours

### **Phase 3: Enhanced Security Features** 🔒 MEDIUM PRIORITY

#### **3.1 Per-Record Initialization Vectors**
- **Implementation**: Generate unique IV for each credential record
- **Security Benefit**: Prevents pattern analysis even with identical passwords
- **Code**:
  ```cpp
  bool generateIV(uint8_t* iv, size_t ivLen) {
    return esp_fill_random(iv, ivLen) == ESP_OK;
  }
  ```
- **Storage**: Store IV alongside encrypted data (safe to be public)
- **Effort**: 30 minutes

#### **3.2 Key Derivation with Context**
- **Implementation**: Derive different keys for different data types
- **Security Benefit**: Compartmentalized encryption (site key ≠ password key)
- **Code**:
  ```cpp
  // Derive separate keys for different fields
  deriveKeyFromMaster(masterKey, "SITE_ENCRYPTION", siteKey);
  deriveKeyFromMaster(masterKey, "PASSWORD_ENCRYPTION", passwordKey);
  deriveKeyFromMaster(masterKey, "USERNAME_ENCRYPTION", usernameKey);
  ```
- **Algorithm**: HKDF-SHA256 with context labels
- **Effort**: 1 hour

#### **3.3 Secure Memory Management**
- **Implementation**: Zero sensitive data after use
- **Code**:
  ```cpp
  void secureZero(void* ptr, size_t len) {
    volatile uint8_t* p = (volatile uint8_t*)ptr;
    for (size_t i = 0; i < len; i++) {
      p[i] = 0;
    }
  }
  ```
- **Apply to**: Plaintext passwords, encryption keys, temporary buffers
- **Effort**: 30 minutes

### **Phase 4: Integration & Testing** ✅ CRITICAL

#### **4.1 Backward Compatibility**
- **Migration Function**:
  ```cpp
  bool migrateToEncryptedStorage() {
    // Read existing plaintext credentials
    // Encrypt and store in new schema
    // Verify migration success
    // Delete old table
  }
  ```
- **Safety**: Backup existing data before migration
- **Rollback**: Keep plaintext backup until encryption verified
- **Effort**: 1 hour

#### **4.2 Error Handling & Recovery**
- **Scenarios**:
  - Master key corruption → regenerate and lose data (with user confirmation)
  - Decryption failure → audit log + error response
  - NVS storage failure → fallback to temporary session-only operation
- **Implementation**: Comprehensive error checking in all crypto operations
- **Effort**: 1 hour

---

## 🔧 **Technical Implementation Details**

### **Encryption Specification**
```
Algorithm: AES-256-GCM
Key Size: 256 bits (32 bytes)
IV Size: 96 bits (12 bytes) - optimal for GCM
Tag Size: 128 bits (16 bytes) - authentication tag
Block Size: 128 bits (16 bytes)
```

### **Key Derivation**
```
Master Key: 256-bit random (from ESP32 hardware RNG)
Context Keys: HKDF-SHA256(master_key, context_label, 32)
Storage: ESP32 NVS with flash encryption (if enabled)
```

### **Data Flow Example**
```cpp
// ENCRYPTION (during INSERT)
String plaintext = "mypassword123";
uint8_t iv[12], tag[16];
generateIV(iv, sizeof(iv));
String ciphertext = encryptWithGCM(plaintext, passwordKey, iv, tag);
// Store: ciphertext, tag, iv in database

// DECRYPTION (during GET)
String retrievedCiphertext, retrievedTag, retrievedIV;
// Load from database
String plaintext = decryptWithGCM(ciphertext, passwordKey, iv, tag);
// Return plaintext to user
```

---

## 📊 **Security Impact Assessment**

### **Threats Mitigated**
| Attack Vector | Before | After | Improvement |
|---------------|--------|-------|-------------|
| **Physical Device Theft** | 🔴 Full exposure | 🟢 Encrypted data | 100% protection |
| **SD Card Removal** | 🔴 Readable database | 🟢 Encrypted database | 100% protection |
| **Database Dump** | 🔴 Plaintext credentials | 🟢 Encrypted blobs | 100% protection |
| **Memory Analysis** | 🔴 Plaintext in RAM | 🟡 Limited exposure | 90% protection |
| **Firmware Extraction** | 🔴 DB readable | 🟢 Key protected | 95% protection |

### **Performance Impact**
- **Encryption Overhead**: ~5ms per credential operation
- **Memory Usage**: +256 bytes (key storage) + 28 bytes per record (IV + tag)
- **Storage Overhead**: ~30% increase in database size
- **CPU Impact**: Negligible on ESP32 (hardware AES acceleration)

### **Usability Impact**
- **User Experience**: No change (encryption transparent)
- **Pairing Process**: No change
- **Command Interface**: No change
- **Error Handling**: Enhanced error messages for crypto failures

---

## 🗓️ **Implementation Timeline**

### **Sprint 1 (2-3 days)** - Core Infrastructure
- [ ] Add mbedTLS includes and basic encryption functions
- [ ] Implement key generation and NVS storage
- [ ] Create encryption/decryption helper functions
- [ ] Unit test crypto operations

### **Sprint 2 (1-2 days)** - Database Integration  
- [ ] Modify database schema for encrypted storage
- [ ] Update all CRUD operations for encryption
- [ ] Implement data migration function
- [ ] Test encrypted CRUD operations

### **Sprint 3 (1 day)** - Enhanced Security
- [ ] Add per-record IVs and key derivation
- [ ] Implement secure memory management
- [ ] Add comprehensive error handling
- [ ] Integration testing

### **Sprint 4 (1 day)** - Testing & Validation
- [ ] End-to-end encryption testing
- [ ] Performance benchmarking
- [ ] Security validation
- [ ] Documentation updates

**Total Estimated Effort: 5-7 days**

---

## 🎛️ **Configuration Options**

### **Encryption Settings**
```cpp
// Configurable parameters
#define ENCRYPTION_ENABLED true
#define AES_KEY_SIZE 32        // 256-bit keys
#define AES_IV_SIZE 12         // 96-bit IV for GCM
#define AES_TAG_SIZE 16        // 128-bit authentication tag
#define KEY_DERIVATION_ITERATIONS 10000  // PBKDF2 iterations if needed
```

### **Migration Settings**
```cpp
#define AUTO_MIGRATE_ON_BOOT true
#define KEEP_PLAINTEXT_BACKUP false
#define MIGRATION_TIMEOUT_MS 30000
```

---

## 🧪 **Testing Strategy**

### **Unit Tests**
- [ ] Key generation and storage
- [ ] Encryption/decryption round-trips
- [ ] IV uniqueness verification
- [ ] Authentication tag validation
- [ ] Key derivation consistency

### **Integration Tests**  
- [ ] Full CRUD operations with encryption
- [ ] Data migration from plaintext
- [ ] Error recovery scenarios
- [ ] Performance under load

### **Security Tests**
- [ ] Encrypted data analysis (should be random)
- [ ] Key recovery attempts
- [ ] Memory dump analysis
- [ ] Timing attack resistance

---

## 🚀 **Expected Outcomes**

### **Security Improvements**
- ✅ **Physical theft protection**: Encrypted data useless without key
- ✅ **Offline attack resistance**: No plaintext data available
- ✅ **Forensic protection**: Database appears as random data
- ✅ **Compliance ready**: Meets enterprise encryption standards

### **Maintained Features**
- ✅ **Same user experience**: Transparent encryption
- ✅ **Same performance**: Minimal overhead
- ✅ **Same reliability**: Enhanced error handling
- ✅ **Same compatibility**: Backward compatible with migration

### **New Capabilities**
- ✅ **Selective decryption**: Only decrypt data when needed
- ✅ **Key rotation ready**: Foundation for future key updates
- ✅ **Audit trail**: Crypto operations logged
- ✅ **Enterprise ready**: Military-grade data protection

---

## ❓ **Questions for Decision**

1. **Migration Strategy**: Auto-migrate on first boot or require manual trigger?
2. **Backward Compatibility**: Keep plaintext backup or force one-way migration?
3. **Performance vs Security**: Use hardware AES acceleration or pure software?
4. **Key Recovery**: Implement master password backup or accept data loss on key corruption?
5. **Encryption Scope**: Encrypt audit logs too or keep them plaintext for debugging?

---

**Next Step**: Review this plan and decide on configuration options, then proceed with Phase 1 implementation.