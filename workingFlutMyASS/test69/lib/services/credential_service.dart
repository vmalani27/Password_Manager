import 'dart:async';
import 'package:flutter/foundation.dart';
import '../constants/ble_constants.dart';
import '../models/credential.dart';
import 'command_service.dart';

/// High-level service for credential CRUD operations
/// Sends commands to ESP32 and parses responses
/// ESP32 commands: add, get, update, delete, list
class CredentialService {
  final CommandService _commandService;
  
  CredentialService(this._commandService);
  
  /// Add a new credential to ESP32
  /// ESP32 code: if (cmd.equalsIgnoreCase("add") && tokens.size() == 4) { insertCredential(...); sendNotification(ok ? "Added" : "ADD FAIL"); }
  Future<bool> addCredential({
    required String deviceId,
    required String site,
    required String username,
    required String password,
  }) async {
    debugPrint('[Credential] Adding: $site / $username');
    
    try {
      final response = await _commandService.sendCommand(
        Esp32Commands.add(site, username, password),
        timeout: BleConstants.commandTimeout,
      );
      
      // Parse response: "Added" or "ADD FAIL"
      if (response.contains(Esp32Commands.added)) {
        debugPrint('[Credential] ✓ Added successfully');
        return true;
      } else if (response.contains(Esp32Commands.addFail)) {
        debugPrint('[Credential] ✗ Add failed');
        return false;
      } else {
        throw Exception('Unexpected add response: $response');
      }
      
    } catch (e) {
      debugPrint('[Credential] ✗ Add error: $e');
      rethrow;
    }
  }
  
  /// Get password for a specific credential
  /// ESP32 code: if (cmd.equalsIgnoreCase("get") && tokens.size() == 3) { String pw = getPassword(...); sendNotification("Password: " + pw); }
  Future<String?> getPassword({
    required String deviceId,
    required String site,
    required String username,
  }) async {
    debugPrint('[Credential] Getting password: $site / $username');
    
    try {
      final response = await _commandService.sendCommand(
        Esp32Commands.get(site, username),
        timeout: BleConstants.commandTimeout,
      );
      
      // Parse response: "Password: xyz" or "NOT FOUND"
      if (response.startsWith(Esp32Commands.passwordPrefix)) {
        final password = response.substring(Esp32Commands.passwordPrefix.length).trim();
        debugPrint('[Credential] ✓ Password retrieved');
        return password;
      } else if (response.contains(Esp32Commands.notFound)) {
        debugPrint('[Credential] Credential not found');
        return null;
      } else {
        throw Exception('Unexpected get response: $response');
      }
      
    } catch (e) {
      debugPrint('[Credential] ✗ Get error: $e');
      rethrow;
    }
  }
  /// Update password for existing credential
  /// ESP32 code: if (cmd.equalsIgnoreCase("update") && tokens.size() == 4) { updateCredential(...); sendNotification(ok ? "Updated" : "UPDATE FAIL"); }
  Future<bool> updateCredential({
    required String deviceId,
    required String site,
    required String username,
    required String newPassword,
  }) async {
    debugPrint('[Credential] Updating: $site / $username');
    
    try {
      final response = await _commandService.sendCommand(
        Esp32Commands.update(site, username, newPassword),
        timeout: BleConstants.commandTimeout,
      );
      
      // Parse response: "Updated" or "UPDATE FAIL"
      if (response.contains(Esp32Commands.updated)) {
        debugPrint('[Credential] ✓ Updated successfully');
        return true;
      } else if (response.contains(Esp32Commands.updateFail)) {
        debugPrint('[Credential] ✗ Update failed');
        return false;
      } else {
        throw Exception('Unexpected update response: $response');
      }
      
    } catch (e) {
      debugPrint('[Credential] ✗ Update error: $e');
      rethrow;
    }
  }
  
  /// Delete a credential
  /// ESP32 code: if (cmd.equalsIgnoreCase("delete") && tokens.size() == 3) { deleteCredential(...); sendNotification(ok ? "Deleted" : "DELETE FAIL"); }
  Future<bool> deleteCredential({
    required String deviceId,
    required String site,
    required String username,
  }) async {
    debugPrint('[Credential] Deleting: $site / $username');
    
    try {
      final response = await _commandService.sendCommand(
        Esp32Commands.delete(site, username),
        timeout: BleConstants.commandTimeout,
      );
      
      // Parse response: "Deleted" or "DELETE FAIL"
      if (response.contains(Esp32Commands.deleted)) {
        debugPrint('[Credential] ✓ Deleted successfully');
        return true;
      } else if (response.contains(Esp32Commands.deleteFail)) {
        debugPrint('[Credential] ✗ Delete failed');
        return false;
      } else {
        throw Exception('Unexpected delete response: $response');
      }
      
    } catch (e) {
      debugPrint('[Credential] ✗ Delete error: $e');
      rethrow;
    }
  }
  
  /// List all credentials (returns site/username pairs only, no passwords)
  /// ESP32 code: if (cmd.equalsIgnoreCase("list")) { String out = listCredentials(); sendNotification("LIST:\n" + out); }
  /// ESP32 format: "LIST:\nSite: example.com | User: john@example.com\nSite: google.com | User: jane@gmail.com"
  Future<List<Credential>> listCredentials(String deviceId) async {
    debugPrint('[Credential] Listing all credentials...');
    
    try {
      final response = await _commandService.sendCommand(
        Esp32Commands.list,
        timeout: BleConstants.commandTimeout,
      );
      
      // Parse response: "LIST:\n<credentials>"
      if (!response.startsWith(Esp32Commands.listPrefix)) {
        throw Exception('Unexpected list response: $response');
      }
      
      // Extract credential lines
      final content = response.substring(Esp32Commands.listPrefix.length).trim();
      
      // Handle empty list
      if (content.isEmpty || content == '(none)') {
        debugPrint('[Credential] ✓ No credentials found');
        return [];
      }
      
      // Parse each line: "Site: example.com | User: john@example.com"
      final lines = content.split('\n');
      final credentials = <Credential>[];
      
      for (final line in lines) {
        if (line.trim().isEmpty) continue;
        
        try {
          final credential = Credential.fromEsp32Response(line);
          credentials.add(credential);
        } catch (e) {
          debugPrint('[Credential] Warning: Failed to parse line: $line - $e');
          // Continue parsing other lines
        }
      }
      
      debugPrint('[Credential] ✓ Retrieved ${credentials.length} credential(s)');
      return credentials;
      
    } catch (e) {
      debugPrint('[Credential] ✗ List error: $e');
      rethrow;
    }
  }
  
  /// Get full credential with password
  /// This is a convenience method that combines list + get operations
  Future<Credential?> getFullCredential({
    required String deviceId,
    required String site,
    required String username,
  }) async {
    final password = await getPassword(deviceId: deviceId, site: site, username: username);
    
    if (password == null) {
      return null;
    }
    
    return Credential(
      site: site,
      username: username,
      password: password,
    );
  }
}