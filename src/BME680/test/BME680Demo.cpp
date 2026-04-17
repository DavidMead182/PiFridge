#include "BME680Sensor.hpp"
#include <iostream>
#include <csignal>
#include <thread>

// ---------------------------------------------------------------------------
// Signal handling - Ctrl+C shuts everything down cleanly
// ---------------------------------------------------------------------------
static std::atomic<bool> g_quit{false};
static void sigHandler(int) { g_quit = true; }

int main() {
    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    BME680Settings sensorSettings;
    sensorSettings.osrs_t         = 4;
    sensorSettings.osrs_p         = 3;
    sensorSettings.osrs_h         = 2;
    sensorSettings.filter         = 2;
    sensorSettings.enable_gas     = true;
    sensorSettings.heater_temp_c  = 320;
    sensorSettings.heater_time_ms = 150;
    sensorSettings.ambient_temp_c = 25;

    BME680Sensor bme680(
        /*i2c_bus=*/  1,
        /*i2c_addr=*/ 0x76,
        /*interval=*/ std::chrono::milliseconds(5000),
        sensorSettings
    );
    
    bme680.registerCallback([&](const BME680Sample& sample) {
        std::cout
            << "[BME680] "
            << "T="   << sample.temperature_c << "°C  "
            << "P="   << sample.pressure_hpa  << "hPa  "
            << "RH="  << sample.humidity_rh   << "%  "
            << "Gas=" << sample.gas_ohms      << "ohm\n";
    });

    bme680.start();
    
    while (!g_quit) {
        pause();
    }

    std::cout << "\nShutting down temperature sensor...\n";
    bme680.stop();
}