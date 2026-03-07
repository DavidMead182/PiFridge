# 🧊 PiFridge — Mobile Dashboard

> **⚠️ STATUS: IN DEVELOPMENT**
> This is the Flutter mobile app for the PiFridge ecosystem. It acts as the **display interface** for the Raspberry Pi smart fridge system, showing live fridge data (temperature, humidity, door status, inventory) sent from the Pi. It also includes a **backup barcode scanner** using the phone camera for when the Pi's physical scanner misses something.

---

## 📂 Understanding the Project Files

In Flutter you write one codebase in Dart and it runs on Android, Web, and Windows automatically.

- **`lib/main.dart`** — The entire app. Contains all the UI and logic.
- **`pubspec.yaml`** — The package manager. Lists all dependencies like the barcode scanner and OpenFoodFacts API.
- **`android/`**, **`web/`**, **`windows/`** — Platform wrappers. You rarely need to touch these. They just host the Dart code on each platform.

---

## 🖥️ How the App Fits Into the Bigger System

```
[Raspberry Pi]
  ├── Physical barcode scanner (scans food automatically when fridge opens)
  ├── Temperature & humidity sensors
  ├── Door open/close sensor
  └── Sends all data → [This Mobile App]

[This Mobile App]
  ├── Displays live fridge vitals (temp, humidity, door status, scanner status)
  ├── Shows food inventory (what the Pi scanner has logged)
  └── Backup: lets you scan barcodes with your phone camera if Pi misses something
```

> The app currently uses **simulated/hardcoded data**. The Pi connection (via HTTP/WebSocket/MQTT) has not been wired up yet — that comes later.

---

## 🚀 How to Run the App

### Prerequisites
Make sure you have these installed:
- [Flutter SDK](https://docs.flutter.dev/get-started/install) (this project uses 3.41.4)
- Android SDK with cmdline-tools (easiest via [Android Studio](https://developer.android.com/studio))
- ADB (Android Debug Bridge) — comes with Android Studio or download [platform-tools](https://developer.android.com/tools/releases/platform-tools) separately

Verify your setup is correct by running:
```powershell
flutter doctor
```
All items should show ✅ before proceeding.

---

### Option 1: Run in Chrome (Quickest — no phone needed)
Good for fast UI testing.
```powershell
flutter run -d chrome
```
Flutter starts a local web server and opens the dashboard in Chrome automatically.

---

### Option 2: Run on Android via USB (Most Reliable)
Plug your phone in with a USB cable, enable USB debugging, then:
```powershell
flutter run
```
Your phone will appear in the device list — select it.

---

### Option 3: Run on Android Wirelessly (More Steps)

> ⚠️ **Important:** Every phone gets a different IP address and port. Do NOT copy the IP/port from someone else's setup — you must find your own.

**Step 1 — Enable Wireless Debugging on your phone:**
1. Go to **Settings → About Phone** and tap **Build Number** 7 times to unlock Developer Options
2. Go to **Settings → Developer Options → Wireless Debugging** and turn it on
3. Note the **IP address and port** shown on that screen (e.g. `192.168.x.x:XXXXX`) — this is YOUR connect port
4. Tap **"Pair device with pairing code"** — note the separate pairing IP:port and 6-digit code

**Step 2 — Add ADB to your PATH (one-time setup):**

Find where your `adb.exe` is (usually inside Android SDK platform-tools), then run:
```powershell
$env:PATH = "C:\path\to\platform-tools;" + $env:PATH
```
Replace `C:\path\to\platform-tools` with your actual path.

**Step 3 — Pair your phone (one-time per session):**
```powershell
adb pair <PAIRING-IP:PAIRING-PORT> <6-DIGIT-CODE>
```
Example (your numbers will be different):
```powershell
adb pair 192.168.0.206:37363 652937
```

**Step 4 — Connect to your phone:**

Use the IP and port from the **main Wireless Debugging screen** (not the pairing screen):
```powershell
adb connect <YOUR-IP:YOUR-PORT>
```
Example (your numbers will be different):
```powershell
adb connect 192.168.0.206:35279
```

**Step 5 — Run the app:**
```powershell
flutter run
```
Your phone should now appear in the device list. Select it.

**If your phone doesn't appear in the Flutter device list:**
```powershell
adb kill-server
adb start-server
flutter run
```

---

### Option 4: Run on Windows Desktop
```powershell
flutter run -d windows
```

---

## 📦 Dependencies

| Package | Purpose |
|---|---|
| `mobile_scanner` | Phone camera barcode scanning (backup scanner) |
| `openfoodfacts` | Looks up product info from a scanned barcode |

Install them by running:
```powershell
flutter pub get
```

---

## 🌳 Branch Structure

| Branch | Contents |
|---|---|
| `feature/mobile-app-dashboard` | This Flutter mobile/web UI |
| `main` | Original Raspberry Pi Python/C++ sensor code and legacy web app |

---

## 🔮 What's Coming Next

- [ ] Connect app to Raspberry Pi over local network (HTTP polling or WebSocket)
- [ ] Replace hardcoded fridge vitals with real sensor data from Pi
- [ ] Inventory sync between Pi scanner and mobile app
- [ ] Expiry date notifications
- [ ] Shopping list feature (if item removed from fridge, add to list)