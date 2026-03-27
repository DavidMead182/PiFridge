import 'package:flutter/material.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import 'package:openfoodfacts/openfoodfacts.dart';
import 'sensor_service.dart';

void main() {
  OpenFoodAPIConfiguration.userAgent = UserAgent(
    name: 'PiFridge',
    version: '1.0.0',
  );
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

// --- FOOD MODEL ---
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

// ─────────────────────────────────────────────
//  DASHBOARD
// ─────────────────────────────────────────────
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
    FoodItem(
      name: "Organic Eggs",
      icon: "🥚",
      expirationDate: "Expires in 4 days",
    ),
    FoodItem(
      name: "Whole Milk",
      icon: "🥛",
      expirationDate: "Expires in 2 days",
    ),
  ];

  @override
  void initState() {
    super.initState();
    // Listen to the Pi sensor stream
    _sensorService.getAllSensorData().listen((data) {
      if (mounted) {
        setState(() {
          _sensorData = data;
          _piConnected = _sensorService.piConnected;
        });
      }
    });
  }

  // ── Connection banner ──
  Widget _buildConnectionBanner() {
    return AnimatedContainer(
      duration: const Duration(milliseconds: 500),
      width: double.infinity,
      padding: const EdgeInsets.symmetric(vertical: 8, horizontal: 16),
      color: _piConnected ? Colors.green.shade900 : Colors.red.shade900,
      child: Row(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Icon(
            _piConnected ? Icons.wifi : Icons.wifi_off,
            size: 16,
            color: _piConnected ? Colors.greenAccent : Colors.redAccent,
          ),
          const SizedBox(width: 8),
          Text(
            _piConnected
                ? "● Raspberry Pi connected · ${_formatTime(_sensorData.lastUpdated)}"
                : "⚠ Pi Disconnected — Showing Last Known Data",
            style: TextStyle(
              fontSize: 12,
              color: _piConnected ? Colors.greenAccent : Colors.redAccent,
            ),
          ),
        ],
      ),
    );
  }

  // ── Vitals row ──
  Widget _buildVitals() {
    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceEvenly,
      children: [
        _buildVitalCard(
          "Temp",
          "${_sensorData.temperature.toStringAsFixed(1)}°C",
          _sensorData.temperature > 8.0 ? Colors.red : Colors.blue,
        ),
        _buildVitalCard(
          "Humidity",
          "${_sensorData.humidity.toStringAsFixed(0)}%",
          Colors.teal,
        ),
        _buildVitalCard(
          "Door",
          _sensorData.doorOpen ? "OPEN" : "CLOSED",
          _sensorData.doorOpen ? Colors.red : Colors.green,
        ),
        _buildVitalCard(
          "Lux",
          "${_sensorData.lux.toInt()} lx",
          Colors.amber,
        ),
      ],
    );
  }

  Widget _buildVitalCard(String label, String value, Color color) {
    return Card(
      elevation: 4,
      child: Padding(
        padding: const EdgeInsets.all(12.0),
        child: Column(
          children: [
            Text(label, style: const TextStyle(fontSize: 11)),
            const SizedBox(height: 4),
            Text(
              value,
              style: TextStyle(
                fontSize: 16,
                fontWeight: FontWeight.bold,
                color: color,
              ),
            ),
          ],
        ),
      ),
    );
  }

  // ── Inventory list ──
  Widget _buildInventory() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            const Text(
              "Fridge Contents",
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            Text(
              "${_inventory.length} items",
              style: const TextStyle(color: Colors.grey),
            ),
          ],
        ),
        const SizedBox(height: 8),
        ListView.builder(
          shrinkWrap: true,
          physics: const NeverScrollableScrollPhysics(),
          itemCount: _inventory.length,
          itemBuilder: (context, index) {
            final item = _inventory[index];
            return Card(
              margin: const EdgeInsets.symmetric(vertical: 4),
              child: ListTile(
                leading: Text(item.icon, style: const TextStyle(fontSize: 28)),
                title: Text(item.name),
                subtitle: Text("${item.expirationDate} · ${_formatTime(item.addedAt)}"),
                trailing: IconButton(
                  icon: const Icon(Icons.delete_outline, color: Colors.red),
                  onPressed: () => setState(() => _inventory.removeAt(index)),
                ),
              ),
            );
          },
        ),
      ],
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text("🧊 PiFridge"),
        centerTitle: true,
      ),
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
        icon: const Icon(Icons.add),
        label: const Text("Add Item"),
      ),
    );
  }

  // ── Logic Methods ──

  void _showAddOptions() {
    showModalBottomSheet(
      context: context,
      builder: (context) => SafeArea(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            ListTile(
              leading: const Icon(Icons.qr_code_scanner, color: Colors.cyan),
              title: const Text("Scan barcode with phone"),
              onTap: () {
                Navigator.pop(context);
                _openPhoneScanner();
              },
            ),
            ListTile(
              leading: const Icon(Icons.edit_note, color: Colors.amber),
              title: const Text("Enter manually"),
              onTap: () {
                Navigator.pop(context);
                _showManualEntryDialog();
              },
            ),
          ],
        ),
      ),
    );
  }

  void _openPhoneScanner() {
    Navigator.push(
      context,
      MaterialPageRoute(
        builder: (context) => PhoneScannerScreen(
          onBarcodeScanned: (barcode) async {
            Navigator.pop(context);
            await _lookupAndAddBarcode(barcode);
          },
        ),
      ),
    );
  }

  Future<void> _lookupAndAddBarcode(String barcode) async {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text("Looking up $barcode...")),
    );

    try {
      final config = ProductQueryConfiguration(
        barcode,
        version: ProductQueryVersion.v3,
        fields: [ProductField.NAME],
      );
      final result = await OpenFoodAPIClient.getProductV3(config);

      if (result.product != null) {
        setState(() {
          _inventory.add(FoodItem(
            name: result.product!.productName ?? "Unknown Product",
            icon: "📦",
            expirationDate: "Added via scan",
            barcode: barcode,
          ));
        });
      } else {
        _showManualEntryDialog(prefillBarcode: barcode);
      }
    } catch (e) {
      debugPrint("Lookup failed: $e");
    }
  }

  void _showManualEntryDialog({String? prefillBarcode}) {
    final nameController = TextEditingController();
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text("Manual Entry"),
        content: TextField(
          controller: nameController,
          decoration: const InputDecoration(labelText: "Food Name"),
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(context), child: const Text("Cancel")),
          ElevatedButton(
            onPressed: () {
              setState(() {
                _inventory.add(FoodItem(
                  name: nameController.text,
                  icon: "🍴",
                  expirationDate: "Manual entry",
                ));
              });
              Navigator.pop(context);
            },
            child: const Text("Add"),
          ),
        ],
      ),
    );
  }

  String _formatTime(DateTime dt) {
    final diff = DateTime.now().difference(dt);
    if (diff.inSeconds < 60) return "just now";
    return "${diff.inMinutes}m ago";
  }
}

// ─────────────────────────────────────────────
//  PHONE SCANNER SCREEN
// ─────────────────────────────────────────────
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