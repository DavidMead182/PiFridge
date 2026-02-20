# Raspberry Pi sensors

C++ code for Raspberry Pi sensor reads and simple control logic.
Core logic is unit tested without hardware.

## Light sensor feature

BH1750 light sensor over I2C
Door open close detection using lux thresholds with hysteresis
GPIO output to control the fridge light

## Wiring BH1750

VCC to 3.3V
GND to GND
SDA to GPIO2 SDA1 pin 3
SCL to GPIO3 SCL1 pin 5

Default BH1750 I2C address is 0x23.
Some boards support 0x5c when address is set high.

## Run tests anywhere

```bash
cmake -S rpi_sensors -B rpi_sensors/build -G Ninja
cmake --build rpi_sensors/build
ctest --test-dir rpi_sensors/build --output-on-failure
```

## Run on Raspberry Pi

Enable I2C in raspi config.

Install dependencies:

```bash
sudo apt-get update -y
sudo apt-get install -y libgpiod-dev i2c-tools cmake ninja-build pkg-config
```

Confirm sensor is detected:

```bash
sudo i2cdetect -y 1
```

Build hardware demo:

```bash
cmake -S rpi_sensors -B rpi_sensors/build -G Ninja -DPIFRIDGE_BUILD_HARDWARE=ON
cmake --build rpi_sensors/build
```

Run demo:

```bash
sudo ./rpi_sensors/build/pifridge_light_sensor_demo
```

Options:

open threshold lux default 30
close threshold lux default 10
interval ms default 200
gpio line default 17

Example:

```bash
sudo ./rpi_sensors/build/pifridge_light_sensor_demo --open 40 --close 15 --gpio-line 17
```

If your light wiring is active low:

```bash
sudo ./rpi_sensors/build/pifridge_light_sensor_demo --active-low
```

## Calibration

Measure lux with door open and door closed.
Set open threshold higher than closed lux.
Set close threshold lower than open threshold to avoid flicker.
