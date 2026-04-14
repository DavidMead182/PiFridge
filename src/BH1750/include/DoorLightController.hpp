#pragma once

#include <functional>

/**
 * @brief Converts lux samples into door open and closed state changes.
 *
 * The controller applies hysteresis using separate open and close thresholds
 * so that small fluctuations in light level do not cause repeated toggling.
 *
 * A door-state callback is fired only when the state actually changes.
 */
class DoorLightController {
public:
    using DoorStateCallback = std::function<void(bool, double)>;

    /**
     * @brief Construct a controller with open and close thresholds.
     *
     * @param openThresholdLux Lux value at or above which the door is open.
     * @param closeThresholdLux Lux value at or below which the door is closed.
     */
    DoorLightController(double openThresholdLux,
                        double closeThresholdLux);

    /**
     * @brief Consume one lux sample and update door state if needed.
     *
     * @param lux Latest light reading in lux.
     */
    void hasLightSample(double lux);

    /**
     * @brief Register the callback fired when door state changes.
     *
     * @param callback Function invoked on open or closed transitions.
     */
    void registerDoorStateCallback(DoorStateCallback callback);

    /**
     * @brief Return the current door state.
     *
     * @return true if the door is currently open, otherwise false.
     */
    bool isDoorOpen() const;

    /**
     * @brief Return the latest lux sample seen by the controller.
     *
     * @return Most recent lux value.
     */
    double lastLux() const;

private:
    /**
     * @brief Notify the registered callback about a state transition.
     *
     * @param isOpen New door state.
     * @param lux Lux value that triggered the transition.
     */
    void notifyDoorState(bool isOpen, double lux);

    double openThresholdLux_;
    double closeThresholdLux_;
    bool isOpen_;
    double lastLux_;
    DoorStateCallback doorStateCallback_;
};