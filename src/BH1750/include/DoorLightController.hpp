#pragma once

#include "ILightSensor.hpp"
#include "IGpioOutput.hpp"

class DoorLightController {
public:
    DoorLightController(ILightSensor& sensor,
                        IGpioOutput& gpio,
                        double openThresholdLux,
                        double closeThresholdLux)
        : sensor_(sensor),
          gpio_(gpio),
          openThresholdLux_(openThresholdLux),
          closeThresholdLux_(closeThresholdLux),
          isOpen_(false),
          lastLux_(-1.0) {}

    void update() {
        double lux = sensor_.readLux();
        lastLux_ = lux;

        if (!isOpen_ && lux >= openThresholdLux_) {
            isOpen_ = true;
            gpio_.setHigh();
        }
        else if (isOpen_ && lux <= closeThresholdLux_) {
            isOpen_ = false;
            gpio_.setLow();
        }
    }

    bool isDoorOpen() const {
        return isOpen_;
    }

    double lastLux() const {
        return lastLux_;
    }

private:
    ILightSensor& sensor_;
    IGpioOutput& gpio_;
    double openThresholdLux_;
    double closeThresholdLux_;
    bool isOpen_;
    double lastLux_;
};
