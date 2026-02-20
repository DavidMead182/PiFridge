#include "Bh1750Sensor.hpp"

#include <stdexcept>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

namespace {
constexpr std::uint8_t kCmdPowerOn = 0x01;
constexpr std::uint8_t kCmdReset = 0x07;
constexpr std::uint8_t kCmdContinuousHighRes = 0x10;
constexpr double kLuxDivisor = 1.2;
}

Bh1750Sensor::Bh1750Sensor(std::string i2cDevicePath, std::uint8_t i2cAddress)
    : fd_(-1),
      devPath_(std::move(i2cDevicePath)),
      addr_(i2cAddress) {

    fd_ = ::open(devPath_.c_str(), O_RDWR);
    if (fd_ < 0) {
        throw std::runtime_error("Bh1750Sensor open failed: " + devPath_);
    }

    if (::ioctl(fd_, I2C_SLAVE, addr_) < 0) {
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error("Bh1750Sensor ioctl I2C_SLAVE failed");
    }

    writeCommand(kCmdPowerOn);
    writeCommand(kCmdReset);
    writeCommand(kCmdContinuousHighRes);
}

Bh1750Sensor::~Bh1750Sensor() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

void Bh1750Sensor::writeCommand(std::uint8_t cmd) {
    int rc = ::write(fd_, &cmd, 1);
    if (rc != 1) {
        throw std::runtime_error("Bh1750Sensor write command failed");
    }
}

double Bh1750Sensor::readLux() {
    std::uint8_t buf[2] = {0, 0};
    int rc = ::read(fd_, buf, 2);
    if (rc != 2) {
        throw std::runtime_error("Bh1750Sensor read failed");
    }

    std::uint16_t raw = static_cast<std::uint16_t>(buf[0] << 8) | buf[1];
    return static_cast<double>(raw) / kLuxDivisor;
}
