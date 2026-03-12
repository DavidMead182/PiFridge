#pragma once

#include <cstdint>
#include <string>

#include "ILightSensor.hpp"

class Bh1750Sensor final : public ILightSensor {
public:
    explicit Bh1750Sensor(std::string i2cDevicePath = "/dev/i2c-1",
                          std::uint8_t i2cAddress = 0x23);

    ~Bh1750Sensor() override;

    Bh1750Sensor(const Bh1750Sensor&) = delete;
    Bh1750Sensor& operator=(const Bh1750Sensor&) = delete;

    double readLux() override;

private:
    void writeCommand(std::uint8_t cmd);

    int fd_;
    std::string devPath_;
    std::uint8_t addr_;
};
