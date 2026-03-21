// demo_main.cpp
// Demonstrates BME680 using the callback pattern described in the RT coding handout.
//
// Build:
//   g++ -std=c++17 -O2 -pthread \
//       demo_main.cpp BME680.cpp LinuxI2CDevice.cpp \
//       -I../../include \
//       -o bme680_demo
//
// Run (needs i2c group or root):
//   ./bme680_demo
 
#include "BME680.hpp"
#include "LinuxI2CDevice.hpp"
 
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <functional>
#include <stdexcept>
#include <sys/timerfd.h>
#include <thread>
#include <unistd.h>
 
// ---------------------------------------------------------------------------
// Subscriber: just prints the sample - knows nothing about I2C or threads
// ---------------------------------------------------------------------------
 
class BME680Printer {
public:
    void onSample(const BME680Sample& s) {
        std::cout
            << "T="   << s.temperature_c << " °C  "
            << "P="   << s.pressure_hpa  << " hPa  "
            << "RH="  << s.humidity_rh   << " %  "
            << "Gas=" << s.gas_ohms      << " ohm\n";
    }
};
 
// ---------------------------------------------------------------------------
// Evented sensor wrapper: owns thread + timerfd, fires callback each interval
// As per handout: blocking I/O (timerfd read) drives the timing.
// ---------------------------------------------------------------------------
 
class BME680Sensor {
public:
    using Callback = std::function<void(const BME680Sample&)>;
 
    explicit BME680Sensor(int i2c_bus, uint8_t i2c_addr,
                          std::chrono::milliseconds interval,
                          BME680Settings sensor_settings = {})
        : interval_(interval)
        , sensor_settings_(sensor_settings)
        , i2c_bus_(i2c_bus)
        , i2c_addr_(i2c_addr)
    {}
 
    ~BME680Sensor() { stop(); }
 
    /** Register the lambda/functor called on each new sample. */
    void registerCallback(Callback cb) { cb_ = std::move(cb); }
 
    void start() {
        if (running_) return;
 
        // Create sensor - inject the Linux I2C implementation
        auto dev = std::make_unique<LinuxI2CDevice>(i2c_bus_, i2c_addr_);
        bme_ = std::make_unique<BME680>(std::move(dev));
        bme_->initialize(sensor_settings_);
 
        // Set up timerfd for reliable interval timing (see handout section 3.4)
        tfd_ = ::timerfd_create(CLOCK_MONOTONIC, 0);
        if (tfd_ < 0) throw std::runtime_error("timerfd_create failed");
 
        itimerspec its{};
        its.it_value    = toTimespec(interval_);
        its.it_interval = toTimespec(interval_);
        ::timerfd_settime(tfd_, 0, &its, nullptr);
 
        running_ = true;
        worker_ = std::thread([this] { run(); });
    }
 
    void stop() {
        if (!running_) return;
        running_ = false;
 
        // Wake the blocked timerfd read immediately
        if (tfd_ >= 0) {
            itimerspec its{};
            its.it_value.tv_nsec = 1;
            ::timerfd_settime(tfd_, 0, &its, nullptr);
        }
 
        if (worker_.joinable()) worker_.join();
 
        if (tfd_ >= 0) { ::close(tfd_); tfd_ = -1; }
        bme_.reset();
    }
 
private:
    static timespec toTimespec(std::chrono::milliseconds ms) {
        timespec ts{};
        ts.tv_sec  = ms.count() / 1000;
        ts.tv_nsec = (ms.count() % 1000) * 1'000'000L;
        return ts;
    }
 
    // Worker: blocks on timerfd read, then fires the callback (handout 3.3.3)
    void run() {
        while (running_) {
            uint64_t expirations = 0;
            if (::read(tfd_, &expirations, sizeof(expirations)) < 0) continue;
            if (!running_) break;
 
            try {
                const BME680Sample sample = bme_->readSample();
                if (cb_) cb_(sample);
            } catch (const std::exception& e) {
                std::cerr << "BME680 read error: " << e.what() << "\n";
            }
        }
    }
 
    std::chrono::milliseconds interval_;
    BME680Settings             sensor_settings_;
    int                        i2c_bus_;
    uint8_t                    i2c_addr_;
    Callback                   cb_;
 
    std::unique_ptr<BME680>    bme_;
    int                        tfd_ = -1;
    std::thread                worker_;
    std::atomic<bool>          running_{false};
};
 
// ---------------------------------------------------------------------------
// Signal handling so Ctrl+C cleanly stops the thread
// ---------------------------------------------------------------------------
 
static std::atomic<bool> g_quit{false};
static void sigHandler(int) { g_quit = true; }
 
// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
 
int main() {
    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);
 
    try {
        BME680Settings settings;
        settings.osrs_t        = 4;    // x8
        settings.osrs_p        = 3;    // x4
        settings.osrs_h        = 2;    // x2
        settings.filter        = 2;
        settings.enable_gas    = true;
        settings.heater_temp_c = 320;
        settings.heater_time_ms= 150;
        settings.ambient_temp_c= 25;
 
        BME680Sensor sensor(
            /*i2c_bus=*/  1,
            /*i2c_addr=*/ 0x77,   // change to 0x76 if your board uses that
            /*interval=*/ std::chrono::milliseconds(1000),
            settings
        );
 
        BME680Printer printer;
 
        // Connect publisher -> subscriber with a lambda (handout 2.2.1)
        sensor.registerCallback([&](const BME680Sample& s) {
            printer.onSample(s);
        });
 
        sensor.start();
        std::cout << "BME680 running. Press Ctrl+C to stop.\n";
 
        while (!g_quit) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
 
        sensor.stop();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
 
    return 0;
}