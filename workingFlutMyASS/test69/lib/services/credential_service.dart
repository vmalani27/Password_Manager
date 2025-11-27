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
  
  String _normalizeService(String service) {
    return service.replaceAll(' ', '_');
  }

  /// Add a new credential to ESP32
  /// ESP32 code: if (cmd.equalsIgnoreCase("add") && tokens.size() == 4) { insertCredential(...); sendNotification(ok ? "Added" : "ADD FAIL"); }
  Future<bool> addCredential({
    required String service,
    required String identifier,
    required String password,
  }) async {
    final normalizedService = _normalizeService(service);
    debugPrint('[Credential] Adding: $normalizedService / $identifier');
    
    try {
      final response = await _commandService.sendCommand(
        Esp32Commands.add(normalizedService, identifier, password),
        timeout: BleConstants.commandTimeout,
      );
      
      // Parse response: "Added" or "ADD FAIL"
      if (response.contains(Esp32Commands.added)) {
        debugPrint('[Credential] Added successfully');
        return true;
      } else if (response.contains(Esp32Commands.addFail)) {
        debugPrint('[Credential] Add failed - credential may already exist');
        throw Exception('Failed to add credential. It may already exist or storage is full.');
      } else {
        throw Exception('Unexpected add response: $response');
      }
      
    } catch (e) {
      debugPrint('[Credential] Add error: $e');
      rethrow;
    }
  }
  
  /// Get password for a specific credential
  /// ESP32 code: if (cmd.equalsIgnoreCase("get") && tokens.size() == 3) { String pw = getPassword(...); sendNotification("Password: " + pw); }
  Future<String?> getPassword({
    required String service,
    required String identifier,
  }) async {
    final normalizedService = _normalizeService(service);
    debugPrint('[Credential] Getting password: $normalizedService / $identifier');
    
    try {
      final response = await _commandService.sendCommand(
        Esp32Commands.get(normalizedService, identifier),
        timeout: BleConstants.commandTimeout,
      );
      
      // Parse response: "Password: xyz" or "NOT FOUND"
      if (response.startsWith(Esp32Commands.passwordPrefix)) {
        final password = response.substring(Esp32Commands.passwordPrefix.length).trim();
        debugPrint('[Credential] Password retrieved');
        return password;
      } else if (response.contains(Esp32Commands.notFound)) {
        debugPrint('[Credential] Credential not found');
        return null;
      } else {
        throw Exception('Unexpected get response: $response');
      }
      
    } catch (e) {
      debugPrint('[Credential] Get error: $e');
      rethrow;
    }
  }
  /// Update password for existing credential
  /// ESP32 code: if (cmd.equalsIgnoreCase("update") && tokens.size() == 4) { updateCredential(...); sendNotification(ok ? "Updated" : "UPDATE FAIL"); }
  Future<bool> updateCredential({
    required String service,
    required String identifier,
    required String newPassword,
  }) async {
    final normalizedService = _normalizeService(service);
    debugPrint('[Credential] Updating: $normalizedService / $identifier');
    
    try {
      final response = await _commandService.sendCommand(
        Esp32Commands.update(normalizedService, identifier, newPassword),
        timeout: BleConstants.commandTimeout,
      );
      
      // Parse response: "Updated" or "UPDATE FAIL"
      if (response.contains(Esp32Commands.updated)) {
        debugPrint('[Credential] Updated successfully');
        return true;
      } else if (response.contains(Esp32Commands.updateFail)) {
        debugPrint('[Credential] Update failed - credential not found');
        throw Exception('Failed to update credential. It may not exist.');
      } else {
        throw Exception('Unexpected update response: $response');
      }
      
    } catch (e) {
      debugPrint('[Credential] Update error: $e');
      rethrow;
    }
  }
  
  /// Delete a credential
  /// ESP32 code: if (cmd.equalsIgnoreCase("delete") && tokens.size() == 3) { deleteCredential(...); sendNotification(ok ? "Deleted" : "DELETE FAIL"); }
  Future<bool> deleteCredential({
    required String service,
    required String identifier,
  }) async {
    final normalizedService = _normalizeService(service);
    debugPrint('[Credential] Deleting: $normalizedService / $identifier');
    
    try {
      final response = await _commandService.sendCommand(
        Esp32Commands.delete(normalizedService, identifier),
        timeout: BleConstants.commandTimeout,
      );
      
      // Parse response: "Deleted" or "DELETE FAIL"
      if (response.contains(Esp32Commands.deleted)) {
        debugPrint('[Credential] Deleted successfully');
        return true;
      } else if (response.contains(Esp32Commands.deleteFail)) {
        debugPrint('[Credential] Delete failed - credential not found');
        throw Exception('Failed to delete credential. It may not exist.');
      } else {
        throw Exception('Unexpected delete response: $response');
      }
      
    } catch (e) {
      debugPrint('[Credential] Delete error: $e');
      rethrow;
    }
  }
  
  /// List all credentials (returns service/identifier pairs only, no passwords)
  /// ESP32 code: if (cmd.equalsIgnoreCase("list")) { String out = listCredentials(); sendNotification("LIST:\n" + out); }
  /// ESP32 actual format: "LIST:\njdjdir nfkfjd\ninstagram vmalanixx" (space-separated: site username)
  Future<List<Credential>> listCredentials(String deviceId) async {
    debugPrint('[Credential] Listing all credentials...');
    
    try {
      final response = await _commandService.sendCommand(
        Esp32Commands.list,
        timeout: BleConstants.commandTimeout,
      );
      
      debugPrint('[Credential] Raw LIST response: $response');
      
      // Check for NOT AUTHORIZED (session expired)
      if (response == 'NOT AUTHORIZED') {
        throw Exception('SESSION_TIMEOUT: Not authorized');
      }
      
      if (!response.startsWith('LIST:')) {
        throw Exception('Unexpected list response: $response');
      }
      
      // Extract content after "LIST:"
      // Response format: "LIST:\njdjdir nfkfjd" or "LIST:\n(none)"
      String content = response.substring(5); // Remove "LIST:"
      
      // Remove leading newline if present
      if (content.startsWith('\n')) {
        content = content.substring(1);
      } else if (content.startsWith('\\n')) {
        content = content.substring(2);
      }
      
      // Now trim whitespace
      content = content.trim();
      
      debugPrint('[Credential] Extracted content: "$content"');
      
      // Handle empty list - ESP32 sends "(none)" when no credentials exist
      if (content.isEmpty || content == '(none)') {
        debugPrint('[Credential] No credentials found');
        return [];
      }
      
      // Parse each line - Credential model handles ESP32's "Site: ... | User: ..." format
      final lines = content.split('\n');
      final credentials = <Credential>[];
      
      for (final line in lines) {
        if (line.trim().isEmpty) continue;
        
        try {
          final credential = Credential.fromEsp32Response(line);
          credentials.add(credential);
          debugPrint('[Credential] Parsed: ${credential.service} / ${credential.username}');
        } catch (e) {
          debugPrint('[Credential] Warning: Failed to parse line: $line - $e');
        }
      }
      
      debugPrint('[Credential] Retrieved ${credentials.length} credential(s)');
      return credentials;
      
    } catch (e) {
      debugPrint('[Credential] List error: $e');
      rethrow;
    }
  }
  
  /// Get full credential with password
  /// This is a convenience method that combines list + get operations
  Future<Credential?> getFullCredential({
    required String deviceId,
    required String service,
    required String identifier,
  }) async {
    final password = await getPassword(service: service, identifier: identifier);
    if (password == null) {
      return null;
    }
    return Credential(
      service: service,
      username: identifier,
      password: password,
    );
  }
}