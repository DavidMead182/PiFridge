# PiFridge — Real-Time Smart Fridge Inventory & Vitals Monitor
[![Instagram](https://img.shields.io/badge/Instagram-pi_fridge-E4405F?logo=instagram&logoColor=white)](https://www.instagram.com/pi_fridge)

PiFridge is a Raspberry Pi-powered system that scans food barcodes in real time and monitors fridge conditions (temperature & humidity) to maintain a live inventory and food safety alerts.

Designed as a real-time embedded system for Uni Coursework.

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

## Hardware Requirements

### Core Components

- Raspberry Pi 5
- Raspberry Pi Camera Module 3
- Temperature/Humidity sensor BME ...
- Optional LED for alerts


## Circuit Setup

< -- GET A SCHEMATIC OR A VIDEO OF SOME KIND -- >


## Software Requirements

- Raspberry Pi OS (64-bit recommended)
- C++
- Camera libraries
- Add more as we go


## Installation — Start to Finish
