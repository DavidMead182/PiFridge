# PiFridge — Real-Time Smart Fridge Inventory & Vitals Monitor
[![Instagram](https://img.shields.io/badge/Instagram-pi_fridge-E4405F?logo=instagram&logoColor=white)](https://www.instagram.com/pi_fridge)

![pifridge_logo](https://github.com/user-attachments/assets/92124dd9-67de-48a7-8908-be75254f832a)

PiFridge is a Raspberry Pi 5-powered real-time embedded system that scans food via barcodes or object detection and monitors fridge environmental conditions. This is to keep an accurate view of the contents of your fridge

Built as a real-time embedded systems project at the University of Glasgow.

## Demo

https://github.com/user-attachments/assets/58a5558d-a3d2-4910-9e7c-18d99d581527

### 3D Model
[Link to Fusion Files](3D_Model)

<img width="250" alt="image" src="https://github.com/user-attachments/assets/b929943d-58ae-4886-97f6-aacb84f34ed2" />
<img width="250" alt="image" src="https://github.com/user-attachments/assets/533ffd28-ffdd-4b51-84ae-7e644565bd23" />
<img width="250" alt="image" src="https://github.com/user-attachments/assets/849aa461-8347-4353-b137-0b18dcc71159" />




## Table of Contents
- [Bill of Materials](#bill-of-materials)
- [Circuit Setup](#circuit-setup)
- [Installation](#installation--clone-to-running-program)
- [Documentation](#documentation---update-as-we-go)
- [Latency Timings](#latency-timings)
- [Social Media](#social-media)
- [Acknowledgements](#acknowledgements)
- [Authors & Contributions](#authors--contributions)
- [Future Work](#future-work)
- [License](#license)

## Bill of Materials

**Total Amount: £119.36**

During this project only £42.17 was spent as raspberry pi, pi camera and breadboard & wires were provided thanks to Glasgow Uni

### Controller

| Microcontroller     | Quantity | Cost (£) | Link |
|---------------------|----------|----------|------|
| Raspberry Pi 5      | 1        | 43.20    | [LINK](https://thepihut.com/products/raspberry-pi-5) |

### Sensors

| Component                        | Quantity | Cost (£) | Link |
|----------------------------------|----------|----------|------|
| Raspberry Pi Camera Module 3     | 1        | 24   | [LINK](https://thepihut.com/products/raspberry-pi-camera-module-3?src=raspberrypi) |
| BME680 – Temperature/Humidity/Gas Sensor | 1 | 8.71  | [LINK](https://shop.watterott.com/BME680-Breakout-Qwiic-Luftfeuchtigkeits-Druck-Temperatur-Luftguetesensor) |
| BH1750 – Ambient Light Sensor    | 1        | 3.62   | [LINK](https://cpc.farnell.com/dfrobot/sen0097/fermion-bh1750-light-sensor-breakout/dp/SC21071) |
| Barcode Scanner     | 1        | 29.84  | [LINK](https://www.waveshare.com/barcode-scanner-module.htm) |

### Miscellaneous

| Component                        | Quantity | Cost (£) | Link |
|----------------------------------|----------|----------|------|
| Wires & breadboard    |  1    | 9.99 | [LINK](https://www.amazon.co.uk/Breadboard-tie-Points-Flexible-U-Shape-JumperWires/dp/B0BMFXPSVG/ref=sxin_15_pa_sp_search_thematic_sspa?c=ts&content-id=amzn1.sym.39f3f322-e4f4-475f-ab3a-16807016655d%3Aamzn1.sym.39f3f322-e4f4-475f-ab3a-16807016655d&cv_ct_cx=Breadboards&keywords=Breadboards&pd_rd_i=B0BMFXPSVG&pd_rd_r=9d42dc0d-cbb2-49be-a3d2-94858a94e145&pd_rd_w=WXJJp&pd_rd_wg=RcQmz&pf_rd_p=39f3f322-e4f4-475f-ab3a-16807016655d&pf_rd_r=TMYDXR77S4XN4G5YAV79&qid=1776025404&s=industrial&sbo=RZvfv%2F%2FHxDF%2BO5021pAnSA%3D%3D&sr=1-3-ad3222ed-9545-4dc8-8dd8-6b2cb5278509-spons&ts_id=10256475031&aref=Iuoce5IY8t&sp_csd=d2lkZ2V0TmFtZT1zcF9zZWFyY2hfdGhlbWF0aWM&th=1) |

## Circuit Setup
### Diagram
<img width="1261" height="623" alt="image" src="https://github.com/user-attachments/assets/64a72e70-ce40-4763-bb38-6fcf1cf4c318" />

### Real Life Setup
![WhatsApp Image 2026-04-12 at 21 28 18](https://github.com/user-attachments/assets/6dc8ac35-a8f4-4e9a-965c-3e4624887d54)

## Installation — Clone to Running Program
### 1. Clone the repo
 
```bash
git clone https://github.com/DavidMead182/PiFridge.git
cd PiFridge
```
 
### 2. Install dependencies
 
```bash
sudo apt update
sudo apt install -y nginx libfcgi-dev cmake pkg-config libcurl4-openssl-dev build-essential 
```
 
### 3. Build & Run

#### Option A — Using `run.sh` (Recommended)
 
`run.sh` handles building, permissions, socket setup, and starting all processes in the correct order.
 
> **Note:** Open `run.sh` and set `PI_USER` at the top to your Raspberry Pi username if it differs from `pifridge`.
 
```bash
chmod +x run.sh
./run.sh
```
 
Once running, the terminal will print the URL to open in your browser:
 
```
===========================================
  PiFridge is running!
  Open in your browser:
    On this Pi:          http://localhost
    From another device: http://<Pi's IP>
===========================================
```
 
Stop all processes with `Ctrl+C`.
 
> **Note:** Tests covering direct GPIO writes and hardware-dependent classes are run separately via test executables in each subdirectory, as they require physical hardware to be connected.
 
---
 
#### Option B — Manual Steps
 
If you prefer to run each step yourself, follow the sequence below. Replace `pifridge` with your Raspberry Pi username throughout.
 
**1. Build**
 
```bash
cmake -B build .
cmake --build build
```
 
**2. Copy web app files to nginx serve directory**
 
```bash
sudo mkdir -p /var/www/pifridge
sudo cp src/web_app/index.html /var/www/pifridge/
sudo chown -R www-data:www-data /var/www/pifridge
```
 
**3. Create socket directory with correct permissions**
 
```bash
sudo mkdir -p /var/run/pifridge
sudo chown pifridge:www-data /var/run/pifridge
sudo chmod 770 /var/run/pifridge
```
 
**4. Create database directory**
 
```bash
sudo mkdir -p /var/lib/pifridge
sudo chown pifridge:pifridge /var/lib/pifridge
```
 
**5. Start nginx**
 
```bash
sudo systemctl start nginx
```
 
**6. Kill any existing PiFridge processes (if restarting)**
 
```bash
pkill -x pifridge_api       2>/dev/null || true
pkill -x pifridge_inventory 2>/dev/null || true
sleep 0.5
```
 
**7. Start the vitals FastCGI API**
 
```bash
./build/src/web_app/pifridge_api &
```
 
Then fix socket permissions:
 
```bash
sleep 1
sudo chown pifridge:www-data /var/run/pifridge/pifridge.sock
sudo chmod 660 /var/run/pifridge/pifridge.sock
```
 
**8. Start the inventory FastCGI API**
 
```bash
./build/src/web_app/pifridge_inventory &
```
 
Then fix socket permissions:
 
```bash
sleep 1
sudo chown pifridge:www-data /var/run/pifridge/pifridge_inventory.sock
sudo chmod 660 /var/run/pifridge/pifridge_inventory.sock
```
 
**9. Start the main sensor process**
 
```bash
sudo ./build/src/pifridge
```
 
### 4. Open Website
 
**On the Raspberry Pi:**
 
```
http://localhost
```
 
**From another device on the same WiFi:**
 
```
http://<IP address of Raspberry Pi>
```
 
To find the Pi's IP address:
 
```bash
hostname -I
```

## Documentation - update as we go

- [BH1750 Light Sensor](src/BH1750/README.md)
- [BME680 Temp, Humidity and Pressure Sensor](src/BME680/README.md)
- [Barcode Sensor](src/BarcodeScanner/README.md)
- [Raspberry Pi Camera](src/Camera/README.md)
- [NGINX Config](config/README.md)
- [Web App](src/web_app/README.md)
- [Common Includes](src/common/README.md)
- [Main Program](src/README.md)

## Latency Timings

| Sensor | Program | Time |
|-------|---------|------|
| BH1750 | <TODO: Demo> | XXms |
| BH1750 | main.cpp | XXms |
| BME680 | <TODO: Demo> | XXms |
| BME680 | main.cpp | XXms |
| Barcode Scanner  | <TODO: Demo> | XXms |
| Barcode Scanner | main.cpp | XXms |
| Pi Camera  | <TODO: Demo> | XXms |
| Pi Camera | main.cpp | XXms |

Note: Demo is the program that only runs that sensor code to get latency (scan to console), and main.cpp is the full integration, so from scan to display is the latency

## Social Media
 
| Platform   | Handle / Link          |
|------------|------------------------|
| Instagram  | [@pi_fridge](https://www.instagram.com/pi_fridge) |

## Acknowledgements

We would like to thank **Dr. Bernd Porr** for supervising this project and for providing the foundational teaching material that underpins the real-time architecture of PiFridge. His course notes, [*Realtime Embedded Coding in C++ under Linux*](https://berndporr.github.io/realtime_cpp_coding/), were an essential reference throughout development — covering event-driven design, callback-based programming, threading, and GPIO interfacing on the Raspberry Pi under Linux.
 
We would also like to thank **Dr. Chongfeng Wei** for his lectures on software engineering, which informed how we structured, tested, and managed the codebase across the team.
 
Finally, we extend our thanks to the technicians for their support with hardware and resources.

## Authors & Contributions
 
| Name              | Contributions                                         |
|-------------------|-------------------------------------------------------|
| David Mead   | BME680 sensor class, Nginx & Webapp Startup & Integration with Main Program, Main Program Integration, documentaton, and 3D model design |
| Hamna Khalid   | BH1750 light sensor module, Code Refactoring, door state controller logic, CMake test restoration, Raspberry Pi hardware validation of the BH1750 sensor path and combined BH1750/BME680 sensor setup in the lab, and BH1750 module documentation  |
| Patrick Dawodu   | Webapp design, and Nginx configuration  |
| Ross Cameron   | Barcode sensor class, Main Program Integration, Code Refactoring |
| Ryan Ho   | Object & Text Detection Classes, Main Program Integration |

## Future Work

## License

### cppTimer
The Timer Wrapper C++ class was adopted from [Bernd Porr](https://github.com/berndporr/cppTimer).
 
*Last updated: 14/04/2026*


