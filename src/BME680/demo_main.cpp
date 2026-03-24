#include "BME680Sensor.hpp"
 
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
 
// ---------------------------------------------------------------------------
// Subscriber: prints samples - knows nothing about I2C, threads, or timers
// ---------------------------------------------------------------------------
 
class BME680Printer {
public:
    void onSample(const BME680Sample& s) {
        std::cout
            << "T="   << s.temperature_c << "°C  "
            << "P="   << s.pressure_hpa  << "hPa  "
            << "RH="  << s.humidity_rh   << "%  "
            << "Gas=" << s.gas_ohms      << "ohm\n";
    }
};
 
// ---------------------------------------------------------------------------
// Signal handling
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
        settings.osrs_t         = 4;
        settings.osrs_p         = 3;
        settings.osrs_h         = 2;
        settings.filter         = 2;
        settings.enable_gas     = true;
        settings.heater_temp_c  = 320;
        settings.heater_time_ms = 150;
        settings.ambient_temp_c = 25;
 
        BME680Sensor sensor(1, 0x77,
                            std::chrono::milliseconds(1000),
                            settings);
        BME680Printer printer;
 
        // Connect publisher -> subscriber with a lambda (handout section 2.2.1)
        sensor.registerCallback([&](const BME680Sample& s) {
            printer.onSample(s);
        });
 
        sensor.start();
        std::cout << "BME680 demo running. Press Ctrl+C to stop.\n";
 
        while (!g_quit)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
 
        sensor.stop();
 
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
 
    return 0;
}