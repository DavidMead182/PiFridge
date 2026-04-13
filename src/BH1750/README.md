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
| `include/ILightSensor.hpp` / `ILightSensor.cpp` | Callback-based BH1750 sensor interface |
| `include/Bh1750Sensor.hpp` / `Bh1750Sensor.cpp` | BH1750 I2C implementation, worker thread, and callback emission |
| `include/DoorLightController.hpp` / `DoorLightController.cpp` | Door open and closed detection using lux thresholds with hysteresis |
| `CMakeLists.txt` | Builds the `bh1750` and `bh1750_logic` static libraries and integrates the BH1750 unit test |
| `test/DoorLightControllerTest.cpp` | Unit test for door-state callback and hysteresis behavior |
 
## Callback Interfaces
 
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
 
The module uses hysteresis to avoid rapid flicker between open and closed states:
 
- **open threshold** - lux value at or above which the door is considered open
- **close threshold** - lux value at or below which the door is considered closed
 
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
 
## Latency Note
 
During Raspberry Pi validation, the sampling interval was configured at `500 ms`.
 
Because door-state evaluation happens once on each timer wakeup, the designed response bound is one configured sample interval plus normal Linux scheduling overhead. For fridge door detection this is acceptable because the event is human-scale and does not require microsecond-level response.
 
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
 
### Build the full project
From the project root:
 
```bash
cmake -B build
cmake --build build
```
 
## Hardware Assumptions
 
- I2C device path: `/dev/i2c-1`
- BH1750 address: `0x23`
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
 
The BH1750 module test currently covers `DoorLightController` behavior:
- remains closed below open threshold
- opens when lux crosses the open threshold
- stays open while lux remains above close threshold
- closes when lux drops below the close threshold
- callback fires only on real state changes
 
This test is integrated through CMake and exposed through CTest.
 
## Author and Responsibility
 
**Hamna Khalid**
 
This BH1750 module refactor and validation work includes:
- callback-based light sensor interface
- event-driven sampling thread
- door-state threshold and hysteresis logic
- CMake test restoration for the BH1750 module
- Raspberry Pi validation of the BH1750 sensor path
- BH1750 module documentation update
 
## I2C Address
 
| Address pin state | BH1750 address |
|-------------------|----------------|
| Default / low     | `0x23` |
| High              | `0x5c` |
 
## Acknowledgements
 
Threading, callback, and blocking I/O patterns were shaped by Dr. Bernd Porr's realtime C++ teaching material and course feedback on event-driven design. Software engineering structure, testing, and documentation practices were also informed by lectures from Dr. Chongfeng Wei.