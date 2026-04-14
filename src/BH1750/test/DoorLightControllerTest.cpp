#include <cmath>
#include <iostream>
#include <string>

#include "../include/DoorLightController.hpp"
#include "../include/ILightSensor.hpp"

// Minimal fake sensor used to verify callback-chain behavior without hardware.
class FakeLightSensor final : public ILightSensor {
public:
    ~FakeLightSensor() override = default;

    void registerCallback(LightLevelCallback callback) override {
        callback_ = std::move(callback);
    }

    void start(int) override {}
    void stop() override {}

    void emit(double lux) {
        if (callback_) {
            callback_(lux);
        }
    }

private:
    LightLevelCallback callback_;
};

static void expectTrue(bool condition, const std::string& message, int& failures) {
    if (!condition) {
        std::cout << "FAIL: " << message << "\n";
        ++failures;
    }
}

static void expectNear(double actual, double expected, double tolerance,
                       const std::string& message, int& failures) {
    if (std::fabs(actual - expected) > tolerance) {
        std::cout << "FAIL: " << message
                  << " expected " << expected
                  << " got " << actual << "\n";
        ++failures;
    }
}

int main() {
    int failures = 0;

    {
        DoorLightController controller(30.0, 10.0);
        int callbackCount = 0;
        bool lastState = false;
        double lastLux = -1.0;

        controller.registerDoorStateCallback([&](bool isOpen, double lux) {
            ++callbackCount;
            lastState = isOpen;
            lastLux = lux;
        });

        controller.hasLightSample(0.0);
        controller.hasLightSample(5.0);
        controller.hasLightSample(9.0);

        expectTrue(!controller.isDoorOpen(),
                   "door should remain closed below open threshold",
                   failures);
        expectTrue(callbackCount == 0,
                   "callback should not fire while state does not change",
                   failures);
        expectNear(controller.lastLux(), 9.0, 1e-9,
                   "lastLux should track most recent sample",
                   failures);
        (void)lastState;
        (void)lastLux;
    }

    {
        DoorLightController controller(30.0, 10.0);
        int callbackCount = 0;
        bool lastState = false;
        double lastLux = -1.0;

        controller.registerDoorStateCallback([&](bool isOpen, double lux) {
            ++callbackCount;
            lastState = isOpen;
            lastLux = lux;
        });

        controller.hasLightSample(31.0);

        expectTrue(controller.isDoorOpen(),
                   "door should open when sample reaches open threshold",
                   failures);
        expectTrue(callbackCount == 1,
                   "callback should fire once on open transition",
                   failures);
        expectTrue(lastState,
                   "callback state should report open",
                   failures);
        expectNear(lastLux, 31.0, 1e-9,
                   "callback lux should report opening sample",
                   failures);
    }

    {
        DoorLightController controller(30.0, 10.0);
        int callbackCount = 0;

        controller.registerDoorStateCallback([&](bool, double) {
            ++callbackCount;
        });

        controller.hasLightSample(35.0);
        controller.hasLightSample(20.0);
        controller.hasLightSample(11.0);

        expectTrue(controller.isDoorOpen(),
                   "door should stay open while above close threshold",
                   failures);
        expectTrue(callbackCount == 1,
                   "callback count should remain one while state stays open",
                   failures);
        expectNear(controller.lastLux(), 11.0, 1e-9,
                   "lastLux should update even without state change",
                   failures);
    }

    {
        DoorLightController controller(30.0, 10.0);
        int callbackCount = 0;
        bool lastState = true;
        double lastLux = -1.0;

        controller.registerDoorStateCallback([&](bool isOpen, double lux) {
            ++callbackCount;
            lastState = isOpen;
            lastLux = lux;
        });

        controller.hasLightSample(35.0);
        controller.hasLightSample(20.0);
        controller.hasLightSample(9.0);

        expectTrue(!controller.isDoorOpen(),
                   "door should close when sample goes below close threshold",
                   failures);
        expectTrue(callbackCount == 2,
                   "callback should fire once for open and once for close",
                   failures);
        expectTrue(!lastState,
                   "callback state should report closed",
                   failures);
        expectNear(lastLux, 9.0, 1e-9,
                   "callback lux should report closing sample",
                   failures);
    }

    {
        FakeLightSensor sensor;
        DoorLightController controller(30.0, 10.0);
        int callbackCount = 0;
        bool lastState = false;
        double lastLux = -1.0;

        // This test verifies the full callback chain from sensor event to door-state event.
        sensor.registerCallback([&](double lux) {
            controller.hasLightSample(lux);
        });

        controller.registerDoorStateCallback([&](bool isOpen, double lux) {
            ++callbackCount;
            lastState = isOpen;
            lastLux = lux;
        });

        sensor.emit(31.0);
        sensor.emit(9.0);

        expectTrue(callbackCount == 2,
                   "callback chain should propagate open and close transitions",
                   failures);
        expectTrue(!lastState,
                   "final state should be closed after callback-chain test",
                   failures);
        expectNear(lastLux, 9.0, 1e-9,
                   "final lux should match closing sample in callback-chain test",
                   failures);
        expectTrue(!controller.isDoorOpen(),
                   "controller should end closed after callback-chain test",
                   failures);
    }

    if (failures == 0) {
        std::cout << "PASS\n";
        return 0;
    }

    std::cout << "FAILURES: " << failures << "\n";
    return 1;
}