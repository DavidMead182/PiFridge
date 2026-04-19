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
#include <sqlite3.h>
#include <ctime>
#include <unistd.h>

struct FridgeState {
    BME680Sample    vitals{};
    bool            door_open = false;
    std::mutex      mutex;
    double          lux = 0.0;
};

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

// Upserts a camera-detected item into inventory.
// Matches on name (no barcode for loose produce).
// Increments quantity if already exists, inserts new row if not.
static void addCameraItemToInventory(const std::string& name) {
    static const char* DB_PATH = "/var/lib/pifridge/inventory.db";

    sqlite3* db = nullptr;
    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        std::cerr << "[Camera] Failed to open inventory DB\n";
        return;
    }

    // Check if item already exists by name (no barcode for camera detections)
    const char* selectQuery =
        "SELECT id, quantity FROM inventory WHERE name = ? AND (barcode IS NULL OR barcode = '');";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, selectQuery, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[Camera] Failed to prepare select\n";
        sqlite3_close(db);
        return;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        // Exists — increment quantity
        int id  = sqlite3_column_int(stmt, 0);
        int qty = sqlite3_column_int(stmt, 1);
        sqlite3_finalize(stmt);

        const char* updateQuery = "UPDATE inventory SET quantity = ? WHERE id = ?;";
        sqlite3_stmt* upStmt = nullptr;
        if (sqlite3_prepare_v2(db, updateQuery, -1, &upStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(upStmt, 1, qty + 1);
            sqlite3_bind_int(upStmt, 2, id);
            sqlite3_step(upStmt);
            sqlite3_finalize(upStmt);
            std::cout << "[Camera] Incremented inventory for: " << name << "\n";
        }
    } else {
        // Does not exist — insert new row with empty barcode
        sqlite3_finalize(stmt);

        time_t now = time(nullptr);
        char dateBuf[11];
        strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", localtime(&now));

        const char* insertQuery =
            "INSERT INTO inventory (name, barcode, quantity, date_added) VALUES (?, '', 1, ?);";
        sqlite3_stmt* insStmt = nullptr;

        if (sqlite3_prepare_v2(db, insertQuery, -1, &insStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(insStmt, 1, name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(insStmt, 2, dateBuf,      -1, SQLITE_STATIC);
            sqlite3_step(insStmt);
            sqlite3_finalize(insStmt);
            std::cout << "[Camera] Added to inventory: " << name << "\n";
        }
    }

    sqlite3_close(db);
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

        std::cout << "[Barcode] Scanned: " << code << "\n";
        fetch_product(code);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        // Re-arm the scanner if the door is still open
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            if (state.door_open) {
                scanner.triggerScan();
            }
        }
    });

    scanner.start();

    // -----------------------------------------------------------------------
    // Camera setup - captures image when door is open, runs OCR and object detection
    // -----------------------------------------------------------------------

    // Create camera config, directories for saving captured images and JSON output
    Camera::Config cameraConfig;
    cameraConfig.image_output_dir = "/tmp/pifridge_frames";
    cameraConfig.json_output_path = "/tmp/fridge_camera.json";

    // Camera capture command
    // --zsl for zero shutter lag, better capture timing
    // Output sent to /dev/null to suppress logs
    cameraConfig.capture_command =
        "rpicam-still --zsl -n --immediate --width 1280 --height 720 -o {image} >/dev/null 2>&1";

    // OCR configuration (Tesseract)
    // --psm: 11 for sparse text, 6 for block of text, 7 for single line, 8 for single word.
    // Whitelist to focus on common expiry date characters
    cameraConfig.tesseract_command =
        "tesseract {image} stdout --psm 11 -c tessedit_char_whitelist=0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz/:.- 2>/dev/null";

    // Object detection model and label file paths
    cameraConfig.model_path = "/home/pifridge/PiFridge/src/Camera/detect.tflite";
    cameraConfig.label_path = "/home/pifridge/PiFridge/src/Camera/labelmap.txt";

    // Capture parameters
    cameraConfig.interval = std::chrono::milliseconds(200);
    cameraConfig.confidence_threshold = 0.7f;
    cameraConfig.num_threads = 2;

    Camera camera(cameraConfig);

    camera.registerCallback([&](const CameraEvent& event) {
        if (event.type == CameraEvent::Type::Object) {
            for (const auto& label : event.labels) {
                addCameraItemToInventory(label);
            }
        }

        if (event.type == CameraEvent::Type::Text) {
            std::cout << "Best before detected: " << event.text << "\n";
        }
    });

    // Start camera thread
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

        camera.setDoorOpen(isOpen); // tell camera bout the door state so it can trigger immediate capture

        if (isOpen) {
            std::cout << "[Door] Opened (lux=" << lux << ") - Barcode scanner ON, Camera ON\n";
            scanner.triggerScan();
            camera.triggerCaptureNow();
        } else {
            std::cout << "[Door] Closed (lux=" << lux << ") - Barcode scanner OFF, Camera OFF\n";
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
        pause();
    }

    //----- Clean shutdown -----
    std::cout << "\nPiFridge shutting down...\n";
    lightSensor.stop();
    bme680.stop();
    scanner.stop();
    camera.stop();

    return 0;
}