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
#include "BarcodeScanner.hpp"
#include "Camera.hpp"
#include <fstream>
#include <iomanip> 
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <algorithm>

struct FridgeState {
    BME680Sample    vitals{};
    bool            door_open = false;
    std::mutex      mutex;
    double          lux = 0.0;
};


// hANDSHAKE FUNCTION
// writes the JSON file that the API will serve to the PIFRIDGE app 
void saveStateToJson(const FridgeState& state) {
    // We create the file in the current working directory (usually /build)
    std::ofstream outFile("/tmp/fridge_data.json");
    if (outFile.is_open()) {
        outFile << "{\n"
                << "  \"temperature\": " << std::fixed << std::setprecision(2) << state.vitals.temperature_c << ",\n"
                << "  \"humidity\": " << state.vitals.humidity_rh << ",\n"
                << "  \"pressure\": " << state.vitals.pressure_hpa << ",\n"
                << "  \"lux\": " << state.lux << ",\n"
                << "  \"door_open\": " << (state.door_open ? "true" : "false") << "\n"
                << "}";
        outFile.close();
    }
}
// ---------------------------------------------------------------------------
// Signal handling - Ctrl+C shuts everything down cleanly
// ---------------------------------------------------------------------------
 
static std::atomic<bool> g_quit{false};
static void sigHandler(int) { g_quit = true; }

int main() {
    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    // -- Shared state --
    FridgeState state;

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

    // -- Shared state --
    
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
            saveStateToJson(state); // Export to JSON whenever climate updates
        }
        std::cout
            << "[BME680] "
            << "T="   << sample.temperature_c << "°C  "
            << "P="   << sample.pressure_hpa  << "hPa  "
            << "RH="  << sample.humidity_rh   << "%  "
            << "Gas=" << sample.gas_ohms      << "ohm\n";
    });

    BarcodeScanner scanner("/dev/ttyAMA0");

    scanner.registerCallback([&](const std::string& barcode) {
        std::string code = barcode;

        // Strip 5-byte hardware header
        if (code.size() <= 5) {
            std::cerr << "[Barcode] Too short\n";
            return;
        }
        code = code.substr(5);

        // Take the last 13 digits — real EAN-13 is always at the end
        if (code.size() > 13) {
            code = code.substr(code.size() - 13);
        }

        // Validate it's all digits
        if (code.find_first_not_of("0123456789") != std::string::npos) {
            std::cerr << "[Barcode] Non-digit characters found: " << code << "\n";
            return;
        }

        std::cout << "[Barcode] Scanned: " << code << "\n";
        fetch_product(code);
    });

    scanner.start();

    // Camera

    Camera::Config cameraConfig;
    cameraConfig.image_output_dir = "/tmp/pifridge_frames";
    cameraConfig.json_output_path = "/tmp/fridge_camera.json";
    // cameraConfig.capture_command =
    //     "rpicam-still -n --immediate --width 1280 --height 720 -o {image}";
    //--zsl for better image capture
    cameraConfig.capture_command =
        "rpicam-still --zsl -n --immediate --width 1280 --height 720 -o {image} >/dev/null 2>&1";
    
    // cameraConfig.tesseract_command =
    //     "tesseract {image} stdout --psm 6 2>/dev/null";

    //--psm 11 for sparse text, 6 for block of text, 7 for single line, 8 for single word. Can experiment for best results
    //focus on common expiry date characters
    cameraConfig.tesseract_command =
        "tesseract {image} stdout --psm 11 -c tessedit_char_whitelist=0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz/:.- 2>/dev/null";
    cameraConfig.model_path = "/home/pifridge/PiFridge/src/Camera/detect.tflite";
    cameraConfig.label_path = "/home/pifridge/PiFridge/src/Camera/labelmap.txt";
    cameraConfig.interval = std::chrono::milliseconds(2000);
    cameraConfig.confidence_threshold = 0.7f;
    cameraConfig.num_threads = 2;

    Camera camera(cameraConfig);

    camera.registerCallback([&](const CameraSnapshot& snapshot) {
        std::cout << "[Camera] image=" << snapshot.image_path << "\n";

        if (!snapshot.text.empty()) {
            std::cout << "[Camera] text=" << snapshot.text << "\n";
        }

        for (const auto& obj : snapshot.objects) {
            std::cout << "[Camera] object=" << obj.label
                    << " confidence=" << obj.confidence << "\n";
        }
    });

    camera.start();
    
    // -----------------------------------------------------------------------
    // BH1750 + DoorLightController - door detection
    // -----------------------------------------------------------------------

    DoorLightController doorController(30.0, 10.0);

    // Callback: fires only when door state CHANGES (open->closed or closed->open)
    doorController.registerDoorStateCallback([&](bool isOpen, double lux) {
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            state.door_open = isOpen;
            state.lux = lux;
            saveStateToJson(state); // Export to JSON whenever door state changes
        }

        camera.setDoorOpen(isOpen);
 
        if (isOpen) {
            std::cout << "[Door] Opened (lux=" << lux << ") - Barcode scanner ON\n";
            scanner.triggerScan();
            camera.triggerCaptureNow();
        } else {
            std::cout << "[Door] Closed (lux=" << lux << ") - Barcode scanner OFF\n";
            scanner.stopScan();
        }
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
    scanner.stop();
    camera.stop();

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