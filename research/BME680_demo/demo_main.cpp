// demo_main.cpp
// g++ -std=c++17 -O2 -pthread demo_main.cpp -o bme680_demo
//
// Requires Linux headers: i2c-dev, timerfd.
// Run with permissions for /dev/i2c-* (often root or in i2c group).

#include "BME680.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/timerfd.h>
#include <unistd.h>

// -------------------------- Small utilities (RAII FD) --------------------------

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

// -------------------------- Linux I2C implementation --------------------------

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
        const ssize_t w = ::write(fd_.get(), data, len);
        if (w < 0 || static_cast<size_t>(w) != len) throw std::runtime_error("I2C write failed");
    }

    void readBytes(uint8_t* data, size_t len) override {
        const ssize_t r = ::read(fd_.get(), data, len);
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

// -------------------------- Implement a couple BME680 private helpers --------------------------

#include <cmath>

void BME680::sleepMs(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

std::vector<int32_t> BME680::unpackCoeffs(const uint8_t* b) {
    // Format: "<hbBHhbBhhbbHhhBBBHbbbBbHhbb"
    // Read sequentially from 38 bytes and output as int32s.
    std::vector<int32_t> out;
    out.reserve(32);

    size_t i = 0;
    auto need = [&](size_t n) {
        if (i + n > 38) throw std::runtime_error("Coeff unpack out of range");
    };

    auto rd_i8  = [&]() -> int32_t { need(1); return int8_t(b[i++]); };
    auto rd_u8  = [&]() -> int32_t { need(1); return uint8_t(b[i++]); };
    auto rd_i16 = [&]() -> int32_t { need(2); int16_t v = int16_t(uint16_t(b[i]) | (uint16_t(b[i+1])<<8)); i+=2; return v; };
    auto rd_u16 = [&]() -> int32_t { need(2); uint16_t v = uint16_t(b[i]) | (uint16_t(b[i+1])<<8); i+=2; return v; };

    // h b B H h b B h h b b H h h B B B H b b b B b H h b b
    out.push_back(rd_i16()); // h
    out.push_back(rd_i8());  // b
    out.push_back(rd_u8());  // B
    out.push_back(rd_u16()); // H
    out.push_back(rd_i16()); // h
    out.push_back(rd_i8());  // b
    out.push_back(rd_u8());  // B
    out.push_back(rd_i16()); // h
    out.push_back(rd_i16()); // h
    out.push_back(rd_i8());  // b
    out.push_back(rd_i8());  // b
    out.push_back(rd_u16()); // H
    out.push_back(rd_i16()); // h
    out.push_back(rd_i16()); // h
    out.push_back(rd_u8());  // B
    out.push_back(rd_u8());  // B
    out.push_back(rd_u8());  // B
    out.push_back(rd_u16()); // H
    out.push_back(rd_i8());  // b
    out.push_back(rd_i8());  // b
    out.push_back(rd_i8());  // b
    out.push_back(rd_u8());  // B
    out.push_back(rd_i8());  // b
    out.push_back(rd_u16()); // H
    out.push_back(rd_i16()); // h
    out.push_back(rd_i8());  // b
    out.push_back(rd_i8());  // b

    return out;
}

// -------------------------- Evented wrapper (thread + callback) --------------------------

struct DemoSettings {
    int i2c_bus = 1;
    uint8_t i2c_addr = 0x77; // common BME680 addresses: 0x76 or 0x77
    std::chrono::milliseconds interval{1000};
    BME680Settings sensor{};
};

class BME680Sensor {
public:
    using Callback = std::function<void(const BME680Sample&)>;

    explicit BME680Sensor(DemoSettings s = {}) : cfg_(s) {}
    ~BME680Sensor() { stop(); }

    void registerCallback(Callback cb) { cb_ = std::move(cb); }

    void setConfig(const DemoSettings& s) {
        cfg_ = s;
        if (bme_) bme_->applySettings(cfg_.sensor);
    }

    void start() {
        if (running_) return;

        auto dev = std::make_unique<LinuxI2CDevice>(cfg_.i2c_bus, cfg_.i2c_addr);
        bme_ = std::make_unique<BME680>(std::move(dev));
        bme_->initialize(cfg_.sensor);

        int tfd = ::timerfd_create(CLOCK_MONOTONIC, 0);
        if (tfd < 0) throw std::runtime_error("timerfd_create failed");
        timerfd_.reset(tfd);

        itimerspec its{};
        its.it_value = toTimespec(cfg_.interval);
        its.it_interval = toTimespec(cfg_.interval);

        if (::timerfd_settime(timerfd_.get(), 0, &its, nullptr) < 0) {
            throw std::runtime_error("timerfd_settime failed");
        }

        running_ = true;
        worker_ = std::thread([this] { run(); });
    }

    void stop() {
        if (!running_) return;
        running_ = false;

        // Nudge timerfd so read unblocks quickly.
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
            const ssize_t r = ::read(timerfd_.get(), &expirations, sizeof(expirations));
            if (r < 0) continue;
            if (!running_) break;

            try {
            
                // record the exact microsecond before the sensor read
                auto start_time = std::chrono::high_resolution_clock::now();

                const auto sample = bme_->readSample();

                // We record the exact microsecond after the sensor read
                auto end_time = std::chrono::high_resolution_clock::now();
                
                // Calculate the difference in microseconds (us)
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

                //  prints the real-time performance to the terminal
                std::cout << "[RT-METRIC] I2C Read Latency: " << duration << " us" << std::endl;
                

                if (cb_) cb_(sample);

                /* I am commenting out the original line below as a backup
                // const auto sample = bme_->readSample();
                */
            }
        }
    }

    DemoSettings cfg_;
    Callback cb_;

    std::unique_ptr<BME680> bme_;
    FileDescriptor timerfd_;
    std::thread worker_;
    std::atomic<bool> running_{false};
};

// -------------------------- Demo subscriber --------------------------

class BME680Printer {
public:
    void onSample(const BME680Sample& s) {
        std::cout
            << "T="  << s.temperature_c << " °C, "
            << "P="  << s.pressure_hpa  << " hPa, "
            << "RH=" << s.humidity_rh   << " %, "
            << "Gas="<< s.gas_ohms      << " ohms\n";
    }
};

int main() {
    try {
        DemoSettings cfg;
        cfg.i2c_bus = 1;
        cfg.i2c_addr = 0x77; // change to 0x76 if your board uses that
        cfg.interval = std::chrono::milliseconds(1000);

        // Sensor tuning
        cfg.sensor.osrs_t = 4; // x8
        cfg.sensor.osrs_p = 3; // x4
        cfg.sensor.osrs_h = 2; // x2
        cfg.sensor.filter = 2;

        cfg.sensor.enable_gas = true;
        cfg.sensor.heater_temp_c = 320;
        cfg.sensor.heater_time_ms = 150;
        cfg.sensor.ambient_temp_c = 25;

        BME680Sensor sensor(cfg);
        BME680Printer printer;

        sensor.registerCallback([&](const BME680Sample& s) { printer.onSample(s); });
        sensor.start();

        std::cout << "Reading BME680 (forced mode)... press Ctrl+C to exit.\n";
        while (true) std::this_thread::sleep_for(std::chrono::seconds(60));

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}