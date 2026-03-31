# PiFridge — Real-Time Smart Fridge Inventory & Vitals Monitor
[![Instagram](https://img.shields.io/badge/Instagram-pi_fridge-E4405F?logo=instagram&logoColor=white)](https://www.instagram.com/pi_fridge)

PiFridge is a Raspberry Pi-powered system that scans food barcodes in real time and monitors fridge conditions (temperature & humidity) to maintain a live inventory and food safety alerts.

Designed as a real-time embedded system for Uni Coursework.


## Command For Dependencies

sudo apt install pkg-config libgpiod-dev libcurl4-openssl-dev build-essential cmake libfcgi-dev

## Demo


## Key Features

- Real-time barcode scanning using Pi camera  
- Automatic product lookup via Open Food Facts API  
- Fridge vitals monitoring (temperature, humidity)  
- Door open detection  
- Event-driven architecture (threads, callbacks, timers)  
- Live inventory tracking  
- Alerts for unsafe conditions

## Motivation for PiFridge?

Food waste and food safety issues often happen because people forget what’s inside their fridge or whether it has been stored correctly.

PiFridge solves this by combining:

- barcode scanning
- environmental sensing
- real-time processing
- automated inventory tracking

The system instantly updates when:

- a product is scanned
- the fridge door opens
- temperature thresholds are exceeded

---

## Real-Time Requirements

| Event | Target Latency |
|------|---------------|
Barcode scan → item added | < TBC  
Door open detection | < TBC 
Temperature alert | < TBC  

Implemented using event-driven programming (callbacks, threads, timers) instead of polling where possible.

---

## Documentation - update as we go

- [BH1750 light sensor](docs/sensors.md)

---

## Hardware Requirements

### Core Components

- Raspberry Pi 5
- Raspberry Pi Camera Module 3
- Temperature/Humidity sensor BME680
- Optional LED for alerts


## Circuit Setup

<img width="1076" height="453" alt="image" src="https://github.com/user-attachments/assets/831f75c7-f635-493c-8019-81c1bb2bb99b" />

<img width="1055" height="581" alt="image" src="https://github.com/user-attachments/assets/ef9a75c9-923f-4a9a-ab19-531189d66777" />

## Software Requirements

- Raspberry Pi OS (64-bit recommended)
- C++
- Camera libraries
- Add more as we go


## Installation — Start to Finish
### 1. Clone the repo
 
```bash
git clone https://github.com/<your-username>/PiFridge.git
cd PiFridge
```
 
### 2. Install dependencies
 
```bash
sudo apt update
sudo apt install -y nginx libfcgi-dev cmake
```
 
### 3. Build
 
```bash
cmake -B build && cmake --build build
```
 
### 4. Set up nginx
 
Copy and configure the nginx site:
 
```bash
sudo cp config/pifridge.conf /etc/nginx/sites-available/pifridge
```
 
Open the file and update the `root` path to match where you cloned the repo:
 
```bash
sudo nano /etc/nginx/sites-available/pifridge
# Update: root /home/<your-username>/PiFridge/src/web_app;
```
 
Enable the site and disable the default:
 
```bash
sudo ln -s /etc/nginx/sites-available/pifridge /etc/nginx/sites-enabled/
sudo rm -f /etc/nginx/sites-enabled/default
sudo nginx -t
sudo systemctl reload nginx
```
 
Copy the web app files to the nginx serve directory:
 
```bash
sudo mkdir -p /var/www/pifridge
sudo cp src/web_app/index.html /var/www/pifridge/
sudo chown -R www-data:www-data /var/www/pifridge
```
 
### 5. Set up the FastCGI socket directory
 
```bash
sudo mkdir -p /var/run/pifridge
sudo chown <your-username>:www-data /var/run/pifridge
sudo chmod 770 /var/run/pifridge
```
 
---
 
## Running PiFridge
 
Three things need to be running at the same time. Open three terminals or use `&` to background processes.
 
**Terminal 1 — main sensor process** (requires sudo for I2C/GPIO):
 
```bash
sudo ./build/src/pifridge
```
 
This reads the BME680 and BH1750 sensors and writes fridge state to `/tmp/fridge_data.json`.
 
**Terminal 2 — FastCGI API server:**
 
```bash
./build/src/web_app/pifridge_api
```
 
Then fix the socket permissions so nginx can access it:
 
```bash
sudo chown <your-username>:www-data /var/run/pifridge/pifridge.sock
sudo chmod 660 /var/run/pifridge/pifridge.sock
```
 
**Terminal 3 — nginx** (should already be running, but if not):
 
```bash
sudo systemctl start nginx
```
 
Then open a browser on the Pi and go to:
 
```
http://localhost
```
 
---
 
## Architecture
 
```
pifridge (main)
  BME680 thread  ─┐
  BH1750 thread  ─┴─► /tmp/fridge_data.json
                              │
                    pifridge_api (FastCGI)
                      reads JSON on each request
                              │ unix socket
                            nginx
                              │ HTTP
                         index.html
                      polls /api/fridge every 1s
```
 
---
 
## File Structure
 
```
PiFridge/
├── config/
│   └── pifridge.conf        # nginx site config (template)
├── src/
│   ├── BME680/              # Temperature/humidity sensor
│   ├── BH1750/              # Light sensor + door detection
│   ├── common/              # Shared I2C utilities
│   ├── web_app/
│   │   ├── index.html       # Frontend — served by nginx
│   │   ├── pifridge_api.cpp # FastCGI endpoint
│   │   └── CMakeLists.txt
│   ├── main.cpp
│   └── CMakeLists.txt
├── docs/
├── CMakeLists.txt
└── README.md
```