#pragma once

#include <functional>

class ILightSensor {
public:
    using LightLevelCallback = std::function<void(double)>;

    virtual ~ILightSensor();

    virtual void registerCallback(LightLevelCallback callback) = 0;
    virtual void start(int intervalMs) = 0;
    virtual void stop() = 0;
};
