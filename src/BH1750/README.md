# BH1750 Light Sensor Module

Real-time door-state detection from ambient light levels for PiFridge, built on the BH1750 digital light sensor.

## Overview

This module provides a callback-based, event-driven path for BH1750 light sensing and door-state detection.

It uses a three-part design:

- **`ILightSensor`** - abstract callback-based sensor interface. Exposes callback registration plus lifecycle control with `start()` and `stop()`.
- **`Bh1750Sensor`** - low-level BH1750 I2C reader and threaded event source. Owns the file descriptor, worker thread, `timerfd`, `eventfd`, and callback emission.
- **`DoorLightController`** - threshold and hysteresis logic. Consumes lux samples and emits door state changes only when the state actually changes.

This separation follows the course feedback closely: sensor reads are no longer exposed through a public getter-based polling API, sampling is driven by blocking I/O wakeup, and door state is propagated through callbacks instead of a polling loop.

## Files

| File | Purpose |
|------|---------|
| `include/ILightSensor.hpp` | Public callback-based interface for light sensor modules |
| `ILightSensor.cpp` | Out-of-line implementation for the interface destructor |
| `include/Bh1750Sensor.hpp` | Public BH1750 sensor class definition and lifecycle API |
| `Bh1750Sensor.cpp` | BH1750 I2C implementation, worker thread, `timerfd`, `eventfd`, and callback emission |
| `include/DoorLightController.hpp` | Public door-state controller API and callback type |
| `DoorLightController.cpp` | Threshold and hysteresis logic for door open and closed state changes |
| `CMakeLists.txt` | Builds `bh1750_logic`, `bh1750`, the BH1750 unit test target, and the BH1750 demo target |
| `test/DoorLightControllerTest.cpp` | Unit test covering threshold, hysteresis, callback firing, and callback-chain behavior |
| `test/BH1750Demo.cpp` | Minimal hardware demo that prints lux readings and door-state transitions on a Raspberry Pi |

## Core Interfaces

### `ILightSensor::LightLevelCallback`
Receives a new lux sample whenever the BH1750 worker thread publishes one.

```cpp
using LightLevelCallback = std::function<void(double)>;
```

### `DoorLightController::DoorStateCallback`
Receives a door state change event only when the state changes.

```cpp
using DoorStateCallback = std::function<void(bool, double)>;
```

Parameters:
- `bool` - `true` when open, `false` when closed
- `double` - lux value that triggered the state change

## Configuration and Settings

### `Bh1750Sensor`
The sensor is currently used with:
- I2C device path: `/dev/i2c-1`
- BH1750 address: `0x23`
- configurable sampling interval via `start(int intervalMs)`

### `DoorLightController`
The controller applies hysteresis using two thresholds:
- **open threshold** - lux value at or above which the door is considered open
- **close threshold** - lux value at or below which the door is considered closed

Validated example values:
- `openThresholdLux = 30.0`
- `closeThresholdLux = 10.0`

This avoids repeated toggling while lux remains between the two thresholds.

## Usage

```cpp
#include "Bh1750Sensor.hpp"
#include "DoorLightController.hpp"

Bh1750Sensor sensor("/dev/i2c-1", 0x23);
DoorLightController controller(30.0, 10.0);

controller.registerDoorStateCallback([](bool isOpen, double lux) {
    if (isOpen) {
        std::cout << "door=open lux=" << lux << "\n";
    } else {
        std::cout << "door=closed lux=" << lux << "\n";
    }
});

sensor.registerCallback([&](double lux) {
    controller.hasLightSample(lux);
});

sensor.start(500);

// ... application runs ...

sensor.stop();
```

## Real-Time Design

### Threading model
`Bh1750Sensor` creates one worker thread on `start(intervalMs)`. The worker blocks in `poll()` on two file descriptors:

- a `timerfd` for periodic sampling
- an `eventfd` for shutdown

When the timer expires, the thread wakes, reads one lux sample from the BH1750, and forwards it through the registered callback.

```text
poll()
  -> timerfd readable
       -> readLuxOnce()
            -> LightLevelCallback(lux)
                 -> DoorLightController.hasLightSample(lux)
                      -> DoorStateCallback(open_or_closed, lux)
```

### Shutdown
`stop()` writes to the `eventfd`, which wakes the blocked worker immediately. The worker exits cleanly and `stop()` joins the thread before returning.

### Why `timerfd` and `poll()`?
This module intentionally uses blocking I/O primitives rather than `sleep()` loops. That is closer to the course requirement for waking threads via blocking I/O and avoids a visible polling loop in application logic.

## Door-State Logic

The module uses hysteresis to avoid rapid flicker between open and closed states.

Example:
- open threshold = `30.0`
- close threshold = `10.0`

This means the door does not repeatedly toggle while lux stays between those two thresholds.

## Validation

Validated locally with:
- CMake configure
- CMake build
- CTest unit test

Validated on Raspberry Pi with live hardware:
- BH1750 detected on I2C bus 1 at address `0x23`
- callback-based lux sampling worked on the Pi
- door open and closed transitions were observed from live readings

Observed Pi output included:

```text
door=open lux=425
door=closed lux=0.833333
door=open lux=69.1667
door=closed lux=0.833333
door=open lux=31.6667
door=closed lux=4.16667
```

This confirmed:
- live BH1750 reads worked on hardware
- thresholds were applied correctly
- callback-based event handling produced door open and closed transitions on the Pi

