import 'package:flutter/material.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import 'package:openfoodfacts/openfoodfacts.dart';
import 'sensor_service.dart';

void main() {
  OpenFoodAPIConfiguration.userAgent = UserAgent(name: 'PiFridge', version: '1.0.0');
  OpenFoodAPIConfiguration.globalLanguages = [OpenFoodFactsLanguage.ENGLISH];
  OpenFoodAPIConfiguration.globalCountry = OpenFoodFactsCountry.UNITED_KINGDOM;
  runApp(const PiFridgeApp());
}

class PiFridgeApp extends StatelessWidget {
  const PiFridgeApp({super.key});
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'PiFridge',
      theme: ThemeData(
        colorSchemeSeed: Colors.cyan,
        useMaterial3: true,
        brightness: Brightness.dark,
      ),
      home: const FridgeDashboard(),
    );
  }
}

class FoodItem {
  final String name;
  final String icon;
  final String expirationDate;
  final String? barcode;
  final bool isManualEntry;
  final DateTime addedAt;

  FoodItem({
    required this.name,
    required this.icon,
    required this.expirationDate,
    this.barcode,
    this.isManualEntry = false,
    DateTime? addedAt,
  }) : addedAt = addedAt ?? DateTime.now();
}

class FridgeDashboard extends StatefulWidget {
  const FridgeDashboard({super.key});
  @override
  State<FridgeDashboard> createState() => _FridgeDashboardState();
}

class _FridgeDashboardState extends State<FridgeDashboard> {
  final SensorService _sensorService = SensorService();
  FridgeSensorData _sensorData = FridgeSensorData.initial();
  bool _piConnected = false;

  final List<FoodItem> _inventory = [
    FoodItem(name: "Organic Eggs", icon: "🥚", expirationDate: "Expires in 4 days"),
    FoodItem(name: "Whole Milk", icon: "🥛", expirationDate: "Expires in 2 days"),
  ];

  @override
  void initState() {
    super.initState();
    // Listen to the Pi Stream
    _sensorService.getAllSensorData().listen((data) {
      if (mounted) {
        setState(() {
          _sensorData = data;
          _piConnected = _sensorService.piConnected;
        });
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("🧊 PiFridge"), centerTitle: true),
      body: Column(
        children: [
          _buildConnectionBanner(),
          Expanded(
            child: SingleChildScrollView(
              padding: const EdgeInsets.all(16),
              child: Column(
                children: [
                  _buildVitals(),
                  const Divider(height: 40),
                  _buildInventory(),
                  const SizedBox(height: 80),
                ],
              ),
            ),
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: _showAddOptions,
        label: const Text("Add Item"),
        icon: const Icon(Icons.add),
      ),
    );
  }

  Widget _buildConnectionBanner() {
    return AnimatedContainer(
      duration: const Duration(milliseconds: 500),
      width: double.infinity,
      padding: const EdgeInsets.symmetric(vertical: 8, horizontal: 16),
      color: _piConnected ? Colors.green.shade900 : Colors.red.shade900,
      child: Text(
        _piConnected 
          ? "● Raspberry Pi Connected" 
          : "⚠ Pi Disconnected - Showing Last Known Data",
        style: TextStyle(fontSize: 12, color: _piConnected ? Colors.greenAccent : Colors.redAccent),
        textAlign: TextAlign.center,
      ),
    );
  }

  Widget _buildVitals() {
    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceEvenly,
      children: [
        _buildVitalCard("Temp", "${_sensorData.temperature.toStringAsFixed(1)}°C", 
            _sensorData.temperature > 8.0 ? Colors.red : Colors.blue),
        _buildVitalCard("Humidity", "${_sensorData.humidity.toStringAsFixed(0)}%", Colors.teal),
        _buildVitalCard("Door", _sensorData.doorOpen ? "OPEN" : "CLOSED", 
            _sensorData.doorOpen ? Colors.red : Colors.green),
        _buildVitalCard("Lux", "${_sensorData.lux.toInt()} lx", Colors.amber),
      ],
    );
  }

  Widget _buildVitalCard(String label, String value, Color color) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12.0),
        child: Column(
          children: [
            Text(label, style: const TextStyle(fontSize: 11)),
            Text(value, style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold, color: color)),
          ],
        ),
      ),
    );
  }

  // --- Inventory & Scanner logic (kept from your original) ---
  Widget _buildInventory() {
    return ListView.builder(
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      itemCount: _inventory.length,
      itemBuilder: (context, index) {
        final item = _inventory[index];
        return Card(
          child: ListTile(
            leading: Text(item.icon, style: const TextStyle(fontSize: 24)),
            title: Text(item.name),
            subtitle: Text(item.expirationDate),
            trailing: IconButton(
              icon: const Icon(Icons.delete, color: Colors.redAccent),
              onPressed: () => setState(() => _inventory.removeAt(index)),
            ),
          ),
        );
      },
    );
  }

  void _showAddOptions() { /* ... Same as your original ... */ }
  void _openPhoneScanner() { /* ... Same as your original ... */ }
  Future<void> _lookupAndAddBarcode(String barcode) async { /* ... Same as your original ... */ }
  void _showManualEntryDialog({String? prefillBarcode}) { /* ... Same as your original ... */ }
  String _formatTime(DateTime dt) { /* ... Same as your original ... */ }
}

class PhoneScannerScreen extends StatefulWidget {
  final void Function(String barcode) onBarcodeScanned;
  const PhoneScannerScreen({super.key, required this.onBarcodeScanned});
  @override
  State<PhoneScannerScreen> createState() => _PhoneScannerScreenState();
}

class _PhoneScannerScreenState extends State<PhoneScannerScreen> {
  bool _hasScanned = false;
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Phone Scanner")),
      body: MobileScanner(
        onDetect: (capture) {
          if (_hasScanned) return;
          final barcode = capture.barcodes.first.rawValue;
          if (barcode != null) {
            setState(() => _hasScanned = true);
            widget.onBarcodeScanned(barcode);
          }
        },
      ),
    );
  }
}