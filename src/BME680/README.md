# BME680 Sensor Module
 
Real-time temperature, pressure, humidity, and air quality (VOC) sensing for PiFridge, built on the Bosch BME680.
 
## Overview
 
This module provides a two-class design for the BME680 environmental sensor:
 
- **`BME680`** — low-level register driver. Handles chip initialisation, calibration, forced-mode measurements, and Bosch compensation formulas. Has no knowledge of threads, timers, or the rest of the application.
- **`BME680Sensor`** — event-driven wrapper. Owns a dedicated thread driven by `timerfd` blocking I/O. Fires a `std::function` callback with a fully compensated `BME680Sample` at each configured interval.
 
This separation follows the Single Responsibility Principle: the driver speaks the sensor protocol, the wrapper owns the timing and thread lifecycle, and the callback subscriber decides what to do with the data.
 

## Files
 
| File | Purpose |
|------|---------|
| `BME680.hpp` / `BME680.cpp` | Low-level register driver and Bosch compensation formulas |
| `BME680Sensor.hpp` / `BME680Sensor.cpp` | Threaded, callback-based event wrapper |
| `CMakeLists.txt` | Builds the `bme680` static library |
| `test/` | Unit tests (see [Testing](#testing)) |
 
 
## Data Types
 
### `BME680Sample`
A single fully-compensated reading returned on every callback. All values are in physical units — no raw ADC values are exposed.
 
| Field | Type | Unit |
|-------|------|------|
| `temperature_c` | `float` | °C |
| `pressure_hpa` | `float` | hPa |
| `humidity_rh` | `float` | % RH |
| `gas_ohms` | `uint32_t` | Ω (air quality proxy) |
| `timestamp` | `steady_clock::time_point` | Monotonic clock |
 
### `BME680Settings`
Configuration passed at construction. Safe defaults are provided so the sensor works out of the box.
 
| Field | Default | Description |
|-------|---------|-------------|
| `osrs_t` | 4 | Temperature oversampling (×8) |
| `osrs_p` | 3 | Pressure oversampling (×4) |
| `osrs_h` | 2 | Humidity oversampling (×2) |
| `filter` | 2 | IIR filter coefficient |
| `enable_gas` | `true` | Enable VOC/gas resistance measurement |
| `heater_temp_c` | 320 | Gas heater target temperature (°C) |
| `heater_time_ms` | 150 | Gas heater on-time (ms) |
| `ambient_temp_c` | 25 | Ambient temp used in heater resistance calculation |
 
 
## Usage
 
```cpp
#include "BME680Sensor.hpp"
 
// Create sensor on I2C bus 1, address 0x77, sampling every 5 seconds
BME680Sensor sensor(1, 0x77, std::chrono::milliseconds(5000));
 
// Register callback — fired on every new sample
sensor.registerCallback([](const BME680Sample& s) {
    std::cout << "Temp:     " << s.temperature_c << " °C\n";
    std::cout << "Pressure: " << s.pressure_hpa  << " hPa\n";
    std::cout << "Humidity: " << s.humidity_rh   << " %RH\n";
    std::cout << "Gas:      " << s.gas_ohms      << " Ω\n";
});
 
sensor.start();
 
// ... application runs ...
 
sensor.stop(); // Joins thread cleanly before destruction
```
 
## Real-Time Design
 
### Threading model
`BME680Sensor` spawns a single worker thread on `start()`. The thread blocks on a `timerfd` file descriptor (`CLOCK_MONOTONIC`) rather than `sleep()` or polling. This ensures the sampling interval is decoupled from measurement time and avoids drift.
 
```
timerfd expires
    └─ read() unblocks worker thread
         └─ BME680::readSample() called
              └─ Callback fired with BME680Sample
```
 
### Shutdown
`stop()` disarms the `timerfd` by setting a 1 ns one-shot expiry, which unblocks the blocking `read()` immediately. The worker checks `running_` after each wakeup and exits cleanly. `stop()` then calls `worker_.join()` before returning, guaranteeing no use-after-free.
 
### Why `timerfd` over `sleep()`?
`std::this_thread::sleep_for()` introduces drift because measurement time is not accounted for. `timerfd` with `CLOCK_MONOTONIC` fires at absolute wall-clock intervals regardless of how long each measurement takes, giving consistent sample timing.
 
 
## Latency
 
| Event | Measured latency |
|||
| `timerfd` expiry → callback fired | [X–X µs] |
| BME680 forced-mode measurement | ~[X] ms (dominated by heater warm-up at 150 ms) |
| I2C burst read (17 bytes) | [X µs] |
| End-to-end: timer expiry → sample in callback | ~[X] ms |
 
> **TODO:** Fill in with measured values from the running system.
 
 
## Dependencies
 
### Runtime
- `libgpiod` — not used directly by this module, but required by `linux_i2c`
- Linux `timerfd` — kernel 2.6.25+, available on all Raspberry Pi OS versions
 
### Build
- CMake ≥ 3.10
- `linux_i2c` — built from `src/common/`, provides `LinuxI2CDevice`
- `II2CDevice` interface — from `include/II2CDevice.hpp`
 
### Install dependencies
```bash
sudo apt install libgpiod-dev cmake build-essential
```
 

 
## Building
 
From the project root:
 
```bash
cmake .
make bme680
```
 
Or build the full project:
 
```bash
cmake .
make
```
 
The `bme680` static library is linked into the main executable automatically via the top-level `CMakeLists.txt`.
 

 
## Testing
 
> **TODO:** Unit tests to be written. See `test/` directory.

## Author
 
**David Mead** — BME680 register driver (`BME680.cpp/.hpp`), Bosch compensation formulas, `BME680Sensor` threaded wrapper (`BME680Sensor.cpp/.hpp`), and CMake build configuration.
 
## I2C Address
 
The BME680 supports two I2C addresses depending on the state of the `SDO` pin:
 
| `SDO` pin | Address |
|||
| GND | `0x76` |
| VCC | `0x77` (default for PiFridge) |
 

 
## Acknowledgements
 
Compensation formulas follow the official Bosch BME680 datasheet and are adapted from the [Bosch Sensortec BME68x API](https://github.com/boschsensortec/BME68x_SensorAPI). Threading and `timerfd` patterns follow Dr. Bernd Porr's [*Realtime Embedded Coding in C++ under Linux*](https://berndporr.github.io/realtime_cpp_coding/).
