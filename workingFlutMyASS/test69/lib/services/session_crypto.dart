import 'dart:typed_data';
import 'dart:convert';
import 'package:pointycastle/export.dart';

class SessionCrypto {
  final Uint8List sessionKey;

  SessionCrypto(this.sessionKey);

  /// Encrypt command for BLE transmission (AES-256-CTR)
  String encryptCommand(String plaintext) {
    final nonce = _generateNonce();
    final cipher = CTRStreamCipher(AESEngine())
      ..init(true, ParametersWithIV(KeyParameter(sessionKey), nonce));
    final plaintextBytes = utf8.encode(plaintext);
    final ciphertext = cipher.process(Uint8List.fromList(plaintextBytes));
    final combined = Uint8List(16 + ciphertext.length);
    combined.setRange(0, 16, nonce);
    combined.setRange(16, combined.length, ciphertext);
    return 'ENC:${base64Encode(combined)}';
  }

  /// Decrypt response from BLE transmission (AES-256-CTR)
  String decryptResponse(String encryptedResponse) {
    if (!encryptedResponse.startsWith('ENC:')) {
      return encryptedResponse;
    }
    final base64Data = encryptedResponse.substring(4);
    final decoded = base64Decode(base64Data);
    final nonce = decoded.sublist(0, 16);
    final ciphertext = decoded.sublist(16);
    final cipher = CTRStreamCipher(AESEngine())
      ..init(false, ParametersWithIV(KeyParameter(sessionKey), nonce));
    final plaintext = cipher.process(ciphertext);
    return utf8.decode(plaintext);
  }

  /// Generate random 16-byte nonce
  Uint8List _generateNonce() {
    final random = SecureRandom('Fortuna')
      ..seed(KeyParameter(
        Uint8List.fromList(
          List.generate(32, (_) => DateTime.now().millisecondsSinceEpoch % 256),
        ),
      ));
    return Uint8List.fromList(
      List.generate(16, (_) => random.nextUint8()),
    );
  }
}
