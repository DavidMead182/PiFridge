#pragma once
 
#include "../../include/II2CDevice.hpp"
 
// Concrete Linux implementation of II2CDevice.
// Lives in src/common/ so it can be shared by all sensor modules.
#include <cstdint>
#include <string>
 
/**
 * @brief Linux /dev/i2c-* implementation of II2CDevice.
 *
 * This is the only file that includes Linux-specific headers (linux/i2c-dev.h).
 * The BME680 driver never sees these - it only sees II2CDevice.
 *
 * Usage:
 * @code
 *   auto dev = std::make_unique<LinuxI2CDevice>(1, 0x77);
 *   BME680 sensor(std::move(dev));
 * @endcode
 */
class LinuxI2CDevice final : public II2CDevice {
public:
    /**
     * @param bus     I2C bus number (e.g. 1 for /dev/i2c-1 on a Pi)
     * @param address 7-bit I2C address of the sensor
     * @throws std::runtime_error if the bus cannot be opened
     */
    LinuxI2CDevice(int bus, uint8_t address);
    ~LinuxI2CDevice() override;
 
    void writeReg(uint8_t reg, uint8_t value) override;
    void readReg(uint8_t reg, uint8_t* data, size_t len) override;
 
private:
    int fd_ = -1;
};