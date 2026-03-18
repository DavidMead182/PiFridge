#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include "ILightSensor.hpp"

class Bh1750Sensor final : public ILightSensor {
public:
    explicit Bh1750Sensor(std::string i2cDevicePath = "/dev/i2c-1",
                          std::uint8_t i2cAddress = 0x23);

    ~Bh1750Sensor() override;

    Bh1750Sensor(const Bh1750Sensor&) = delete;
    Bh1750Sensor& operator=(const Bh1750Sensor&) = delete;

    void registerCallback(LightLevelCallback callback) override;
    void start(int intervalMs) override;
    void stop() override;

private:
    void writeCommand(std::uint8_t cmd);
    double readLuxOnce();
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
