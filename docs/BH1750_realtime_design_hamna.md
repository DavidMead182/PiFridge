# BH1750 realtime design note

## Owner

Hamna Khalid

## Area of responsibility

BH1750 light sensor module refactor and validation:
- callback-based interface
- event-driven sensor sampling
- CMake test integration
- Raspberry Pi validation
- BH1750 module documentation update

## Feedback addressed

Earlier feedback identified these problems:
- getter-based light sensor API
- no callback registration
- polling-style flow
- long and unclear `main`
- tests not showing event-driven behavior clearly

This refactor addressed that by:
- replacing the public getter-style interface with callback registration
- moving sampling into a dedicated worker thread
- using `timerfd`, `eventfd`, and `poll()` for blocking wakeup
- moving event handling into `DoorLightController::hasLightSample(...)`
- restoring a dedicated CMake unit test target

## Callback flow

`Bh1750Sensor`
-> reads lux sample
-> invokes registered `LightLevelCallback`
-> `DoorLightController::hasLightSample(double lux)`
-> updates door state
-> invokes registered `DoorStateCallback`

## Realtime rationale

The course requires event-driven C++ code using callbacks and or waking threads through blocking I/O instead of wait-based polling loops.

This module uses:
- `std::function` callback registration
- one worker thread in `Bh1750Sensor`
- `timerfd` to schedule sampling
- `eventfd` to signal shutdown
- `poll()` to block until either a timer tick or stop event occurs

This avoids a sleep-based busy loop and keeps the callback path small and deterministic.

## Latency note

Sampling interval is configurable through `start(int intervalMs)`.

During standalone Raspberry Pi validation, the demo was run with:
- `intervalMs = 500`
- `openThresholdLux = 30`
- `closeThresholdLux = 10`

The expected response bound is one configured sampling period plus normal Linux scheduling overhead. For fridge door detection this is acceptable because the event is human-scale and not microsecond-critical.

## Build and test

Use these commands:

    cmake -S src/BH1750 -B src/BH1750/build
    cmake --build src/BH1750/build
    ctest --test-dir src/BH1750/build --output-on-failure

## Raspberry Pi validation

Sensor detection on Pi:
- `/dev/i2c-1`
- address `0x23` visible in `i2cdetect`

Observed output during live run included:

    door=open lux=425
    door=closed lux=0.833333
    door=open lux=69.1667
    door=closed lux=0.833333
    door=open lux=31.6667
    door=closed lux=4.16667

This confirmed:
- live BH1750 reads worked on hardware
- thresholds were applied correctly
- callback-based event handling produced door open and closed transitions on the Pi

## Notes for integration

The callback-based BH1750 core should remain intact during wider integration.

Do not reintroduce:
- public getter-driven polling flow
- sleep-based timing loops
- long sensor-processing logic in `main`
- removal of module test coverage
