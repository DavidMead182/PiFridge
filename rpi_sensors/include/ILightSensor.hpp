#pragma once

class ILightSensor {
public:
    virtual ~ILightSensor() = default;

    // Returns current light level in lux
    virtual double readLux() = 0;
};
