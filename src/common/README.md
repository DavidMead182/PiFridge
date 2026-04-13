# common — Shared I2C Abstraction Layer

Provides the concrete Linux I2C implementation used by all sensor modules in PiFridge.


## Overview

This module contains a single concrete class, `LinuxI2CDevice`, which implements the `II2CDevice` interface defined in `include/II2CDevice.hpp`. It is the **only** place in the codebase that includes Linux-specific I2C headers (`linux/i2c-dev.h`). All sensor drivers (e.g. `BME680`, `BH1750`) depend solely on the `II2CDevice` interface — they never see platform-specific code.

This design follows the **Dependency Inversion Principle**: high-level sensor drivers depend on an abstraction, not on Linux internals. It also makes every sensor driver independently testable by injecting a mock `II2CDevice` without requiring physical hardware.


## Files

| File | Purpose |
|||
| `LinuxI2CDevice.hpp` | Declaration of the concrete Linux I2C implementation |
| `LinuxI2CDevice.cpp` | Opens `/dev/i2c-*`, sets slave address, implements read/write |
| `CMakeLists.txt` | Builds the `linux_i2c` static library |

The `II2CDevice` interface itself lives at `include/II2CDevice.hpp` at the project root, so it can be included by any module without creating a circular dependency on `common`.


## Interface

`LinuxI2CDevice` implements the `II2CDevice` interface:

```cpp
void writeReg(uint8_t reg, uint8_t value);
void readReg (uint8_t reg, uint8_t* data, size_t len);
```

- **`writeReg`** — writes a single byte to a register address. Used for configuration and control registers.
- **`readReg`** — writes the register address then reads `len` bytes back in a single I2C transaction. Used for burst reads of sensor data and calibration coefficients.


## Usage

`LinuxI2CDevice` is always injected into a sensor driver via `std::unique_ptr`, transferring ownership cleanly:

```cpp
#include "common/LinuxI2CDevice.hpp"
#include "BME680.hpp"

auto dev = std::make_unique<LinuxI2CDevice>(1, 0x77); // bus 1, address 0x77
BME680 sensor(std::move(dev));
sensor.initialize(BME680Settings{});
```

The sensor driver holds the `II2CDevice` by `std::unique_ptr<II2CDevice>` — it never knows it is talking to a Linux device specifically.


## Design Notes

### Why a separate abstraction layer?
Without `II2CDevice`, every sensor driver would `#include <linux/i2c-dev.h>` directly, making them untestable on non-Linux machines and tightly coupled to the OS. By isolating this in `common`, the rest of the codebase is clean and portable.

### Why `final`?
`LinuxI2CDevice` is marked `final` because it is a leaf implementation — it is not intended to be subclassed. Subclassing a concrete platform class would bypass the abstraction entirely.

### Error handling
Both `writeReg` and `readReg` throw `std::runtime_error` if the underlying `write()` or `read()` syscall returns fewer bytes than expected. The constructor throws if the bus cannot be opened or if `ioctl(I2C_SLAVE)` fails. This ensures failures surface immediately rather than silently producing corrupt data.



## Building

`linux_i2c` is built as a static library and linked by any module that needs I2C access:

```bash
cmake .
make linux_i2c
```

Sensor modules declare their dependency in their own `CMakeLists.txt`:

```cmake
target_link_libraries(bme680 PRIVATE linux_i2c)
```



## Dependencies

- `linux/i2c-dev.h` — kernel I2C userspace API (standard on all Raspberry Pi OS installs)
- `II2CDevice.hpp` — pure virtual interface, from `include/` at project root

No additional packages are required beyond the standard build tools.



## Author

**David Mead** — `LinuxI2CDevice` implementation, `II2CDevice` interface design, and CMake build configuration.



## Acknowledgements

I2C userspace access pattern follows Dr. Bernd Porr's [*Realtime Embedded Coding in C++ under Linux*](https://berndporr.github.io/realtime_cpp_coding/).
