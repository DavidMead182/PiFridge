import 'dart:async';
import 'package:http/http.dart' as http;
import 'dart:convert';

class FridgeSensorData {
  final double temperature;
  final double humidity;
  final double lux;
  final bool doorOpen;
  final DateTime lastUpdated;

  const FridgeSensorData({
    required this.temperature,
    required this.humidity,
    required this.lux,
    required this.doorOpen,
    required this.lastUpdated,
  });

  factory FridgeSensorData.initial() => FridgeSensorData(
        temperature: 0.0,
        humidity: 0.0,
        lux: 0.0,
        doorOpen: false,
        lastUpdated: DateTime.now(),
      );

  // Maps the C++ JSON keys to Flutter
  factory FridgeSensorData.fromJson(Map<String, dynamic> json) {
    return FridgeSensorData(
      temperature: (json['temperature'] as num).toDouble(),
      humidity: (json['humidity'] as num).toDouble(),
      lux: (json['lux'] as num).toDouble(),
      doorOpen: json['door_open'] as bool, // Matches C++ key
      lastUpdated: DateTime.now(),
    );
  }
}

class SensorService {
  // Your Pi's Professional NGINX URL
  static const String _baseUrl = "http://10.56.198.47/api/fridge";

  bool _piConnected = false;
  bool get piConnected => _piConnected;

  Stream<FridgeSensorData> getAllSensorData() async* {
    while (true) {
      try {
        final response = await http
            .get(Uri.parse(_baseUrl))
            .timeout(const Duration(seconds: 2));

        if (response.statusCode == 200) {
          _piConnected = true;
          yield FridgeSensorData.fromJson(jsonDecode(response.body));
        } else {
          _piConnected = false;
        }
      } catch (e) {
        _piConnected = false;
      }
      // Poll every 1 second for a "Live" feel
      await Future.delayed(const Duration(seconds: 1));
    }
  }
}