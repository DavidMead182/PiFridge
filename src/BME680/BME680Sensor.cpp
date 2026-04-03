#include "BME680Sensor.hpp"
#include "../common/LinuxI2CDevice.hpp"
 
#include <iostream>
#include <stdexcept>
#include <sys/timerfd.h>
#include <unistd.h>
 
BME680Sensor::BME680Sensor(int i2c_bus,
                           uint8_t i2c_addr,
                           std::chrono::milliseconds interval,
                           BME680Settings sensor_settings)
    : i2c_bus_(i2c_bus)
    , i2c_addr_(i2c_addr)
    , interval_(interval)
    , sensor_settings_(sensor_settings)
{}
 
BME680Sensor::~BME680Sensor() {
    stop();
}
 
void BME680Sensor::registerCallback(Callback cb) {
    cb_ = std::move(cb);
}
 
void BME680Sensor::start() {
    if (running_) return;
 
    // Inject the Linux I2C implementation into the driver
    auto dev = std::make_unique<LinuxI2CDevice>(i2c_bus_, i2c_addr_);
    bme_ = std::make_unique<BME680>(std::move(dev));
    bme_->initialize(sensor_settings_);
 
    // Set up timerfd for reliable interval timing (handout section 3.4)
    tfd_ = ::timerfd_create(CLOCK_MONOTONIC, 0);
    if (tfd_ < 0) throw std::runtime_error("BME680Sensor: timerfd_create failed");
 
    itimerspec its{};
    its.it_value    = toTimespec(interval_);
    its.it_interval = toTimespec(interval_);
    ::timerfd_settime(tfd_, 0, &its, nullptr);
 
    running_ = true;
    worker_ = std::thread([this] { run(); });
}
 
void BME680Sensor::stop() {
    if (!running_) return;
    running_ = false;
 
    // Nudge timerfd so the blocking read() unblocks immediately
    if (tfd_ >= 0) {
        itimerspec its{};
        its.it_value.tv_nsec = 1;
        ::timerfd_settime(tfd_, 0, &its, nullptr);
    }
 
    // Wait for thread to finish (handout section 3.3.2)
    if (worker_.joinable()) worker_.join();
 
    if (tfd_ >= 0) { ::close(tfd_); tfd_ = -1; }
    bme_.reset();
}
 
void BME680Sensor::run() {
    while (running_) {
        uint64_t expirations = 0;
        if (::read(tfd_, &expirations, sizeof(expirations)) < 0) continue;
        if (!running_) break;
 
        try {
            const BME680Sample sample = bme_->readSample();
            if (cb_) cb_(sample);
        } catch (const std::exception& e) {
            std::cerr << "[BME680Sensor] Read error: " << e.what() << "\n";
        }
    }
}
 
timespec BME680Sensor::toTimespec(std::chrono::milliseconds ms) {
    timespec ts{};
    ts.tv_sec  = ms.count() / 1000;
    ts.tv_nsec = (ms.count() % 1000) * 1'000'000L;
    return ts;
}