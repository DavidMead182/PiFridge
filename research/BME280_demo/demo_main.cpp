// demo_main.cpp
// g++ -std=c++17 -O2 -pthread demo_main.cpp -o bme280_demo
//
// Requires Linux headers: i2c-dev, timerfd.
// Run with permissions for /dev/i2c-* (often root or i2c group).

#include "bme280.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <chrono>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/timerfd.h>
#include <linux/i2c-dev.h>

// -------------------------- Small utility: RAII FD --------------------------

class FileDescriptor {
public:
    FileDescriptor() = default;
    explicit FileDescriptor(int fd) : fd_(fd) {}
    ~FileDescriptor() { closeNoThrow(); }

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    FileDescriptor(FileDescriptor&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
    FileDescriptor& operator=(FileDescriptor&& other) noexcept {
        if (this != &other) {
            closeNoThrow();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int get() const { return fd_; }
    explicit operator bool() const { return fd_ >= 0; }

    void reset(int newFd = -1) {
        closeNoThrow();
        fd_ = newFd;
    }

private:
    void closeNoThrow() noexcept {
        if (fd_ >= 0) ::close(fd_);
        fd_ = -1;
    }
    int fd_ = -1;
};

// -------------------------- I2C abstraction + Linux impl --------------------------

struct II2CDevice {
    virtual ~II2CDevice() = default;
    virtual void writeBytes(const uint8_t* data, size_t len) = 0;
    virtual void readBytes(uint8_t* data, size_t len) = 0;
    virtual void writeReg(uint8_t reg, uint8_t value) = 0;
    virtual void readReg(uint8_t reg, uint8_t* data, size_t len) = 0;
};

class LinuxI2CDevice final : public II2CDevice {
public:
    LinuxI2CDevice(int bus, uint8_t address) {
        const std::string path = "/dev/i2c-" + std::to_string(bus);
        int fd = ::open(path.c_str(), O_RDWR);
        if (fd < 0) throw std::runtime_error("Failed to open " + path);
        fd_.reset(fd);

        if (::ioctl(fd_.get(), I2C_SLAVE, address) < 0) {
            throw std::runtime_error("Failed to set I2C_SLAVE addr");
        }
    }

    void writeBytes(const uint8_t* data, size_t len) override {
        ssize_t w = ::write(fd_.get(), data, len);
        if (w < 0 || static_cast<size_t>(w) != len) throw std::runtime_error("I2C write failed");
    }

    void readBytes(uint8_t* data, size_t len) override {
        ssize_t r = ::read(fd_.get(), data, len);
        if (r < 0 || static_cast<size_t>(r) != len) throw std::runtime_error("I2C read failed");
    }

    void writeReg(uint8_t reg, uint8_t value) override {
        uint8_t buf[2] = {reg, value};
        writeBytes(buf, sizeof(buf));
    }

    void readReg(uint8_t reg, uint8_t* data, size_t len) override {
        writeBytes(&reg, 1);
        readBytes(data, len);
    }

private:
    FileDescriptor fd_;
};

// -------------------------- Evented wrapper (thread + callback) --------------------------

class BME280Sensor {
public:
    using Callback = std::function<void(const BME280Sample&)>;

    explicit BME280Sensor(BME280Settings settings = {})
        : settings_(settings) {}

    ~BME280Sensor() { stop(); }

    void setSettings(const BME280Settings& s) {
        settings_ = s;
        if (bme_) bme_->applySettings(settings_);
    }

    void registerCallback(Callback cb) { callback_ = std::move(cb); }

    void start() {
        if (running_) return;

        auto dev = std::make_unique<LinuxI2CDevice>(settings_.i2c_bus, settings_.i2c_addr);
        bme_ = std::make_unique<BME280>(std::move(dev));
        bme_->initialize(settings_);

        int tfd = ::timerfd_create(CLOCK_MONOTONIC, 0);
        if (tfd < 0) throw std::runtime_error("timerfd_create failed");
        timerfd_.reset(tfd);

        itimerspec its{};
        its.it_value = toTimespec(settings_.interval);
        its.it_interval = toTimespec(settings_.interval);
        if (::timerfd_settime(timerfd_.get(), 0, &its, nullptr) < 0) {
            throw std::runtime_error("timerfd_settime failed");
        }

        running_ = true;
        worker_ = std::thread([this] { run(); });
    }

    void stop() {
        if (!running_) return;
        running_ = false;

        // Nudge timerfd read to unblock quickly
        if (timerfd_) {
            itimerspec its{};
            its.it_value.tv_nsec = 1;
            ::timerfd_settime(timerfd_.get(), 0, &its, nullptr);
        }

        if (worker_.joinable()) worker_.join();
        timerfd_.reset();
        bme_.reset();
    }

private:
    static timespec toTimespec(std::chrono::milliseconds ms) {
        timespec ts{};
        const auto count = ms.count();
        ts.tv_sec = static_cast<time_t>(count / 1000);
        ts.tv_nsec = static_cast<long>((count % 1000) * 1000000L);
        return ts;
    }

    void run() {
        while (running_) {
            uint64_t expirations = 0;
            ssize_t r = ::read(timerfd_.get(), &expirations, sizeof(expirations));
            if (r < 0) continue;
            if (!running_) break;

            try {
                if (!settings_.normal_mode) {
                    bme_->applySettings(settings_);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                const auto sample = bme_->readSample();
                if (callback_) callback_(sample);

            } catch (const std::exception& e) {
                std::cerr << "BME280 read error: " << e.what() << "\n";
            }
        }
    }

    BME280Settings settings_;
    Callback callback_;

    std::unique_ptr<BME280> bme_;
    FileDescriptor timerfd_;
    std::thread worker_;
    std::atomic<bool> running_{false};
};

// -------------------------- Demo subscriber --------------------------

class BME280Printer {
public:
    void onSample(const BME280Sample& s) {
        std::cout
            << "T=" << s.temperature_c << " °C, "
            << "P=" << s.pressure_hpa  << " hPa, "
            << "RH=" << s.humidity_rh  << " %\n";
    }
};

int main() {
    try {
        BME280Settings settings;
        settings.i2c_bus = 1;
        settings.i2c_addr = 0x76;
        settings.interval = std::chrono::milliseconds(1000);
        settings.osrs_t = 1;
        settings.osrs_p = 1;
        settings.osrs_h = 1;
        settings.filter = 0;
        settings.standby = 0;
        settings.normal_mode = true;

        BME280Sensor sensor(settings);
        BME280Printer printer;

        sensor.registerCallback([&](const BME280Sample& s) { printer.onSample(s); });
        sensor.start();

        std::cout << "Reading BME280... press Ctrl+C to exit.\n";
        while (true) std::this_thread::sleep_for(std::chrono::seconds(60));

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}