#pragma once

#include <functional>


class DoorLightController {
public:
    using DoorStateCallback = std::function<void(bool, double)>;

    DoorLightController(double openThresholdLux,
                        double closeThresholdLux);

    void hasLightSample(double lux);
    void registerDoorStateCallback(DoorStateCallback callback);

    bool isDoorOpen() const;
    double lastLux() const;

private:
    void notifyDoorState(bool isOpen, double lux);

    double openThresholdLux_;
    double closeThresholdLux_;
    bool isOpen_;
    double lastLux_;
    DoorStateCallback doorStateCallback_;
};
