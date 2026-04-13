# Raspberry Pi Camera Object and Text Detection Documentation

This guide explains how to set up the PiFridge camera stack for:
- real-time object detection (TensorFlow Lite + OpenCV)
- text extraction from captured images (Tesseract OCR)

## System Requirements

- Raspberry Pi 5
- Raspberry Pi OS Trixie (Debian 13)
- Raspberry Pi Camera Module 3

## Connect the Camera Module

Connect the camera before powering on the Raspberry Pi.

- Ensure the ribbon cable is aligned and fully seated.
- If you connect the camera while the Pi is already running, reboot the system.

## Install Dependencies

### 1) Update system packages

```bash
sudo apt update
sudo apt upgrade -y
```

### 2) Install build and camera dependencies

```bash
sudo apt install -y \
	git meson ninja-build cmake pkg-config \
	libcamera-dev libdrm-dev libepoxy-dev \
	libjpeg-dev libpng-dev libtiff-dev libexif-dev \
	libboost-program-options-dev \
	libopencv-dev
```

These packages support object detection with libcamera, TensorFlow Lite, and OpenCV rendering.

### 3) Install OCR dependencies

```bash
sudo apt install -y tesseract-ocr tesseract-ocr-eng
```

## Build rpicam-apps With TensorFlow Lite and OpenCV

Run these steps in your local rpicam-apps source directory.

### 1) Clean old build artifacts

```bash
rm -rf build
```

### 2) Configure build options

```bash
meson setup build \
	-Denable_libav=disabled \
	-Denable_drm=enabled \
	-Denable_egl=enabled \
	-Denable_qt=disabled \
	-Denable_opencv=enabled \
	-Denable_tflite=enabled \
	-Denable_hailo=disabled
```

### 3) Compile

```bash
meson compile -C build -j 1
```

### 4) Install binaries

```bash
sudo meson install -C build
sudo ldconfig
```

## Download the Object Detection Model

Download Google MobileNet v1 SSD from:

https://storage.googleapis.com/download.tensorflow.org/models/tflite/coco_ssd_mobilenet_v1_1.0_quant_2018_06_29.zip

Extract it to a directory on your Pi, then open object_detect_tf.json and verify that:
- model_file points to your .tflite model path
- labels_file points to your label map path

## Run Object Detection Preview (Test)

From the directory containing object_detect_tf.json:

```bash
rpicam-hello \
	--timeout 0 \
	--post-process-file object_detect_tf.json \
	--lores-width 400 \
	--lores-height 300
```

If setup is correct, camera preview should show bounding boxes and confidence scores for detected objects.

## Run Text Detection Test

Capture an image:

```bash
rpicam-still -o text.jpg
```

Run OCR:

```bash
tesseract text.jpg stdout
```

## Reference

Raspberry Pi object detection documentation:
https://www.raspberrypi.com/documentation/computers/camera_software.html#post-processing-with-tensorflow-lite