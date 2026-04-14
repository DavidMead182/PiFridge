#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include "ILightSensor.hpp"

/**
 * @brief BH1750 light sensor implementation for Raspberry Pi Linux systems.
 *
 * This class owns the BH1750 I2C device handle, runs a dedicated worker thread,
 * and publishes lux readings through the callback defined by ILightSensor.
 *
 * Sampling is driven by blocking I/O wakeup in the implementation rather than
 * by a sleep-based polling loop.
 */
class Bh1750Sensor final : public ILightSensor {
public:
    /**
     * @brief Construct a BH1750 sensor on the given I2C device and address.
     *
     * @param i2cDevicePath Path to the Linux I2C device, typically /dev/i2c-1.
     * @param i2cAddress I2C slave address for the BH1750, typically 0x23.
     */
    explicit Bh1750Sensor(std::string i2cDevicePath = "/dev/i2c-1",
                          std::uint8_t i2cAddress = 0x23);

    ~Bh1750Sensor() override;

    Bh1750Sensor(const Bh1750Sensor&) = delete;
    Bh1750Sensor& operator=(const Bh1750Sensor&) = delete;

    /**
     * @brief Register the callback that receives lux samples.
     *
     * @param callback Function invoked whenever a new lux reading is available.
     */
    void registerCallback(LightLevelCallback callback) override;

    /**
     * @brief Start periodic sensor sampling.
     *
     * @param intervalMs Sampling interval in milliseconds.
     */
    void start(int intervalMs) override;

    /**
     * @brief Stop sensor sampling and join the worker thread.
     */
    void stop() override;

private:
    /**
     * @brief Send a command byte to the BH1750.
     *
     * @param cmd BH1750 command byte.
     */
    void writeCommand(std::uint8_t cmd);

    /**
     * @brief Read one lux sample from the BH1750.
     *
     * @return Current light intensity in lux.
     */
    double readLuxOnce();

    /**
     * @brief Worker loop that waits for sample timing events and emits lux data.
     *
     * @param intervalMs Sampling interval in milliseconds.
     */
    void runLoop(int intervalMs);

    int fd_;
    std::string devPath_;
    std::uint8_t addr_;
    std::mutex callbackMutex_;
    LightLevelCallback callback_;
    std::atomic<bool> running_;
    int stopFd_;
    std::thread worker_;
};