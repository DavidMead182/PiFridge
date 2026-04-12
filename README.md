# PiFridge — Real-Time Smart Fridge Inventory & Vitals Monitor
[![Instagram](https://img.shields.io/badge/Instagram-pi_fridge-E4405F?logo=instagram&logoColor=white)](https://www.instagram.com/pi_fridge)

TODO: <-- INSERT A LOGO HERE -->

PiFridge is a Raspberry Pi 5-powered real-time embedded system that scans food via barcodes or object detection and monitors fridge environmental conditions. This is to keep an accurate view of the contents of your fridge

Built as a real-time embedded systems project at the University of Glasgow.

## Demo

TODO: <-- insert Pictures of it --> and 3d model

## TODO: Table of Contents

## Bill of Matierials

**Total Amount: £119.36**

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
| Wires & breadboard    | —        | 1    | 9.99 | [LINK](https://www.amazon.co.uk/Breadboard-tie-Points-Flexible-U-Shape-JumperWires/dp/B0BMFXPSVG/ref=sxin_15_pa_sp_search_thematic_sspa?c=ts&content-id=amzn1.sym.39f3f322-e4f4-475f-ab3a-16807016655d%3Aamzn1.sym.39f3f322-e4f4-475f-ab3a-16807016655d&cv_ct_cx=Breadboards&keywords=Breadboards&pd_rd_i=B0BMFXPSVG&pd_rd_r=9d42dc0d-cbb2-49be-a3d2-94858a94e145&pd_rd_w=WXJJp&pd_rd_wg=RcQmz&pf_rd_p=39f3f322-e4f4-475f-ab3a-16807016655d&pf_rd_r=TMYDXR77S4XN4G5YAV79&qid=1776025404&s=industrial&sbo=RZvfv%2F%2FHxDF%2BO5021pAnSA%3D%3D&sr=1-3-ad3222ed-9545-4dc8-8dd8-6b2cb5278509-spons&ts_id=10256475031&aref=Iuoce5IY8t&sp_csd=d2lkZ2V0TmFtZT1zcF9zZWFyY2hfdGhlbWF0aWM&th=1) |

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
sudo apt install -y nginx libfcgi-dev cmake
```
 
### 3. Build & Run
 
```bash
chmod +x run.sh
./run.sh
```

Note: Tests covering direct GPIO writes and hardware-dependent classes are run separately via test executables in each subdirectory, as they require physical hardware to be connected.

## Documentation - update as we go

- [BH1750 Light Sensor](docs/BH1750.md)
- [BME680 Temp, Humidity and Pressure Sensor](docs/BME680.md)
- [XXXX Barcode Sensor](docs/Barcode.md)
- [XXXX Raspberry Pi Camera](docs/Barcode.md)
- [Main Program](src/README.md)


## Command For Dependencies

sudo apt install pkg-config libcurl4-openssl-dev build-essential cmake libfcgi-dev

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
| David Mead   | BME680 sensor class, Nginx & Webapp Startup & Integration with Main Program, Main Program Integration |
| Hamma Kalid   | BH1750 Light Sensor class |
| Patrick Dawodu   | Webapp design  |
| Ross Cameron   | Barcode sensor class, Main Program Integration, Code Refactoring |
| Ryan Ho   | Object & Text Detection Classes, Main Program Integration |

## License

### cppTimer
The Timer Wrapper C++ class was adopted from [Bernd Porr](https://github.com/berndporr/cppTimer).
 
*Last updated: 14/04/2026*


