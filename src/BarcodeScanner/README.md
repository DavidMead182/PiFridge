# BarcodeScanner — Barcode Scanning Module

Real-time barcode scanning over serial UART with automatic product lookup and inventory persistence.

## Overview

This module provides two responsibilities:

- **`BarcodeScanner`** — event-driven serial reader for the Waveshare barcode scanner module. Owns a dedicated thread that blocks on `select()` waiting for data from the UART port. Fires a `std::function` callback when a complete barcode is received.
- **`fetch_product`** — queries the [Open Food Facts API](https://world.openfoodfacts.net) with a scanned barcode, extracts the product name, and upserts the item into the SQLite inventory database.

The scanner is **demand-driven** — it is armed (`triggerScan`) when the fridge door opens and disarmed (`stopScan`) when the door closes. This avoids unnecessary scanning and power consumption when the fridge is closed.

## Files

| File | Purpose |
|---|---|
| `BarcodeScanner.hpp` | Class declaration and `fetch_product` signature |
| `BarcodeScanner.cpp` | Serial port handling, barcode parsing, Open Food Facts API integration, SQLite upsert |
| `CMakeLists.txt` | Builds the `barcode_scanner` static library |
| `test/` | Unit tests (see [Testing](#testing)) |

## Usage

```cpp
#include "BarcodeScanner.hpp"

BarcodeScanner scanner("/dev/ttyAMA0");

scanner.registerCallback([](const std::string& barcode) {
    std::cout << "Scanned: " << barcode << "\n";
    fetch_product(barcode); // Look up and add to inventory
});

scanner.start();

// Arm when door opens
scanner.triggerScan();

// Disarm when door closes
scanner.stopScan();

// Clean shutdown
scanner.stop();
```

## Real-Time Design

### Threading model
`BarcodeScanner` spawns a single worker thread on `start()`. The thread blocks on `select()` monitoring two file descriptors — the serial port (`fd`) and a wake pipe. This avoids polling entirely:

```
select() blocks on fd + wake_pipe_[0]
    │
    ├── Serial data arrives
    │       └── read() → parse → callback fired
    │
    └── wake_pipe_[1] written (on stop())
            └── thread exits cleanly
```

### Shutdown
`stop()` writes a single byte to `wake_pipe_[1]`, which unblocks the `select()` immediately. The thread detects data on `wake_pipe_[0]` and exits. `stop()` then calls `thread_.join()` before closing file descriptors, ensuring no use-after-free.

### Barcode parsing
The Waveshare scanner sends data as raw bytes over UART at 9600 baud. The parser accumulates chunks into a buffer, filtering out status response bytes (non-digit characters) and discarding chunks that exceed `MAX_BARCODE_LEN = 32`. A complete barcode is signalled by a trailing `\r` or `\n`.

### Scanner arming
The scanner hardware is controlled by sending 9-byte command sequences over the serial port:

- `triggerScan()` — sends the hardware trigger command and flushes the RX buffer
- `stopScan()` — sends the hardware stop command

## Open Food Facts Integration

`fetch_product` makes an HTTPS GET request to:

```
https://world.openfoodfacts.net/api/v2/product/{barcode}?fields=product_name
```

On a successful response, the product name is extracted and written to the SQLite inventory database at `/var/lib/pifridge/inventory.db` via `upsertItem`:

- If the barcode already exists in the database → quantity is incremented by 1
- If the barcode is new → a new row is inserted with `quantity = 1` and today's date

Products not found in the Open Food Facts database are silently skipped with a log message.

## Latency Timings

### Console
| Run | Time |
|-----|------|
| 1 | 1134ms |
| 2 | 1178ms |
| 3 | 1151ms |
| **Mean** | **1154ms** |

### Webapp
| Run | Time |
|-----|------|
| 1 | 2241ms |
| 2 | 2259ms |
| 3 | 2244ms |
| **Mean** | **2248ms** |

## Serial Port Configuration

| Parameter | Value |
|---|---|
| Port | `/dev/ttyAMA0` |
| Baud rate | 9600 |
| Data bits | 8 |
| Stop bits | 1 |
| Parity | None |
| Flow control | None |

---

## Dependencies

| Library | Purpose |
|---|---|
| `libcurl` | HTTPS requests to Open Food Facts API |
| `libsqlite3` | Inventory database persistence |
| POSIX `termios` | Serial port configuration |
| POSIX `select` | Blocking I/O multiplexing on serial port + wake pipe |

```bash
sudo apt install libcurl4-openssl-dev libsqlite3-dev
```

---

## Building

```bash
cmake .
make barcode_scanner
```

---

## Testing

The module is designed for testability — `fetch_product` and `upsertItem` are independently callable, and the serial port is injected as a path string so tests can substitute a virtual port or a named pipe.

Planned test cases:
- `extractJsonString` correctly parses product name from a known API response
- `upsertItem` increments quantity when barcode already exists
- `upsertItem` inserts a new row when barcode is not present
- `fetch_product` skips gracefully when API returns `"status": 0`
- `BarcodeScanner` callback fires with correct barcode string for known byte sequences
- `stop()` joins thread cleanly without hanging

To run tests once written:

```bash
make test
```

---

## Author

**Ross Cameron** — `BarcodeScanner` class, serial port handling, barcode parsing, Open Food Facts API integration, SQLite upsert logic, latency timings, and CMake build configuration.

---

## Acknowledgements

Serial UART and `select()`-based blocking I/O patterns follow Dr. Bernd Porr's [*Realtime Embedded Coding in C++ under Linux*](https://berndporr.github.io/realtime_cpp_coding/).
