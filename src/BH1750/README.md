# BH1750 light sensor module

## Purpose

This module reads BH1750 lux data on the Raspberry Pi and publishes light samples through callbacks. Door open or closed state is derived from lux thresholds with hysteresis inside `DoorLightController`.

This is the event-driven refactor requested in course feedback:
- no public getter-based polling API
- sensor samples are pushed through callbacks
- sensor work runs in its own thread
- wakeup uses blocking I/O primitives
- unit test is integrated through CMake

## Files

- `include/ILightSensor.hpp`
- `include/Bh1750Sensor.hpp`
- `include/DoorLightController.hpp`
- `ILightSensor.cpp`
- `Bh1750Sensor.cpp`
- `DoorLightController.cpp`
- `test/DoorLightControllerTest.cpp`

## Design

### `ILightSensor`
Defines the callback-based interface:
- `registerCallback(...)`
- `start(int intervalMs)`
- `stop()`

### `Bh1750Sensor`
- owns the BH1750 I2C file descriptor
- runs a worker thread
- uses `timerfd`, `eventfd`, and `poll()` for blocking wakeup
- emits lux values through `ILightSensor::LightLevelCallback`

### `DoorLightController`
- receives lux samples through `hasLightSample(double lux)`
- applies open and close thresholds with hysteresis
- emits door state changes through `DoorStateCallback`

## Build and test

```bash
cmake -S src/BH1750 -B src/BH1750/build
cmake --build src/BH1750/build
ctest --test-dir src/BH1750/build --output-on-failure
```

## Packages

On Debian or Raspberry Pi OS:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config libi2c-dev i2c-tools
```

## Hardware assumptions

- I2C device path: `/dev/i2c-1`
- default BH1750 address: `0x23`
- BH1750 is used in continuous high-resolution mode

Typical wiring:
- VCC to 3.3V
- GND to GND
- SDA to GPIO2 / SDA1
- SCL to GPIO3 / SCL1

Confirm the sensor is visible:

```bash
sudo i2cdetect -y 1
```

## Runtime behavior

The sensor thread blocks in `poll()` and wakes when:
- the periodic `timerfd` expires
- the `eventfd` stop signal is written

Each wake reads lux once and forwards the value through the registered callback.

## Validation

Validated locally with:
- CMake configure
- CMake build
- CTest unit test

Validated on Raspberry Pi during standalone refactor verification:
- BH1750 detected on I2C bus 1 at `0x23`
- callback flow produced correct open and closed transitions from live lux data

Observed Pi output included:

```text
door=open lux=425
door=closed lux=0.833333
door=open lux=69.1667
door=closed lux=0.833333
```

## Integration note

This directory now provides the BH1750 light sensor as a reusable library module for the integrated application. The test target remains here so the callback and controller logic can still be verified independently from wider team integration.

## Owner and responsibility

This BH1750 module refactor and validation work was carried out by Hamna Khalid.
This includes:
- callback-based light sensor interface
- event-driven sampling thread
- CMake test restoration
- Raspberry Pi validation
- BH1750 module documentation update
