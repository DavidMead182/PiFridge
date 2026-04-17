# Main    

This main.cpp Wires all sensor modules together, manages shared state, and drives the web interface via JSON handoff.



## Overview

`main.cpp` is the integration layer of PiFridge. It does not implement any sensor logic itself — instead it:

1. Constructs each sensor module (`BME680Sensor`, `Bh1750Sensor`, `BarcodeScanner`, `Camera`)
2. Registers callbacks on each module to update shared `FridgeState`
3. Writes `FridgeState` to `/tmp/fridge_data.json` on every state change, which `pifridge_api` reads to serve the live dashboard
4. Coordinates door-open/door-closed events to arm and disarm the barcode scanner and camera
5. Handles `SIGINT`/`SIGTERM` for clean shutdown — all sensor threads are stopped and joined before exit



## File Structure

```
src/
├── BarcodeScanner/         # Barcode scanner module (Ross Cameron)
├── BH1750/                 # BH1750 light sensor module (Hamna Khalid)
├── BME680/                 # BME680 environmental sensor module (David Mead)
├── Camera/                 # Camera & object detection module (Ryan Ho)
├── common/                 # Shared I2C abstraction layer (David Mead)
├── web_app/                # FastCGI endpoints & frontend dashboard (David Mead, Patrick Dawodu)
├── CMakeLists.txt          # Top-level build — links all modules into pifridge executable
└── main.cpp                # Application entry point and integration layer
```

Each subdirectory has its own `README.md` documenting its classes, design decisions, and usage.



## Files

| File | Purpose |
|------|---------|
| `main.cpp` | Application entry point and integration layer |
| `CMakeLists.txt` | Builds the `pifridge` executable and links all modules |



## Architecture

```
main.cpp
  │
  ├── BME680Sensor ──── callback ──► update FridgeState.vitals
  │                                  └── saveStateToJson()
  │
  ├── Bh1750Sensor ─── callback ──► DoorLightController.hasLightSample()
  │                                        │
  │                              door state change callback
  │                                        │
  │                         ┌─────────────┴──────────────┐
  │                     Door open                     Door closed
  │                         │                             │
  │                  scanner.triggerScan()        scanner.stopScan()
  │                  camera.triggerCaptureNow()
  │                  saveStateToJson()
  │
  ├── BarcodeScanner ─ callback ──► fetch_product(barcode)
  │                                  └── upserts inventory DB
  │
  └── Camera ────────── callback ──► addCameraItemToInventory(label)
                                      └── upserts inventory DB
```

### JSON handoff (`saveStateToJson`)
Rather than coupling `pifridge_api` directly to the sensor threads, `main.cpp` writes `/tmp/fridge_data.json` atomically whenever vitals or door state change. `pifridge_api` reads this file on each HTTP request. This keeps the FastCGI process stateless and avoids any shared memory between processes.

`saveStateToJson` is called in two places:
- Inside the BME680 callback — on every 5-second vitals update
- Inside the door state change callback — immediately when the door opens or closes

### Door-driven event coordination
The `DoorLightController` receives raw lux readings from `Bh1750Sensor` and fires a door state callback **only when state changes** (open → closed or closed → open). This avoids redundant events at a 200 ms light sampling rate. On door open, the barcode scanner is armed and the camera takes an immediate capture. On door close, the scanner is disarmed.

### Camera inventory integration
When the camera detects an object above the confidence threshold (0.7), `addCameraItemToInventory` upserts the item into the SQLite inventory database — incrementing quantity if the item already exists, or inserting a new row if not. This runs directly in the camera callback on the camera's worker thread, protected by SQLite's own serialisation.



## Shared State

```cpp
struct FridgeState {
    BME680Sample vitals{};   // Latest temperature, humidity, pressure, gas
    bool         door_open;  // Current door state
    double       lux;        // Latest lux reading
    std::mutex   mutex;      // Guards all fields above
};
```

All callbacks that write to `FridgeState` acquire `state.mutex` via `std::lock_guard` before modifying any field. `saveStateToJson` is always called inside the lock so the JSON file is never written with a partially updated state.



## Sensor Configuration

| Sensor | Bus | Address | Interval |
|--------|-----|---------|----------|
| BME680 | I2C bus 1 | `0x76` | 5000 ms |
| BH1750 | `/dev/i2c-1` | `0x23` | 200 ms |
| BarcodeScanner | `/dev/ttyAMA0` | — | Event-driven |
| Camera | — | — | 200 ms (door open or manual trigger) |

### BME680 settings

| Parameter | Value |
|-----------|-------|
| Temperature oversampling | ×8 (`osrs_t = 4`) |
| Pressure oversampling | ×4 (`osrs_p = 3`) |
| Humidity oversampling | ×2 (`osrs_h = 2`) |
| IIR filter | 2 |
| Gas heater target | 320 °C |
| Gas heater on-time | 150 ms |



## Signal Handling

`SIGINT` (Ctrl+C) and `SIGTERM` both set `g_quit = true`, breaking the main loop. Shutdown calls `stop()` on every module in order, which joins each worker thread before returning. This ensures no sensor threads are left running after the process exits.

```
SIGINT / SIGTERM
    └── g_quit = true
          └── main loop exits
                └── lightSensor.stop()  — joins BH1750 thread
                └── bme680.stop()       — joins BME680 thread
                └── scanner.stop()      — joins barcode thread
                └── camera.stop()       — joins camera thread
```



## Building

From the project root:

```bash
cmake -B build
cmake --build build
```

### Dependencies

```bash
sudo apt install libsqlite3-dev libcurl4-openssl-dev cmake build-essential
```

All sensor module libraries (`bme680`, `bh1750`, `barcode_scanner`, `camera`, `linux_i2c`) are built as part of the same CMake project via `add_subdirectory`.



## Running

```bash
sudo ./build/src/pifridge
```

`sudo` is required for GPIO and I2C access on the Raspberry Pi.

Before running, ensure the FastCGI processes and nginx are started (see `run.sh` or the `web_app` and `config` READMEs).



## Authors & Contributions

| Name | Contribution |
|------|--------------|
| **David Mead** | Integration of BME680Sensor, BH1750/DoorLightController, and BarcodeScanner into `main.cpp`; `saveStateToJson` JSON handoff; camera object detection → inventory DB wiring; CMakeLists.txt (shared) |
| **Ross Cameron** | BarcodeScanner module; CMakeLists.txt (shared) |
| **Hamna Khalid** | `BH1750` sensor module; callback-based event-driven refactor of the light sensor path; `DoorLightController` logic; CMake test restoration for the BH1750 module, CMakeLists.txt; Raspberry Pi validation of the BH1750 sensor path |
| **Ryan Ho** | Camera detection module; Camera integration into `main.cpp`; CMakeLists.txt (shared) |



## Acknowledgements

Threading, callback, and signal-handling patterns follow Dr. Bernd Porr's [*Realtime Embedded Coding in C++ under Linux*](https://berndporr.github.io/realtime_cpp_coding/). Software engineering practices follow lectures by Dr. Chongfeng Wei.
