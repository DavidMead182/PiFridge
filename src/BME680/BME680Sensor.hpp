#pragma once
 
#include "BME680.hpp"
 
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
 
/**
 * @brief Evented wrapper around BME680.
 *
 * Owns a thread driven by timerfd blocking I/O (handout section 3.3.4).
 * Fires a std::function callback with a BME680Sample at each interval.
 *
 * Single Responsibility: owns the thread and timing only.
 * The BME680 driver handles the sensor protocol.
 * The callback handler (subscriber) handles what to do with the data.
 *
 * Usage:
 * @code
 *   BME680Sensor sensor(1, 0x77, std::chrono::milliseconds(5000));
 *   sensor.registerCallback([&](const BME680Sample& s) {
 *       std::cout << s.temperature_c << "\n";
 *   });
 *   sensor.start();
 *   // ... later ...
 *   sensor.stop();
 * @endcode
 */
class BME680Sensor {
public:
    using Callback = std::function<void(const BME680Sample&)>;
 
    /**
     * @param i2c_bus        I2C bus number (usually 1 on a Pi)
     * @param i2c_addr       I2C address (0x76 or 0x77)
     * @param interval       How often to take a reading
     * @param sensor_settings Oversampling, filter, heater settings
     */
    BME680Sensor(int i2c_bus,
                 uint8_t i2c_addr,
                 std::chrono::milliseconds interval,
                 BME680Settings sensor_settings = {});
 
    ~BME680Sensor();
 
    /** Register the lambda called on each new sample (handout section 2.2.1). */
    void registerCallback(Callback cb);
 
    /** Start the sensor thread. Throws if already running. */
    void start();
 
    /** Stop the sensor thread and join it (handout section 3.3.2). */
    void stop();
 
private:
    static timespec toTimespec(std::chrono::milliseconds ms);
 
    /** Worker: blocks on timerfd read, fires callback (handout section 3.3.3). */
    void run();
 
    int                        i2c_bus_;
    uint8_t                    i2c_addr_;
    std::chrono::milliseconds  interval_;
    BME680Settings             sensor_settings_;
    Callback                   cb_;
 
    std::unique_ptr<BME680>    bme_;
    int                        tfd_ = -1;
    std::thread                worker_;
    std::atomic<bool>          running_{false};
};