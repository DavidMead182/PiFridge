#include <iostream>
#include <vector>
#include <string>

#include "../include/DoorLightController.hpp"

class FakeLightSensor : public ILightSensor {
public:
    explicit FakeLightSensor(std::vector<double> luxSequence)
        : lux_(std::move(luxSequence)), idx_(0) {}

    double readLux() override {
        if (lux_.empty()) return 0.0;
        if (idx_ >= lux_.size()) return lux_.back();
        return lux_[idx_++];
    }

private:
    std::vector<double> lux_;
    std::size_t idx_;
};

class FakeGpioOutput : public IGpioOutput {
public:
    FakeGpioOutput() : high_(false), highCalls_(0), lowCalls_(0) {}

    void setHigh() override {
        high_ = true;
        highCalls_++;
    }

    void setLow() override {
        high_ = false;
        lowCalls_++;
    }

    bool isHigh() const { return high_; }
    int highCalls() const { return highCalls_; }
    int lowCalls() const { return lowCalls_; }

private:
    bool high_;
    int highCalls_;
    int lowCalls_;
};

static int g_failures = 0;

static void expectTrue(bool cond, const std::string& msg) {
    if (!cond) {
        std::cout << "FAIL: " << msg << "\n";
        g_failures++;
    }
}

static void expectEqInt(int a, int b, const std::string& msg) {
    if (a != b) {
        std::cout << "FAIL: " << msg << " expected " << b << " got " << a << "\n";
        g_failures++;
    }
}

int main() {
    {
        FakeLightSensor sensor({0.0, 5.0, 9.0});
        FakeGpioOutput gpio;
        DoorLightController ctl(sensor, gpio, 30.0, 10.0);

        ctl.update();
        ctl.update();
        ctl.update();

        expectTrue(!ctl.isDoorOpen(), "door should remain closed under open threshold");
        expectTrue(!gpio.isHigh(), "gpio should remain low when door is closed");
        expectEqInt(gpio.highCalls(), 0, "setHigh should not be called");
        expectEqInt(gpio.lowCalls(), 0, "setLow should not be called");
    }

    {
        FakeLightSensor sensor({0.0, 31.0});
        FakeGpioOutput gpio;
        DoorLightController ctl(sensor, gpio, 30.0, 10.0);

        ctl.update();
        ctl.update();

        expectTrue(ctl.isDoorOpen(), "door should open when lux meets open threshold");
        expectTrue(gpio.isHigh(), "gpio should be high when door is open");
        expectEqInt(gpio.highCalls(), 1, "setHigh should be called once");
        expectEqInt(gpio.lowCalls(), 0, "setLow should not be called");
    }

    {
        FakeLightSensor sensor({35.0, 20.0, 11.0});
        FakeGpioOutput gpio;
        DoorLightController ctl(sensor, gpio, 30.0, 10.0);

        ctl.update(); // open
        ctl.update(); // still open between thresholds
        ctl.update(); // still open above close threshold

        expectTrue(ctl.isDoorOpen(), "door should remain open above close threshold");
        expectTrue(gpio.isHigh(), "gpio should remain high while open");
        expectEqInt(gpio.highCalls(), 1, "setHigh should be called once");
        expectEqInt(gpio.lowCalls(), 0, "setLow should not be called");
    }

    {
        FakeLightSensor sensor({35.0, 20.0, 9.0});
        FakeGpioOutput gpio;
        DoorLightController ctl(sensor, gpio, 30.0, 10.0);

        ctl.update(); // open
        ctl.update(); // still open
        ctl.update(); // close

        expectTrue(!ctl.isDoorOpen(), "door should close when lux meets close threshold");
        expectTrue(!gpio.isHigh(), "gpio should be low after closing");
        expectEqInt(gpio.highCalls(), 1, "setHigh should be called once");
        expectEqInt(gpio.lowCalls(), 1, "setLow should be called once");
    }

    if (g_failures == 0) {
        std::cout << "PASS\n";
        return 0;
    }

    std::cout << "FAILURES: " << g_failures << "\n";
    return 1;
}
