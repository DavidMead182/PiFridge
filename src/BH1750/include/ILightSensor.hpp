#pragma once

#include <functional>

/**
 * @brief Abstract event-driven interface for light sensor modules.
 *
 * Implementations publish lux samples through a registered callback and
 * manage their own runtime lifecycle with start() and stop().
 *
 * This interface keeps the rest of the application independent of the
 * concrete sensor implementation and supports callback-based event flow.
 */
class ILightSensor {
public:
    using LightLevelCallback = std::function<void(double)>;

    virtual ~ILightSensor();

    /**
     * @brief Register the callback that receives lux samples.
     *
     * @param callback Function invoked whenever a new lux sample is available.
     */
    virtual void registerCallback(LightLevelCallback callback) = 0;

    /**
     * @brief Start periodic sensor sampling.
     *
     * @param intervalMs Sampling interval in milliseconds.
     */
    virtual void start(int intervalMs) = 0;

    /**
     * @brief Stop sensor sampling and release runtime resources.
     */
    virtual void stop() = 0;
};
