# web_app — Web Interface Module

Real-time browser dashboard for PiFridge, served over nginx via FastCGI.

---

## Overview

This module provides the complete web-facing layer of PiFridge. It consists of two FastCGI executables and a single-page HTML frontend:

- **`pifridge_api`** — serves live sensor readings (temperature, humidity, pressure, door state, lux) to the browser by reading a JSON file written by `main.cpp`
- **`pifridge_inventory`** — manages the fridge inventory via a SQLite database, handling add, increment, decrement, and delete operations
- **`index.html`** — single-page dashboard that polls both endpoints and renders the UI in the browser

nginx acts as the reverse proxy, routing `/api/fridge` to `pifridge_api` and `/api/inventory` to `pifridge_inventory` via Unix sockets. The nginx configuration lives in `config/pifridge.conf` and is documented in the main project README.

---

## Files

| File | Purpose |
|---|---|
| `pifridge_api.cpp` | FastCGI endpoint — serves sensor data from `/tmp/fridge_data.json` |
| `pifridge_inventory.cpp` | FastCGI endpoint — SQLite-backed inventory CRUD |
| `index.html` | Single-page browser dashboard |
| `CMakeLists.txt` | Builds `pifridge_api` and `pifridge_inventory` executables |
| `test/` | Unit tests (see [Testing](#testing)) |

---

## Architecture

```
Browser (index.html)
    │
    │  GET /api/fridge        (polls every 1 s)
    │  GET/POST /api/inventory (polls every 2 s)
    ▼
nginx (reverse proxy)
    │
    ├── /api/fridge      → Unix socket → pifridge_api
    │                                        └── reads /tmp/fridge_data.json
    │                                                  ▲
    │                                            main.cpp writes this
    │                                            on each sensor callback
    │
    └── /api/inventory   → Unix socket → pifridge_inventory
                                             └── SQLite /var/lib/pifridge/inventory.db
```

### Why a shared JSON file for sensor data?
`pifridge_api` reads `/tmp/fridge_data.json` rather than querying sensors directly. This keeps the FastCGI process stateless and decoupled from the sensor thread — `main.cpp` owns the sensors and writes state on each callback, while `pifridge_api` only reads. Each HTTP request gets a fresh read with no shared mutable state between processes.

---

## API Reference

### `GET /api/fridge`
Returns the latest sensor readings as JSON.

```json
{
  "temperature": 4.2,
  "humidity": 68.1,
  "pressure": 1013.0,
  "lux": 12.4,
  "door_open": false
}
```

### `GET /api/inventory`
Returns all inventory items ordered by date added (newest first).

```json
[
  {
    "id": 1,
    "name": "Milk",
    "barcode": "5000112548167",
    "quantity": 2,
    "date_added": "2025-04-01"
  }
]
```

### `POST /api/inventory`
Adds a new item. Request body:

```json
{ "name": "Eggs", "barcode": "optional", "quantity": 6 }
```

### `POST /api/inventory/increment`
Increments quantity by 1. Request body: `{ "id": 1 }`

### `POST /api/inventory/decrement`
Decrements quantity by 1. Deletes the row automatically when quantity reaches 0. Request body: `{ "id": 1 }`

### `POST /api/inventory/delete`
Deletes an item by id. Request body: `{ "id": 1 }`

---

## Database

`pifridge_inventory` uses SQLite at `/var/lib/pifridge/inventory.db`. The table is created automatically on first run:

```sql
CREATE TABLE IF NOT EXISTS inventory (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  name       TEXT    NOT NULL,
  barcode    TEXT,
  quantity   INTEGER NOT NULL DEFAULT 1,
  date_added TEXT    NOT NULL
);
```

No migration tooling is required — the schema is stable and created idempotently on each startup.

---

## Frontend

`index.html` is a self-contained single-page app with no external JavaScript dependencies. Key behaviours:

- Polls `/api/fridge` every **1 second** and updates temperature, humidity, pressure, door state, and lux in place
- Polls `/api/inventory` every **2 seconds**, re-rendering only when the response changes (diffed via `JSON.stringify`)
- Door state drives a live colour indicator: green (closed) / red (open)
- Items can be added via the form or adjusted with `+`/`−` buttons
- Required field validation with inline error feedback
- Responsive layout down to 400px wide (mobile-friendly for checking the fridge on your phone)

---

## Building

Dependencies:

```bash
sudo apt install libfcgi-dev libsqlite3-dev nginx
```

Build from the project root:

```bash
cmake .
make pifridge_api
make pifridge_inventory
```

Or build everything:

```bash
cmake .
make
```

---

## Running

Create the required runtime directories:

```bash
sudo mkdir -p /var/run/pifridge /var/lib/pifridge
sudo chown $USER /var/run/pifridge /var/lib/pifridge
```

Start both FastCGI processes:

```bash
sudo ./build/src/web_app/pifridge_api &
./build/src/web_app/pifridge_inventory &
```

Then start nginx (see `config/pifridge.conf` README for the full nginx setup).

Open a browser and navigate to `http://<raspberry-pi-ip>/`.

---

## Latency

| Event | Measured latency |
|---|---|
| Sensor callback → `/tmp/fridge_data.json` written | [X µs] |
| Browser poll → JSON delivered via nginx + FastCGI | [X ms] |
| End-to-end: sensor reading → visible in browser | ~[X ms] (dominated by 1 s poll interval) |

> **TODO:** Fill in with measured values from the running system.

---

## Testing

> **TODO:** Unit tests to be written. See `test/` directory.

Planned test cases:
- `getAllItems` returns correct JSON for a known database state
- `addItem` inserts a row and returns success
- `decrementItem` deletes the row when quantity is 1
- `incrementItem` increases quantity correctly
- `extractJsonString` correctly parses string and integer fields
- `pifridge_api` returns `{"error": "data not available yet"}` when the JSON file does not exist

---

## Dependencies

| Library | Purpose |
|---|---|
| `libfcgi` (`fcgiapp.h`) | FastCGI protocol — communication between nginx and C++ processes |
| `libsqlite3` | Embedded database for inventory persistence |
| `nginx` | Reverse proxy — serves `index.html` and routes API requests to FastCGI sockets |

---

## Author

**David Mead** — `pifridge_api.cpp`, `pifridge_inventory.cpp`, `index.html`, CMake build configuration.
**Patrick Dawodu** — `index.html`

---

## Acknowledgements

FastCGI integration pattern follows Dr. Bernd Porr's [*Realtime Embedded Coding in C++ under Linux*](https://berndporr.github.io/realtime_cpp_coding/).