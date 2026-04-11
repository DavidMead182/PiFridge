#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/model.h>
#include <tensorflow/lite/kernels/register.h>

struct CameraDetection {
    std::string label;
    float confidence = 0.0f;
    float y_min = 0.0f;
    float x_min = 0.0f;
    float y_max = 0.0f;
    float x_max = 0.0f;
};

struct CameraSnapshot {
    std::string timestamp;
    std::string image_path;
    std::string text;
    std::vector<CameraDetection> objects;
};

class Camera {
public:
    using Callback = std::function<void(const CameraSnapshot&)>;

    struct Config {
        std::string image_output_dir = "/tmp/pifridge_frames";
        std::string json_output_path = "/tmp/fridge_camera.json";

        std::string capture_command =
            "rpicam-still -n --immediate --width 1280 --height 720 -o {image}";
        std::string tesseract_command =
            "tesseract {image} stdout --psm 6 2>/dev/null";

        std::string model_path = "/home/pifridge/PiFridge/src/Camera/detect.tflite";
        std::string label_path = "/home/pifridge/PiFridge/src/Camera/labelmap.txt";

        std::chrono::milliseconds interval{2000};
        float confidence_threshold = 0.50f;
        int num_threads = 2;
        bool enable_text_detection = true;
        bool enable_object_detection = true;
    };

    Camera();
    explicit Camera(const Config& config);

    void registerCallback(Callback cb);

    void start();
    void stop();

    void setDoorOpen(bool isOpen);
    bool isDoorOpen() const;

    void triggerCaptureNow();
    CameraSnapshot getLastSnapshot() const;

private:
    void run();
    CameraSnapshot processFrame();

    bool ensureOutputDirectory() const;
    bool initialiseObjectDetector();
    std::string buildImagePath() const;
    std::string nowIso8601() const;
    std::string replaceToken(std::string src, const std::string& token, const std::string& value) const;
    std::string execCommand(const std::string& command) const;
    std::string runTextDetection(const std::string& imagePath) const;
    std::vector<CameraDetection> runObjectDetection(const std::string& imagePath);
    std::vector<std::string> loadLabels(const std::string& labelPath) const;
    void writeSnapshotJson(const CameraSnapshot& snapshot) const;

    static std::string trim(const std::string& s);
    static std::string escapeJson(const std::string& s);

    Config config_;
    Callback callback_;

    mutable std::mutex mutex_;
    CameraSnapshot last_snapshot_;

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> door_open_{false};
    std::atomic<bool> capture_requested_{false};

    std::vector<std::string> labels_;
    bool detector_ready_ = false;
    bool detector_init_attempted_ = false;
    std::unique_ptr<tflite::FlatBufferModel> model_;
    std::unique_ptr<tflite::Interpreter> interpreter_;
};

#endif
