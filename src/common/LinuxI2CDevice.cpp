#include "LinuxI2CDevice.hpp"
 
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
 
LinuxI2CDevice::LinuxI2CDevice(int bus, uint8_t address) {
    const std::string path = "/dev/i2c-" + std::to_string(bus);
 
    fd_ = ::open(path.c_str(), O_RDWR);
    if (fd_ < 0)
        throw std::runtime_error("LinuxI2CDevice: cannot open " + path);
 
    if (::ioctl(fd_, I2C_SLAVE, address) < 0)
        throw std::runtime_error("LinuxI2CDevice: cannot set I2C_SLAVE address");
}
 
LinuxI2CDevice::~LinuxI2CDevice() {
    if (fd_ >= 0) ::close(fd_);
}
 
void LinuxI2CDevice::writeReg(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    if (::write(fd_, buf, 2) != 2)
        throw std::runtime_error("LinuxI2CDevice: writeReg failed");
}
 
void LinuxI2CDevice::readReg(uint8_t reg, uint8_t* data, size_t len) {
    if (::write(fd_, &reg, 1) != 1)
        throw std::runtime_error("LinuxI2CDevice: readReg address write failed");
    if (::read(fd_, data, len) != static_cast<ssize_t>(len))
        throw std::runtime_error("LinuxI2CDevice: readReg read failed");
}