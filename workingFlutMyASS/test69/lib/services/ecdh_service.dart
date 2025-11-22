import 'dart:math';
import 'dart:typed_data';
import 'package:flutter/foundation.dart';
import 'package:pointycastle/export.dart';
import 'package:convert/convert.dart';

/// ECDH (Elliptic Curve Diffie-Hellman) service for secure key exchange
/// Implements secp256r1 curve with HKDF key derivation
class EcdhService {
  ECPrivateKey? _privateKey;
  ECPublicKey? _publicKey;
  Uint8List? _sharedSecret;
  Uint8List? _sessionKey;
  
  bool get isReady => _sessionKey != null;
  
  /// Generate ephemeral ECDH key pair (secp256r1)
  void generateKeyPair() {
    final params = ECKeyGeneratorParameters(ECCurve_secp256r1());
    final random = FortunaRandom();
    
    // Seed random with secure random bytes
    final seedSource = Random.secure();
    final seeds = List<int>.generate(32, (_) => seedSource.nextInt(256));
    random.seed(KeyParameter(Uint8List.fromList(seeds)));
    
    final generator = ECKeyGenerator();
    generator.init(ParametersWithRandom(params, random));
    
    final keyPair = generator.generateKeyPair();
    _privateKey = keyPair.privateKey as ECPrivateKey;
    _publicKey = keyPair.publicKey as ECPublicKey;
    
    debugPrint('[ECDH] Key pair generated');
  }
  
  /// Load existing key pair from saved bytes (for reconnection)
  void loadKeyPair(Uint8List privateKeyBytes, Uint8List publicKeyBytes) {
    if (privateKeyBytes.length != 32) {
      throw Exception('Invalid private key length: ${privateKeyBytes.length}');
    }
    if (publicKeyBytes.length != 64) {
      throw Exception('Invalid public key length: ${publicKeyBytes.length}');
    }
    
    final curve = ECCurve_secp256r1();
    
    // Reconstruct private key
    final d = _bytesToBigInt(privateKeyBytes);
    _privateKey = ECPrivateKey(d, curve);
    
    // Reconstruct public key
    final xHex = hex.encode(publicKeyBytes.sublist(0, 32));
    final yHex = hex.encode(publicKeyBytes.sublist(32, 64));
    final x = BigInt.parse(xHex, radix: 16);
    final y = BigInt.parse(yHex, radix: 16);
    final point = curve.curve.createPoint(x, y);
    _publicKey = ECPublicKey(point, curve);
    
    debugPrint('[ECDH] Key pair loaded from storage');
  }
  
  /// Get private key as 32 bytes (for persistent storage)
  Uint8List getPrivateKeyBytes() {
    if (_privateKey == null) throw Exception('Private key not available');
    return _bigIntToBytes(_privateKey!.d!, 32);
  }
  
  /// Get public key as 64 bytes (X||Y coordinates)
  Uint8List getPublicKeyBytes() {
    if (_publicKey == null) throw Exception('Key pair not generated');
    
    final q = _publicKey!.Q!;
    final x = q.x!.toBigInteger()!.toRadixString(16).padLeft(64, '0');
    final y = q.y!.toBigInteger()!.toRadixString(16).padLeft(64, '0');
    
    debugPrint('[ECDH] Public key X: $x');
    debugPrint('[ECDH] Public key Y: $y');
    
    return Uint8List.fromList(hex.decode(x + y));
  }
  
  /// Compute shared secret from ESP32's public key
  void computeSharedSecret(Uint8List esp32PublicKey) {
    if (_privateKey == null) throw Exception('Private key not available');
    if (esp32PublicKey.length != 64) {
      throw Exception('Invalid public key length: ${esp32PublicKey.length}');
    }
    
    // Parse ESP32's public key coordinates
    final xHex = hex.encode(esp32PublicKey.sublist(0, 32));
    final yHex = hex.encode(esp32PublicKey.sublist(32, 64));
    
    debugPrint('[ECDH] ESP32 public key X: $xHex');
    debugPrint('[ECDH] ESP32 public key Y: $yHex');
    
    final x = BigInt.parse(xHex, radix: 16);
    final y = BigInt.parse(yHex, radix: 16);
    
    final curve = ECCurve_secp256r1();
    final point = curve.curve.createPoint(x, y);
    final esp32PubKey = ECPublicKey(point, curve);
    
    // Compute shared secret: d_A * Q_B
    final sharedPoint = esp32PubKey.Q! * _privateKey!.d;
    if (sharedPoint == null || sharedPoint.x == null) {
      throw Exception('Failed to compute shared secret');
    }
    
    _sharedSecret = _bigIntToBytes(sharedPoint.x!.toBigInteger()!, 32);
    debugPrint('[ECDH] Shared secret: ${hex.encode(_sharedSecret!)}');
    
    // Derive session key using HKDF-like approach (matching ESP32)
    _sessionKey = _deriveSessionKey(_sharedSecret!);
    debugPrint('[ECDH] Session key derived: ${hex.encode(_sessionKey!)}');
  }
  
  /// Derive session key using HKDF-like approach (matches ESP32 implementation)
  Uint8List _deriveSessionKey(Uint8List sharedSecret) {
    final hmac = HMac(SHA256Digest(), 64);
    
    // HKDF-Extract: PRK = HMAC-SHA256(salt, shared_secret)
    final salt = Uint8List.fromList('BLE_PASSWORD_MGR'.codeUnits);
    hmac.init(KeyParameter(salt));
    final prk = hmac.process(sharedSecret);
    
    // HKDF-Expand: session_key = HMAC-SHA256(PRK, info || 0x01)
    final info = Uint8List.fromList([...'SESSION'.codeUnits, 0x01]);
    hmac.reset();
    hmac.init(KeyParameter(prk));
    final sessionKey = hmac.process(info);
    
    return sessionKey;
  }
  
  /// Compute HMAC-SHA256 for challenge-response
  Uint8List computeHmac(Uint8List challenge) {
    if (_sessionKey == null) throw Exception('Session key not derived');
    
    final hmac = HMac(SHA256Digest(), 64);
    hmac.init(KeyParameter(_sessionKey!));
    return hmac.process(challenge);
  }
  
  /// Get session key (for future AES-GCM encryption)
  Uint8List? get sessionKey => _sessionKey;
  
  /// Clear all sensitive data
  void clear() {
    _privateKey = null;
    _publicKey = null;
    _sharedSecret?.fillRange(0, _sharedSecret!.length, 0);
    _sessionKey?.fillRange(0, _sessionKey!.length, 0);
    _sharedSecret = null;
    _sessionKey = null;
    debugPrint('[ECDH] Cleared sensitive data');
  }
  
  /// Convert BigInt to fixed-length byte array
  Uint8List _bigIntToBytes(BigInt number, int length) {
    final hexStr = number.toRadixString(16).padLeft(length * 2, '0');
    return Uint8List.fromList(hex.decode(hexStr));
  }
  
  /// Convert byte array to BigInt
  BigInt _bytesToBigInt(Uint8List bytes) {
    final hexStr = hex.encode(bytes);
    return BigInt.parse(hexStr, radix: 16);
  }
}
