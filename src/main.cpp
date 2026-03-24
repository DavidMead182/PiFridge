// main.cpp
// PiFridge - main entry point.
//
// Build from repo root:
//   cmake -B build && cmake --build build
// Run:
//   sudo ./build/src/pifridge

#include "BME680Sensor.hpp"
#include "Bh1750Sensor.hpp"
#include "DoorLightController.hpp"
#include "GpioOutput.hpp"
 
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <mutex>
#include <thread>

struct FridgeState {
    BME680Sample    vitals{};
    bool            door_open = false;
    std::mutex      mutex;
};

// ---------------------------------------------------------------------------
// Signal handling - Ctrl+C shuts everything down cleanly
// ---------------------------------------------------------------------------
 
static std::atomic<bool> g_quit{false};
static void sigHandler(int) { g_quit = true; }

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    std::cout << "PiFridge Starting Up" << std::endl;

    // -- Shared state --
    FridgeState state;
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
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            state.vitals = sample;
        }
        std::cout
            << "[BME680] "
            << "T="   << sample.temperature_c << "°C  "
            << "P="   << sample.pressure_hpa  << "hPa  "
            << "RH="  << sample.humidity_rh   << "%  "
            << "Gas=" << sample.gas_ohms      << "ohm\n";
    });

    // -----------------------------------------------------------------------
    // BH1750 + DoorLightController - door detection
    // -----------------------------------------------------------------------

    GpioOutput gpio(
        /*chipName=*/   "gpiochip0",
        /*lineOffset=*/ 17,
        /*activeHigh=*/ true
    );

    DoorLightController doorController(
        gpio,
        /*openThresholdLux=*/  30.0,
        /*closeThresholdLux=*/ 10.0
    );

    // Callback: fires only when door state CHANGES (open->closed or closed->open)
    doorController.registerDoorStateCallback([&](bool isOpen, double lux) {
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            state.door_open = isOpen;
        }
 
        if (isOpen) {
            std::cout << "[Door] Opened (lux=" << lux << ") - Barcode scanner ON\n";
        } else {
            std::cout << "[Door] Closed (lux=" << lux << ") - Barcode scanner OFF\n";
        }
 
        // TODO: replace prints with real barcode scanner enable/disable
    });

    Bh1750Sensor lightSensor(
        /*i2cDevicePath=*/ "/dev/i2c-1",
        /*i2cAddress=*/    0x23
    );

    lightSensor.registerCallback([&doorController](double lux) {
        doorController.hasLightSample(lux);
    });


    bme680.start();
    std::cout << "PiFridge: BME680 Sensor Thread Started" << std::endl;

    lightSensor.start(/*intervalMs=*/ 200);
    std::cout << "PiFridge: Light Sensor Thread Started" << std::endl;

    // sever.start()

    std::cout << "PiFridge running. Press Ctrl+C to stop.\n";

    while (!g_quit) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nPiFridge shutting down...\n";
    lightSensor.stop();
    bme680.stop();

    // ----- TEMP EXPLANATION OF THE  MAIN PROGRAM
    // server.stop();
    // door.stop();

    // -- BME680 sensor setup --
    // Start up web server

    // Start threads for server vitals & light sensor & turn on barcode scanner

    // start of infinite loop
        // If Door Shut then
        // Turn barcode scanner off
        // Fridge vitals uploaded to web app using intervals
        
        // If Door Open then
        // Barcode scanner on, and able to scan and update the web app whenever
        // Fridge vitals uploaded to web app using intervals
    return 0;
}