## Latency Note

Two BH1750 timing contexts were used during development:

- **Module-level Raspberry Pi validation** used a `500 ms` sampling interval to confirm stable callback-based lux sampling and correct door open/closed transitions on hardware.
- **Integrated PiFridge runtime configuration** in `main.cpp` uses a `200 ms` sampling interval for the live system.

Because door-state evaluation happens once on each timer wakeup, the response bound is one configured sample interval plus normal Linux scheduling overhead. For fridge door detection this is acceptable because the event is human-scale rather than microsecond-critical.

In the integrated system, the `200 ms` sampling interval was chosen to reduce visible delay in door-state detection while still keeping the BH1750 path simple, callback-based, and event-driven.

## Dependencies

### Build
- CMake
- C++17 compiler
- `build-essential`
- `pkg-config`
- `libi2c-dev`
- `i2c-tools`

### Install dependencies

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config libi2c-dev i2c-tools
```

## Building

### Build only the BH1750 module
From the project root:

```bash
cmake -S src/BH1750 -B src/BH1750/build
cmake --build src/BH1750/build
```

### Run the BH1750 unit test

```bash
ctest --test-dir src/BH1750/build --output-on-failure
```

### Run the BH1750 demo on Raspberry Pi hardware

```bash
./src/BH1750/build/bh1750_demo
```

This demo:
- opens the BH1750 on `/dev/i2c-1`
- prints live lux readings
- prints door open and door closed transitions using the configured thresholds
- is intended for Raspberry Pi hardware validation, not unit testing

### Build the full project
From the project root:

```bash
cmake -B build
cmake --build build
```

## Hardware Assumptions

- I2C device path: `/dev/i2c-1`
- PiFridge currently uses BH1750 address `0x23`
- BH1750 continuous high-resolution mode is used

Typical wiring:
- VCC to 3.3V
- GND to GND
- SDA to GPIO2 / SDA1
- SCL to GPIO3 / SCL1

Confirm the sensor is visible:

```bash
sudo i2cdetect -y 1
```

## Testing

The BH1750 module test currently covers:
- `DoorLightController` threshold and hysteresis behavior
- callback firing only on real state changes
- callback-chain propagation using a minimal fake `ILightSensor`

This test is integrated through CMake and exposed through CTest. It verifies controller logic and callback-chain behavior without requiring physical hardware.

Direct BH1750 device access, live lux readings, and real sensor-triggered door transitions are verified separately on Raspberry Pi hardware using the `bh1750_demo` executable.

The separate `bh1750_demo` executable is used for live Raspberry Pi hardware checks of:
- BH1750 I2C access
- lux output on hardware
- door open and closed transitions from real sensor readings

## Latency Timings

Two timing contexts were measured during development:

1. **Module-level BH1750 validation on Raspberry Pi**
   - sampling interval: `500 ms`
   - purpose: confirm stable callback-based lux sampling and correct door open/closed transitions on hardware

2. **Integrated PiFridge runtime**
   - sampling interval: `200 ms`
   - purpose: reduce visible delay in live door-state detection within the full system

### Measured timings

| Context | Event | Run 1 | Run 2 | Run 3 | Mean |
|---------|-------|-------|-------|-------|------|
| Integrated PiFridge runtime | Light change → console | 341μs | 340μs | 337μs | **339μs** |
| Integrated PiFridge runtime | Light change → webapp | 635ms | 641ms | 638ms | **638ms** |

### Design interpretation

For the integrated PiFridge runtime, the BH1750 path uses a `200 ms` sampling interval. This means door-state response is bounded by one timer interval plus normal Linux scheduling overhead. The measured console latency is well below the configured interval, while the larger webapp latency is dominated by the wider system path rather than the BH1750 callback chain alone.

For fridge-door detection, this latency is acceptable because the event is human-scale and the design goal is responsive state detection rather than microsecond-critical control.

## Author and Responsibility

**Hamna Khalid**

This BH1750 module refactor and validation work includes:
- callback-based light sensor interface
- event-driven sampling thread
- door-state threshold and hysteresis logic
- CMake test restoration, strong BH1750 test coverage, and demo target wiring for the BH1750 module
- Raspberry Pi validation of the BH1750 sensor path
- BH1750 module documentation update

**Ross Cameron**
- Latency Timings

## Related History and Links

- [PR #25](https://github.com/DavidMead182/PiFridge/pull/25) Initial BH1750 light sensor work and Raspberry Pi hardware integration
- [PR #35](https://github.com/DavidMead182/PiFridge/pull/35) Callback-based event-driven BH1750 refactor after feedback
- [PR #45](https://github.com/DavidMead182/PiFridge/pull/45) Cleanup and integration follow-up for BH1750 testing and documentation

## I2C Address

PiFridge currently uses the BH1750 at address `0x23`, which was confirmed during Raspberry Pi validation with `i2cdetect`.

The BH1750 supports two I2C slave addresses:
- default / low: `0x23`
- high: `0x5c`

## Acknowledgements

Threading, callback, and blocking I/O patterns were shaped by Dr. Bernd Porr's [*Realtime Embedded Coding in C++ under Linux*](https://berndporr.github.io/realtime_cpp_coding/) and by the course feedback on event-driven design. Software engineering structure, testing, and documentation practices were also informed by lectures from Dr. Chongfeng Wei.