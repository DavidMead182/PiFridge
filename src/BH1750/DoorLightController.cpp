#include "DoorLightController.hpp"

#include <stdexcept>
#include <utility>

DoorLightController::DoorLightController(double openThresholdLux,
                                         double closeThresholdLux)
    : openThresholdLux_(openThresholdLux),
      closeThresholdLux_(closeThresholdLux),
      isOpen_(false),
      lastLux_(-1.0) {
    if (closeThresholdLux_ > openThresholdLux_) {
        throw std::invalid_argument("close threshold must be <= open threshold");
    }
}

void DoorLightController::hasLightSample(double lux) {
    lastLux_ = lux;

    // Open only when the lux reading crosses the upper threshold.
    if (!isOpen_ && lux >= openThresholdLux_) {
        isOpen_ = true;
        notifyDoorState(true, lux);
        return;
    }

    // Close only when the lux reading falls below the lower threshold.
    if (isOpen_ && lux <= closeThresholdLux_) {
        isOpen_ = false;
        notifyDoorState(false, lux);
    }
}

void DoorLightController::registerDoorStateCallback(DoorStateCallback callback) {
    doorStateCallback_ = std::move(callback);
}

bool DoorLightController::isDoorOpen() const {
    return isOpen_;
}

double DoorLightController::lastLux() const {
    return lastLux_;
}

void DoorLightController::notifyDoorState(bool isOpen, double lux) {
    if (doorStateCallback_) {
        doorStateCallback_(isOpen, lux);
    }
}