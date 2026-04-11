#include <cmath>
#include <iostream>
#include <string>
#include <utility>

#include "../include/DoorLightController.hpp"
#include "../include/ILightSensor.hpp"

class FakeLightSensor : public ILightSensor {
public:
    void registerCallback(LightLevelCallback callback) override {
        callback_ = std::move(callback);
    }

    void start(int) override {
    }

    void stop() override {
    }

    void emit(double lux) {
        if (callback_) {
            callback_(lux);
        }
    }

private:
    LightLevelCallback callback_;
};

class FakeGpioOutput : public IGpioOutput {
public:
    FakeGpioOutput() : high_(false), highCalls_(0), lowCalls_(0) {
    }

    void setHigh() override {
        high_ = true;
        ++highCalls_;
    }

    void setLow() override {
        high_ = false;
        ++lowCalls_;
    }

    bool isHigh() const {
        return high_;
    }

    int highCalls() const {
        return highCalls_;
    }

    int lowCalls() const {
        return lowCalls_;
    }

private:
    bool high_;
    int highCalls_;
    int lowCalls_;
};

int main() {
    int failures = 0;

    auto expectTrue = [&](bool cond, const std::string& msg) {
        if (!cond) {
            std::cout << "FAIL: " << msg << "\n";
            ++failures;
        }
    };

    auto expectEqInt = [&](int a, int b, const std::string& msg) {
        if (a != b) {
            std::cout << "FAIL: " << msg << " expected " << b << " got " << a << "\n";
            ++failures;
        }
    };

    auto expectNear = [&](double a, double b, const std::string& msg) {
        if (std::fabs(a - b) > 1e-9) {
            std::cout << "FAIL: " << msg << " expected " << b << " got " << a << "\n";
            ++failures;
        }
    };

    {
        FakeLightSensor sensor;
        FakeGpioOutput gpio;
        DoorLightController controller(gpio, 30.0, 10.0);

        sensor.registerCallback([&controller](double lux) {
            controller.hasLightSample(lux);
        });

        sensor.emit(0.0);
        sensor.emit(5.0);
        sensor.emit(9.0);

        expectTrue(!controller.isDoorOpen(), "door should remain closed under open threshold");
        expectTrue(!gpio.isHigh(), "gpio should remain low while door is closed");
        expectEqInt(gpio.highCalls(), 0, "setHigh should not be called");
        expectEqInt(gpio.lowCalls(), 0, "setLow should not be called");
        expectNear(controller.lastLux(), 9.0, "lastLux should track the latest sample");
    }

    {
        FakeLightSensor sensor;
        FakeGpioOutput gpio;
        DoorLightController controller(gpio, 30.0, 10.0);

        int doorEvents = 0;
        bool lastDoorState = false;
        double lastEventLux = -1.0;

        controller.registerDoorStateCallback([&](bool isOpen, double lux) {
            ++doorEvents;
            lastDoorState = isOpen;
            lastEventLux = lux;
        });

        sensor.registerCallback([&controller](double lux) {
            controller.hasLightSample(lux);
        });

        sensor.emit(0.0);
        sensor.emit(31.0);

        expectTrue(controller.isDoorOpen(), "door should open when lux meets open threshold");
        expectTrue(gpio.isHigh(), "gpio should be high after opening");
        expectEqInt(gpio.highCalls(), 1, "setHigh should be called once");
        expectEqInt(gpio.lowCalls(), 0, "setLow should not be called");
        expectEqInt(doorEvents, 1, "door state callback should fire once on opening");
        expectTrue(lastDoorState, "door state callback should report open");
        expectNear(lastEventLux, 31.0, "door state callback should report latest opening lux");
    }

    {
        FakeLightSensor sensor;
        FakeGpioOutput gpio;
        DoorLightController controller(gpio, 30.0, 10.0);

        sensor.registerCallback([&controller](double lux) {
            controller.hasLightSample(lux);
        });

        sensor.emit(35.0);
        sensor.emit(20.0);
        sensor.emit(11.0);

        expectTrue(controller.isDoorOpen(), "door should remain open above close threshold");
        expectTrue(gpio.isHigh(), "gpio should stay high while still open");
        expectEqInt(gpio.highCalls(), 1, "setHigh should still only be called once");
        expectEqInt(gpio.lowCalls(), 0, "setLow should not be called yet");
    }

    {
        FakeLightSensor sensor;
        FakeGpioOutput gpio;
        DoorLightController controller(gpio, 30.0, 10.0);

        int doorEvents = 0;

        controller.registerDoorStateCallback([&](bool, double) {
            ++doorEvents;
        });

        sensor.registerCallback([&controller](double lux) {
            controller.hasLightSample(lux);
        });

        sensor.emit(35.0);
        sensor.emit(20.0);
        sensor.emit(9.0);

        expectTrue(!controller.isDoorOpen(), "door should close when lux meets close threshold");
        expectTrue(!gpio.isHigh(), "gpio should be low after closing");
        expectEqInt(gpio.highCalls(), 1, "setHigh should be called once");
        expectEqInt(gpio.lowCalls(), 1, "setLow should be called once");
        expectEqInt(doorEvents, 2, "door state callback should fire on open and close");
    }

    if (failures == 0) {
        std::cout << "PASS\n";
        return 0;
    }

    std::cout << "FAILURES: " << failures << "\n";
    return 1;
}
