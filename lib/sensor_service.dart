import 'dart:async';
import 'package:http/http.dart' as http;
import 'dart:convert';

// Holds all sensor readings from the Pi in one object
class FridgeSensorData {
  final double temperature;
  final double humidity;
  final double lux;
  final bool doorOpen;
  final bool scannerActive;
  final DateTime lastUpdated;

  const FridgeSensorData({
    required this.temperature,
    required this.humidity,
    required this.lux,
    required this.doorOpen,
    required this.scannerActive,
    required this.lastUpdated,
  });

  // Used when Pi is offline — keeps the last known values
  FridgeSensorData copyWith({
    double? temperature,
    double? humidity,
    double? lux,
    bool? doorOpen,
    bool? scannerActive,
    DateTime? lastUpdated,
  }) {
    return FridgeSensorData(
      temperature: temperature ?? this.temperature,
      humidity: humidity ?? this.humidity,
      lux: lux ?? this.lux,
      doorOpen: doorOpen ?? this.doorOpen,
      scannerActive: scannerActive ?? this.scannerActive,
      lastUpdated: lastUpdated ?? this.lastUpdated,
    );
  }

  // Default "last known" values shown before Pi connects
  factory FridgeSensorData.initial() => FridgeSensorData(
        temperature: 4.2,
        humidity: 45,
        lux: 0.0,
        doorOpen: false,
        scannerActive: false,
        lastUpdated: DateTime.now(),
      );
}

class SensorService {
  // ⚠️ IMPORTANT: Replace this with your Pi's real IP address
  // Find it by typing 'hostname -I' on the Raspberry Pi terminal
  final String piIpAddress = "192.168.0.100";
  final String port = "5000";

  // Tracks whether Pi is reachable
  bool _piConnected = false;
  bool get piConnected => _piConnected;

  // Holds the last known values so UI never goes blank if Pi disconnects
  FridgeSensorData _lastKnownData = FridgeSensorData.initial();

  // ── Helper: makes a single GET request to the Pi ──
  Future<Map<String, dynamic>?> _get(String endpoint) async {
    try {
      final response = await http
          .get(Uri.parse('http://$piIpAddress:$port/$endpoint'))
          .timeout(const Duration(seconds: 1));

      if (response.statusCode == 200) {
        return jsonDecode(response.body) as Map<String, dynamic>;
      }
    } catch (_) {
      // Pi is off or unreachable — fail silently
    }
    return null;
  }

  // ── Main stream: fetches ALL sensor data every 2 seconds ──
  // This is what main.dart listens to in order to update the UI
  Stream<FridgeSensorData> getAllSensorData() async* {
    while (true) {
      await Future.delayed(const Duration(seconds: 2));

      try {
        // Fetch all endpoints in parallel for speed
        final results = await Future.wait([
          _get('temperature'),
          _get('humidity'),
          _get('lux'),
          _get('door'),
          _get('scanner'),
        ]);

        // If any result came back, Pi is connected
        final anyConnected = results.any((r) => r != null);
        _piConnected = anyConnected;

        if (anyConnected) {
          // Update last known data with whatever the Pi returned
          _lastKnownData = FridgeSensorData(
            temperature: results[0]?['temperature']?.toDouble() ?? _lastKnownData.temperature,
            humidity: results[1]?['humidity']?.toDouble() ?? _lastKnownData.humidity,
            lux: results[2]?['lux']?.toDouble() ?? _lastKnownData.lux,
            doorOpen: results[3]?['door_open'] ?? _lastKnownData.doorOpen,
            scannerActive: results[4]?['scanner_active'] ?? _lastKnownData.scannerActive,
            lastUpdated: DateTime.now(),
          );
        } else {
          // Pi offline — keep last known values, just update the timestamp
          _piConnected = false;
          _lastKnownData = _lastKnownData.copyWith(lastUpdated: DateTime.now());
        }

        yield _lastKnownData;
      } catch (e) {
        _piConnected = false;
        yield _lastKnownData;
      }
    }
  }

  // ── Individual streams (kept for backwards compatibility with lux screen) ──

  Stream<double> getLuxData() async* {
    await for (final data in getAllSensorData()) {
      yield data.lux;
    }
  }

  Stream<double> getTemperatureData() async* {
    await for (final data in getAllSensorData()) {
      yield data.temperature;
    }
  }

  Stream<bool> getDoorStatus() async* {
    await for (final data in getAllSensorData()) {
      yield data.doorOpen;
    }
  }

  // ── Send settings to the Pi (e.g. change reading frequency) ──
  Future<void> updateSensorSettings(int frequency) async {
    try {
      await http.post(
        Uri.parse('http://$piIpAddress:$port/settings'),
        body: jsonEncode({'frequency': frequency}),
        headers: {'Content-Type': 'application/json'},
      );
    } catch (e) {
      print("Failed to update Pi settings: $e");
    }
  }
}