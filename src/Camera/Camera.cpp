#include "Camera.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>
#include <unordered_set>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>
#include <tensorflow/lite/model.h>

namespace {
const std::unordered_set<std::string> kFoodLabels = {
    "banana",
    "apple",
    "sandwich",
    "orange",
    "broccoli",
    "carrot",
    "hot dog",
    "pizza",
    "donut",
    "cake"
};
}

namespace fs = std::filesystem;

Camera::Camera() : Camera(Config{}) {}

Camera::Camera(const Config& config)
    : config_(config) {
}

void Camera::registerCallback(Callback cb) {
    callback_ = std::move(cb);
}

void Camera::start() {
    if (running_) return;

    if (!ensureOutputDirectory()) {
        std::cerr << "[Camera] Failed to create output directory: "
                  << config_.image_output_dir << "\n";
        return;
    }

    if (config_.enable_object_detection) {
        initialiseObjectDetector();
    }

    running_ = true;
    thread_ = std::thread(&Camera::run, this);
}

void Camera::stop() {
    running_ = false;
    capture_requested_ = true;
    if (thread_.joinable()) thread_.join();
}

void Camera::setDoorOpen(bool isOpen) {
    door_open_ = isOpen;
    if (isOpen) capture_requested_ = true;
}

bool Camera::isDoorOpen() const {
    return door_open_.load();
}

void Camera::triggerCaptureNow() {
    capture_requested_ = true;
}

CameraSnapshot Camera::getLastSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_snapshot_;
}

void Camera::run() {
    while (running_) {
        if (door_open_.load() || capture_requested_.load()) {
            capture_requested_ = false;

            CameraSnapshot snapshot = processFrame();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                last_snapshot_ = snapshot;
            }

            writeSnapshotJson(snapshot);
            if (callback_) callback_(snapshot);
        }

        std::this_thread::sleep_for(config_.interval);
    }
}

CameraSnapshot Camera::processFrame() {
    CameraSnapshot snapshot;
    snapshot.timestamp = nowIso8601();
    snapshot.image_path = buildImagePath();

    const std::string captureCmd = replaceToken(config_.capture_command, "{image}", snapshot.image_path);
    if (std::system(captureCmd.c_str()) != 0) {
        std::cerr << "[Camera] Capture failed: " << captureCmd << "\n";
        return snapshot;
    }

    if (config_.enable_text_detection) {
        snapshot.text = runTextDetection(snapshot.image_path);
    }

    if (config_.enable_object_detection) {
        snapshot.objects = runObjectDetection(snapshot.image_path);
    }

    return snapshot;
}

bool Camera::ensureOutputDirectory() const {
    std::error_code ec;
    fs::create_directories(config_.image_output_dir, ec);
    return !ec;
}

bool Camera::initialiseObjectDetector() {
    detector_init_attempted_ = true;

    labels_ = loadLabels(config_.label_path);

    model_ = tflite::FlatBufferModel::BuildFromFile(config_.model_path.c_str());
    if (!model_) {
        std::cerr << "[Camera] Failed to load TFLite model: " << config_.model_path << "\n";
        detector_ready_ = false;
        return false;
    }

    tflite::ops::builtin::BuiltinOpResolver resolver;
    tflite::InterpreterBuilder builder(*model_, resolver);
    builder(&interpreter_);
    if (!interpreter_) {
        std::cerr << "[Camera] Failed to create TFLite interpreter\n";
        detector_ready_ = false;
        return false;
    }

    interpreter_->SetNumThreads(config_.num_threads);

    if (interpreter_->AllocateTensors() != kTfLiteOk) {
        std::cerr << "[Camera] Failed to allocate TFLite tensors\n";
        interpreter_.reset();
        model_.reset();
        detector_ready_ = false;
        return false;
    }

    detector_ready_ = true;
    return true;
}

std::string Camera::buildImagePath() const {
    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&tt, &tm);

    std::ostringstream oss;
    oss << config_.image_output_dir << "/frame_"
        << std::put_time(&tm, "%Y%m%d_%H%M%S")
        << ".jpg";
    return oss.str();
}

std::string Camera::nowIso8601() const {
    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&tt, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return oss.str();
}

std::string Camera::replaceToken(std::string src, const std::string& token, const std::string& value) const {
    size_t pos = 0;
    while ((pos = src.find(token, pos)) != std::string::npos) {
        src.replace(pos, token.size(), value);
        pos += value.size();
    }
    return src;
}

std::string Camera::execCommand(const std::string& command) const {
    std::array<char, 256> buffer{};
    std::string result;

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "[Camera] popen failed: " << command << "\n";
        return result;
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }

    pclose(pipe);
    return result;
}

std::string Camera::runTextDetection(const std::string& imagePath) const {
    const std::string command = replaceToken(config_.tesseract_command, "{image}", imagePath);
    return trim(execCommand(command));
}

