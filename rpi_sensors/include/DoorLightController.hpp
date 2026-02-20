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
          isOpen_(false) {}

    void update() {
        double lux = sensor_.readLux();

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

private:
    ILightSensor& sensor_;
    IGpioOutput& gpio_;
    double openThresholdLux_;
    double closeThresholdLux_;
    bool isOpen_;
};
