#include "Camera.hpp"
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
                std::cout << "Object detected: " << label << "\n";
            }
        }
        if (event.type == CameraEvent::Type::Text) {
            std::cout << "Best before detected: " << event.text << "\n";
        }
    });

    bool isOpen = true; 
    camera.start();
    camera.setDoorOpen(isOpen);
    camera.triggerCaptureNow();

    while (!g_quit) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nShutting down camera...\n";
    camera.stop();
}