std::vector<CameraDetection> Camera::runObjectDetection(const std::string& imagePath) {
    std::vector<CameraDetection> detections;

    if (!detector_ready_ && !detector_init_attempted_) {
        initialiseObjectDetector();
    }
    if (!detector_ready_ || !interpreter_) {
        return detections;
    }

    const int inputIndex = interpreter_->inputs()[0];
    const TfLiteTensor* inputTensor = interpreter_->tensor(inputIndex);
    if (!inputTensor || inputTensor->dims->size < 4) {
        std::cerr << "[Camera] Unexpected input tensor shape\n";
        return detections;
    }

    const int inputHeight = inputTensor->dims->data[1];
    const int inputWidth = inputTensor->dims->data[2];
    const int inputChannels = inputTensor->dims->data[3];

    cv::Mat image = cv::imread(imagePath, cv::IMREAD_COLOR);
    if (image.empty()) {
        std::cerr << "[Camera] Failed to read captured image: " << imagePath << "\n";
        return detections;
    }

    cv::Mat rgb;
    cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);

    cv::Mat resized;
    cv::resize(rgb, resized, cv::Size(inputWidth, inputHeight));

    if (inputChannels != 3) {
        std::cerr << "[Camera] Unsupported input channels: " << inputChannels << "\n";
        return detections;
    }

    if (inputTensor->type == kTfLiteUInt8) {
        std::memcpy(interpreter_->typed_tensor<uint8_t>(inputIndex),
                    resized.data,
                    static_cast<size_t>(inputWidth * inputHeight * inputChannels));
    } else if (inputTensor->type == kTfLiteFloat32) {
        float* input = interpreter_->typed_tensor<float>(inputIndex);
        for (int y = 0; y < inputHeight; ++y) {
            for (int x = 0; x < inputWidth; ++x) {
                const cv::Vec3b pixel = resized.at<cv::Vec3b>(y, x);
                const int base = (y * inputWidth + x) * inputChannels;
                input[base + 0] = static_cast<float>(pixel[0]) / 255.0f;
                input[base + 1] = static_cast<float>(pixel[1]) / 255.0f;
                input[base + 2] = static_cast<float>(pixel[2]) / 255.0f;
            }
        }
    } else {
        std::cerr << "[Camera] Unsupported input tensor type\n";
        return detections;
    }

    if (interpreter_->Invoke() != kTfLiteOk) {
        std::cerr << "[Camera] TFLite inference failed\n";
        return detections;
    }

    if (interpreter_->outputs().size() < 4) {
        std::cerr << "[Camera] Unexpected number of output tensors\n";
        return detections;
    }

    const float* boxes = interpreter_->typed_output_tensor<float>(0);
    const float* classes = interpreter_->typed_output_tensor<float>(1);
    const float* scores = interpreter_->typed_output_tensor<float>(2);
    const float* countPtr = interpreter_->typed_output_tensor<float>(3);

    if (!boxes || !classes || !scores || !countPtr) {
        std::cerr << "[Camera] Missing output tensors\n";
        return detections;
    }

    const int detectionCount = static_cast<int>(countPtr[0]);
    for (int i = 0; i < detectionCount; ++i) {
        const float score = scores[i];
        if (score < config_.confidence_threshold) continue;

        const int classIndex = static_cast<int>(classes[i]);
        const int labelIndex = classIndex + 1;
        CameraDetection det;
        if (labelIndex >= 0 && labelIndex < static_cast<int>(labels_.size())) {
            det.label = labels_[labelIndex];
        } else {
            det.label = "unknown";
        }

        if (det.label == "???" || det.label == "unknown") {
            continue;
        }

        if (kFoodLabels.find(det.label) == kFoodLabels.end()) {
            continue;
        }

        det.confidence = score;
        det.y_min = boxes[i * 4 + 0];
        det.x_min = boxes[i * 4 + 1];
        det.y_max = boxes[i * 4 + 2];
        det.x_max = boxes[i * 4 + 3];
        detections.push_back(det);
    }

    std::sort(detections.begin(), detections.end(), [](const CameraDetection& a, const CameraDetection& b) {
        return a.confidence > b.confidence;
    });

    return detections;
}

std::vector<std::string> Camera::loadLabels(const std::string& labelPath) const {
    std::vector<std::string> labels;
    std::ifstream in(labelPath);
    if (!in.is_open()) {
        std::cerr << "[Camera] Failed to open label file: " << labelPath << "\n";
        return labels;
    }

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;
        labels.push_back(line);
    }

    return labels;
}

void Camera::writeSnapshotJson(const CameraSnapshot& snapshot) const {
    std::ofstream out(config_.json_output_path);
    if (!out.is_open()) {
        std::cerr << "[Camera] Failed to write JSON: " << config_.json_output_path << "\n";
        return;
    }

    out << "{\n";
    out << "  \"timestamp\": \"" << escapeJson(snapshot.timestamp) << "\",\n";
    out << "  \"image_path\": \"" << escapeJson(snapshot.image_path) << "\",\n";
    out << "  \"text\": \"" << escapeJson(snapshot.text) << "\",\n";
    out << "  \"objects\": [\n";

    for (size_t i = 0; i < snapshot.objects.size(); ++i) {
        const auto& obj = snapshot.objects[i];
        out << "    {"
            << "\"label\": \"" << escapeJson(obj.label) << "\", "
            << "\"confidence\": " << obj.confidence << ", "
            << "\"y_min\": " << obj.y_min << ", "
            << "\"x_min\": " << obj.x_min << ", "
            << "\"y_max\": " << obj.y_max << ", "
            << "\"x_max\": " << obj.x_max
            << "}";
        if (i + 1 < snapshot.objects.size()) out << ",";
        out << "\n";
    }

    out << "  ]\n";
    out << "}\n";
}

std::string Camera::trim(const std::string& s) {
    const auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    const auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

std::string Camera::escapeJson(const std::string& s) {
    std::ostringstream oss;
    for (char c : s) {
        switch (c) {
            case '\\': oss << "\\\\"; break;
            case '"':  oss << "\\\""; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:   oss << c; break;
        }
    }
    return oss.str();
}
