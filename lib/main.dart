import 'package:flutter/material.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import 'package:openfoodfacts/openfoodfacts.dart';

void main() {
  // --- Configure OpenFoodFacts API ---
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

// ─────────────────────────────────────────────
//  MODELS
// ─────────────────────────────────────────────

class FoodItem {
  final String name;
  final String icon;
  final String expirationDate;
  final String? barcode;
  final bool isManualEntry; // true = phone, false = Pi
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

class FridgeVitals {
  final double temperature;
  final int humidity;
  final bool doorOpen;
  final bool scannerActive;
  final bool piConnected;
  final DateTime lastUpdated;

  const FridgeVitals({
    required this.temperature,
    required this.humidity,
    required this.doorOpen,
    required this.scannerActive,
    required this.piConnected,
    required this.lastUpdated,
  });
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
  // Simulation Data (Replace with real Pi data later)
  FridgeVitals _vitals = FridgeVitals(
    temperature: 4.2,
    humidity: 45,
    doorOpen: false,
    scannerActive: false,
    piConnected: false,
    lastUpdated: DateTime.now(),
  );

  final List<FoodItem> _inventory = [
    FoodItem(
      name: "Organic Eggs (Dozen)",
      icon: "🥚",
      expirationDate: "Expires in 4 days",
      isManualEntry: false,
      addedAt: DateTime.now().subtract(const Duration(hours: 2)),
    ),
    FoodItem(
      name: "Whole Milk",
      icon: "🥛",
      expirationDate: "Expires in 2 days",
      isManualEntry: false,
      addedAt: DateTime.now().subtract(const Duration(hours: 5)),
    ),
  ];

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text("🧊 PiFridge"),
        centerTitle: true,
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: () => _simulateRefresh(),
          ),
        ],
      ),
      body: Column(
        children: [
          _buildConnectionBanner(),
          Expanded(
            child: SingleChildScrollView(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  _buildVitalsSection(),
                  const Divider(height: 40),
                  _buildInventorySection(),
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

  // --- Helper UI Methods ---

  Widget _buildConnectionBanner() {
    final bool connected = _vitals.piConnected;
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.symmetric(vertical: 8, horizontal: 16),
      color: connected ? Colors.green.withOpacity(0.2) : Colors.red.withOpacity(0.2),
      child: Row(
        children: [
          Icon(
            connected ? Icons.wifi : Icons.wifi_off,
            size: 16,
            color: connected ? Colors.greenAccent : Colors.redAccent,
          ),
          const SizedBox(width: 8),
          Text(
            connected
                ? "Pi Connected • ${_formatTime(_vitals.lastUpdated)}"
                : "Pi Not Connected — Offline Mode",
            style: TextStyle(
              fontSize: 12,
              color: connected ? Colors.greenAccent : Colors.redAccent,
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildVitalsSection() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const Text("Fridge Status", style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
        const SizedBox(height: 12),
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            _vitalCard("🌡 Temp", "${_vitals.temperature}°C", Colors.blue),
            _vitalCard("💧 Humid", "${_vitals.humidity}%", Colors.teal),
            _vitalCard("🚪 Door", _vitals.doorOpen ? "Open" : "Closed", _vitals.doorOpen ? Colors.red : Colors.green),
            _vitalCard("📡 Scan", _vitals.scannerActive ? "Active" : "Idle", _vitals.scannerActive ? Colors.amber : Colors.grey),
          ],
        ),
      ],
    );
  }

  Widget _vitalCard(String label, String value, Color color) {
    return Expanded(
      child: Card(
        child: Padding(
          padding: const EdgeInsets.symmetric(vertical: 12),
          child: Column(
            children: [
              Text(label, style: const TextStyle(fontSize: 10, color: Colors.grey)),
              const SizedBox(height: 4),
              Text(value, style: TextStyle(fontSize: 14, color: color, fontWeight: FontWeight.bold)),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildInventorySection() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            const Text("Inventory", style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
            Text("${_inventory.length} items", style: const TextStyle(color: Colors.grey)),
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
              margin: const EdgeInsets.only(bottom: 8),
              child: ListTile(
                leading: Text(item.icon, style: const TextStyle(fontSize: 24)),
                title: Text(item.name),
                subtitle: Text("${item.expirationDate} • ${item.isManualEntry ? '📱' : '🤖'}"),
                trailing: IconButton(
                  icon: const Icon(Icons.delete_outline, color: Colors.redAccent),
                  onPressed: () => setState(() => _inventory.removeAt(index)),
                ),
              ),
            );
          },
        ),
      ],
    );
  }

  // --- Logic Methods ---

  void _showAddOptions() {
    showModalBottomSheet(
      context: context,
      builder: (context) => Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          ListTile(
            leading: const Icon(Icons.qr_code_scanner),
            title: const Text("Scan with Phone"),
            onTap: () {
              Navigator.pop(context);
              _openPhoneScanner();
            },
          ),
          ListTile(
            leading: const Icon(Icons.edit),
            title: const Text("Manual Entry"),
            onTap: () {
              Navigator.pop(context);
              _showManualEntryDialog();
            },
          ),
        ],
      ),
    );
  }

  void _openPhoneScanner() {
    Navigator.push(
      context,
      MaterialPageRoute(
        builder: (context) => PhoneScannerScreen(
          onBarcodeScanned: (barcode) {
            Navigator.pop(context);
            _lookupAndAddBarcode(barcode);
          },
        ),
      ),
    );
  }

  Future<void> _lookupAndAddBarcode(String barcode) async {
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text("Searching for $barcode...")));

    try {
      final config = ProductQueryConfiguration(
        barcode,
        version: ProductQueryVersion.v3,
        fields: [ProductField.NAME],
      );
      final result = await OpenFoodAPIClient.getProductV3(config);

      if (result.product != null) {
        final name = result.product!.productName ?? "Unknown Item";
        setState(() {
          _inventory.add(FoodItem(
            name: name,
            icon: "📦",
            expirationDate: "Set Date",
            isManualEntry: true,
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
    // Standard dialog logic... (kept from your original snippet)
    final nameController = TextEditingController();
    final expirationController = TextEditingController();
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text("Manual Entry"),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: nameController,
              decoration: const InputDecoration(labelText: "Food Name"),
            ),
            TextField(
              controller: expirationController,
              decoration: const InputDecoration(labelText: "Expiration Date"),
            ),
            if (prefillBarcode != null)
              Padding(
                padding: const EdgeInsets.only(top: 10),
                child: Text("Barcode: $prefillBarcode", 
                  style: const TextStyle(fontSize: 12, color: Colors.grey)),
              ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text("Cancel"),
          ),
          ElevatedButton(
            onPressed: () {
              if (nameController.text.isNotEmpty) {
                setState(() {
                  _inventory.insert(0, FoodItem(
                    name: nameController.text,
                    icon: "📦",
                    expirationDate: expirationController.text.isEmpty 
                        ? "No date" 
                        : expirationController.text,
                    isManualEntry: true,
                    barcode: prefillBarcode,
                  ));
                });
                Navigator.pop(context);
              }
            },
            child: const Text("Add to Fridge"),
          ),
        ],
      ),
    );
  }

  void _simulateRefresh() {
    setState(() {
      _vitals = FridgeVitals(
        temperature: 3.8, // Slightly changed
        humidity: 42,
        doorOpen: false,
        scannerActive: false,
        piConnected: true, // Now "connected"
        lastUpdated: DateTime.now(),
      );
    });
  }

  String _formatTime(DateTime dt) {
    final diff = DateTime.now().difference(dt);
    if (diff.inMinutes < 1) return "just now";
    return "${diff.inMinutes}m ago";
  }
}

// ─────────────────────────────────────────────
//  SCANNER SCREEN
// ─────────────────────────────────────────────

class PhoneScannerScreen extends StatefulWidget {
  final Function(String) onBarcodeScanned;
  const PhoneScannerScreen({super.key, required this.onBarcodeScanned});

  @override
  State<PhoneScannerScreen> createState() => _PhoneScannerScreenState();
}

class _PhoneScannerScreenState extends State<PhoneScannerScreen> {
  bool _isProcessed = false;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Scanner")),
      body: MobileScanner(
        onDetect: (capture) {
          if (!_isProcessed) {
            final barcode = capture.barcodes.first.rawValue;
            if (barcode != null) {
              _isProcessed = true;
              widget.onBarcodeScanned(barcode);
            }
          }
        },
      ),
    );
  }
}