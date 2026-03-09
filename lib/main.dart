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
  // SensorService fetches real data from the Pi
  final SensorService _sensorService = SensorService();

  // Starts with initial placeholder values, updates when Pi connects
  FridgeSensorData _sensorData = FridgeSensorData.initial();
  bool _piConnected = false;

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
  void initState() {
    super.initState();
    // Start listening to the Pi sensor stream
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
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.symmetric(vertical: 8, horizontal: 16),
      color: _piConnected ? Colors.green.shade900 : Colors.red.shade900,
      child: Row(
        children: [
          Icon(
            _piConnected ? Icons.wifi : Icons.wifi_off,
            size: 16,
            color: _piConnected ? Colors.greenAccent : Colors.redAccent,
          ),
          const SizedBox(width: 8),
          Expanded(
            child: Text(
              _piConnected
                  ? "Raspberry Pi connected · Last update: ${_formatTime(_sensorData.lastUpdated)}"
                  : "⚠ Raspberry Pi not connected — showing last known data",
              style: TextStyle(
                fontSize: 12,
                color: _piConnected ? Colors.greenAccent : Colors.redAccent,
              ),
            ),
          ),
        ],
      ),
    );
  }

  // ── Vitals row — now uses REAL data from Pi ──
  Widget _buildVitals() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const Text(
          "Fridge Status",
          style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
        ),
        const SizedBox(height: 10),
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceEvenly,
          children: [
            _buildVitalCard(
              "🌡 Temp",
              "${_sensorData.temperature.toStringAsFixed(1)}°C",
              Colors.blue,
            ),
            _buildVitalCard(
              "💧 Humidity",
              "${_sensorData.humidity.toStringAsFixed(0)}%",
              Colors.teal,
            ),
            _buildVitalCard(
              "🚪 Door",
              _sensorData.doorOpen ? "Open" : "Closed",
              _sensorData.doorOpen ? Colors.red : Colors.green,
            ),
            _buildVitalCard(
              "💡 Lux",
              "${_sensorData.lux.toStringAsFixed(0)} lx",
              Colors.amber,
            ),
          ],
        ),
      ],
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
                subtitle: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(item.expirationDate),
                    const SizedBox(height: 4),
                    Row(
                      children: [
                        Container(
                          padding: const EdgeInsets.symmetric(
                            horizontal: 6,
                            vertical: 2,
                          ),
                          decoration: BoxDecoration(
                            color: item.isManualEntry
                                ? Colors.amber.shade900
                                : Colors.cyan.shade900,
                            borderRadius: BorderRadius.circular(4),
                          ),
                          child: Text(
                            item.isManualEntry
                                ? "📱 Added via phone"
                                : "🤖 Added by Pi scanner",
                            style: const TextStyle(fontSize: 10),
                          ),
                        ),
                        const SizedBox(width: 6),
                        Text(
                          _formatTime(item.addedAt),
                          style: const TextStyle(
                            fontSize: 10,
                            color: Colors.grey,
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
                trailing: IconButton(
                  icon: const Icon(Icons.delete_outline, color: Colors.red),
                  onPressed: () {
                    setState(() => _inventory.removeAt(index));
                    ScaffoldMessenger.of(context).showSnackBar(
                      const SnackBar(
                        content: Text("Item removed from inventory"),
                        duration: Duration(seconds: 2),
                      ),
                    );
                  },
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
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            tooltip: "Refresh Pi data",
            onPressed: () {
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(
                  content: Text("Refreshing data from Raspberry Pi..."),
                  duration: Duration(seconds: 2),
                ),
              );
            },
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
        tooltip: "Backup: add item manually or scan with phone",
      ),
    );
  }

  void _showAddOptions() {
    showModalBottomSheet(
      context: context,
      builder: (context) => SafeArea(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Padding(
              padding: EdgeInsets.all(16),
              child: Text(
                "Add Item (Backup — Pi scanner missed something?)",
                style: TextStyle(fontWeight: FontWeight.bold, fontSize: 15),
                textAlign: TextAlign.center,
              ),
            ),
            ListTile(
              leading: const Icon(Icons.qr_code_scanner, color: Colors.cyan),
              title: const Text("Scan barcode with phone"),
              subtitle: const Text("Use phone camera as backup scanner"),
              onTap: () {
                Navigator.pop(context);
                _openPhoneScanner();
              },
            ),
            ListTile(
              leading: const Icon(Icons.edit_note, color: Colors.amber),
              title: const Text("Enter manually"),
              subtitle: const Text("Type in food details by hand"),
              onTap: () {
                Navigator.pop(context);
                _showManualEntryDialog();
              },
            ),
            const SizedBox(height: 8),
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
    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text("Looking up barcode $barcode..."),
        duration: const Duration(seconds: 2),
      ),
    );

    try {
      final config = ProductQueryConfiguration(
        barcode,
        version: ProductQueryVersion.v3,
        fields: [ProductField.NAME, ProductField.CATEGORIES],
      );
      final result = await OpenFoodAPIClient.getProductV3(config);

      if (result.product != null) {
        final product = result.product!;
        final name = product.productName ?? "Unknown Product";
        setState(() {
          _inventory.add(FoodItem(
            name: name,
            icon: "📦",
            expirationDate: "Check packaging",
            barcode: barcode,
            isManualEntry: true,
          ));
        });
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text("'$name' added via phone scan"),
              backgroundColor: Colors.green.shade800,
            ),
          );
        }
      } else {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(
              content: Text("Product not found — please enter details manually"),
              backgroundColor: Colors.orange,
            ),
          );
          _showManualEntryDialog(prefillBarcode: barcode);
        }
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text("Lookup failed: $e")),
        );
      }
    }
  }

  void _showManualEntryDialog({String? prefillBarcode}) {
    final nameController = TextEditingController();
    final barcodeController = TextEditingController(text: prefillBarcode ?? '');
    final expirationController = TextEditingController();

    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text("Manual Entry"),
        content: SingleChildScrollView(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              TextField(
                controller: nameController,
                decoration: const InputDecoration(
                  labelText: "Food Name *",
                  hintText: "e.g., Whole Milk",
                ),
              ),
              const SizedBox(height: 12),
              TextField(
                controller: expirationController,
                decoration: const InputDecoration(
                  labelText: "Expiration *",
                  hintText: "e.g., Expires in 5 days",
                ),
              ),
              const SizedBox(height: 12),
              TextField(
                controller: barcodeController,
                decoration: const InputDecoration(
                  labelText: "Barcode (optional)",
                ),
              ),
            ],
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text("Cancel"),
          ),
          ElevatedButton(
            onPressed: () {
              if (nameController.text.isEmpty ||
                  expirationController.text.isEmpty) {
                ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(
                      content: Text("Please fill in required fields")),
                );
                return;
              }
              setState(() {
                _inventory.add(FoodItem(
                  name: nameController.text,
                  icon: "📦",
                  expirationDate: expirationController.text,
                  barcode: barcodeController.text.isNotEmpty
                      ? barcodeController.text
                      : null,
                  isManualEntry: true,
                ));
              });
              Navigator.pop(context);
              ScaffoldMessenger.of(context).showSnackBar(
                SnackBar(
                  content: Text("'${nameController.text}' added manually"),
                  duration: const Duration(seconds: 2),
                ),
              );
            },
            child: const Text("Add to Fridge"),
          ),
        ],
      ),
    );
  }

  Widget _buildVitalCard(String label, String value, Color color) {
    return Card(
      child: Container(
        padding: const EdgeInsets.all(12),
        width: 80,
        child: Column(
          children: [
            Text(
              label,
              style: const TextStyle(fontSize: 11, fontWeight: FontWeight.w600),
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 6),
            Text(
              value,
              style: TextStyle(
                fontSize: 14,
                color: color,
                fontWeight: FontWeight.bold,
              ),
              textAlign: TextAlign.center,
            ),
          ],
        ),
      ),
    );
  }

  String _formatTime(DateTime dt) {
    final diff = DateTime.now().difference(dt);
    if (diff.inMinutes < 1) return "just now";
    if (diff.inMinutes < 60) return "${diff.inMinutes}m ago";
    if (diff.inHours < 24) return "${diff.inHours}h ago";
    return "${diff.inDays}d ago";
  }
}

// ─────────────────────────────────────────────
//  PHONE SCANNER SCREEN (backup scanner)
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
      appBar: AppBar(
        title: const Text("📱 Phone Scanner (Backup)"),
        centerTitle: true,
      ),
      body: Stack(
        children: [
          MobileScanner(
            onDetect: (capture) {
              if (_hasScanned) return;
              final barcodes = capture.barcodes;
              if (barcodes.isNotEmpty && barcodes.first.rawValue != null) {
                setState(() => _hasScanned = true);
                widget.onBarcodeScanned(barcodes.first.rawValue!);
              }
            },
          ),
          Align(
            alignment: Alignment.bottomCenter,
            child: Container(
              margin: const EdgeInsets.all(24),
              padding: const EdgeInsets.all(16),
              decoration: BoxDecoration(
                color: Colors.black87,
                borderRadius: BorderRadius.circular(12),
              ),
              child: const Text(
                "Point at the barcode on the food packaging.\n"
                "This is a backup — the Pi scanner handles this automatically.",
                textAlign: TextAlign.center,
                style: TextStyle(color: Colors.white70),
              ),
            ),
          ),
        ],
      ),
    );
  }
}