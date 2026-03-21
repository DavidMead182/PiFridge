#pragma once
 
#include <cstddef>
#include <cstdint>
 
/**
 * @brief Abstract I2C device interface.
 *
 * Any class that talks to a physical I2C device must implement this.
 * This allows sensor drivers (BME680, BH1750, etc.) to depend only on
 * this interface, not on Linux-specific headers - following the
 * Dependency Inversion Principle (SOLID).
 */
struct II2CDevice {
    virtual ~II2CDevice() = default;
 
    /**
     * @brief Write a single register.
     * @param reg  Register address.
     * @param value Byte to write.
     */
    virtual void writeReg(uint8_t reg, uint8_t value) = 0;
 
    /**
     * @brief Read one or more bytes from a register.
     * @param reg  Register address to start reading from.
     * @param data Output buffer.
     * @param len  Number of bytes to read.
     */
    virtual void readReg(uint8_t reg, uint8_t* data, size_t len) = 0;
};
 