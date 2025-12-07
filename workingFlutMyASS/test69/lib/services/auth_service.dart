import 'package:local_auth/local_auth.dart';

class AuthService {
 final LocalAuthentication _auth = LocalAuthentication();

Future<bool> authenticate({String reason = 'Please authenticate'}) async {
  try {
    // Check device capability
    final bool canAuthenticate = await _auth.canCheckBiometrics
                              || await _auth.isDeviceSupported();
    if (!canAuthenticate) {
      return false;
    }

    final bool didAuthenticate = await _auth.authenticate(
      localizedReason: reason,
      biometricOnly: true,       // only biometric (fingerprint/face)
      persistAcrossBackgrounding: true,          // keep auth if user leaves app temporarily
    );

    return didAuthenticate;
  } catch (e) {
    print('Local auth error: $e');
    return false;
  }
}
}