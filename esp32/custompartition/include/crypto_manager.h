#ifndef CRYPTO_MANAGER_H
#define CRYPTO_MANAGER_H

#include <Arduino.h>
#include "config.h"

// ============================================================================
// ECDH KEY MANAGEMENT
// ============================================================================

class CryptoManager {
public:
    // Singleton access
    static CryptoManager& getInstance();
    
    // Initialization
    bool begin();
    
    // ECDH Key Generation
    bool initECDH();
    bool computeSharedSecret(const uint8_t* client_pubkey);
    void clearECDH();
    
    // Device Binding (NVS)
    bool loadOrGenerateKeys();
    bool savePairing(const uint8_t* client_pubkey_to_save);
    bool unpairDevice();
    bool isDevicePaired();
    
    // Key Access (read-only)
    const uint8_t* getEsp32PrivateKey() const { return esp32_private_key; }
    const uint8_t* getEsp32PublicKey() const { return esp32_public_key; }
    const uint8_t* getClientPublicKey() const { return client_public_key; }
    const uint8_t* getSharedSecret() const { return shared_secret; }
    const uint8_t* getSessionKey() const { return session_aes_key; }
    
    // State
    bool isEcdhReady() const { return ecdh_ready; }
    void setEcdhReady(bool ready) { ecdh_ready = ready; }
    
    // Challenge-Response
    const uint8_t* getPendingChallenge() const { return pending_challenge; }
    uint8_t* getPendingChallenge() { return pending_challenge; }
    bool isChallengePending() const { return challenge_pending; }
    void setChallengePending(bool pending) { challenge_pending = pending; }
    
    // Pairing State
    pairing_state_t getPairingState() const { return pairing_state; }
    bool hasStoredClientKey() const { return has_stored_client_key; }
    const uint8_t* getStoredClientPublicKey() const { return stored_client_public_key; }
    
private:
    CryptoManager();
    ~CryptoManager();
    CryptoManager(const CryptoManager&) = delete;
    CryptoManager& operator=(const CryptoManager&) = delete;
    
    // Key storage
    uint8_t esp32_private_key[ECDH_PRIVATE_KEY_SIZE];
    uint8_t esp32_public_key[ECDH_PUBLIC_KEY_SIZE];
    uint8_t client_public_key[ECDH_PUBLIC_KEY_SIZE];
    uint8_t shared_secret[SHARED_SECRET_SIZE];
    uint8_t session_aes_key[SESSION_KEY_SIZE];
    uint8_t pending_challenge[CHALLENGE_SIZE];
    
    // State
    bool ecdh_ready;
    bool challenge_pending;
    pairing_state_t pairing_state;
    bool has_stored_client_key;
    uint8_t stored_client_public_key[ECDH_PUBLIC_KEY_SIZE];
};

#endif // CRYPTO_MANAGER_H
