# Project TMC - Smart Hydroponics & Automated Dosing Controller

![ESP32-S3](https://img.shields.io/badge/Board-ESP32--S3-blue.svg)
![PlatformIO](https://img.shields.io/badge/Environment-PlatformIO-orange.svg)
![Wokwi](https://img.shields.io/badge/Simulator-Wokwi-green.svg)
![Language](https://img.shields.io/badge/Code-C%2B%2B-00599C.svg)

An automated hydroponics monitoring and dosing system built on the **ESP32-S3 DevKitC-1**. This project features non-blocking multi-sensor tracking, stage-based growth presets, a 5-actuator pump dosing controller, and a dual-zone SSD1306 OLED interface with real-time live dosing timer metrics.

---

## ⚡ Key Features

* **Non-Blocking Architecture:** High-speed `millis()` timers eliminate input lag and UI latency.
* **Dual-Zone OLED Interface:** 
  * **Left Panel:** Live sensor telemetry (pH, EC, Temperature).
  * **Right Panel:** Live active dosing duration timer (in seconds) or the last recorded pump action.
* **5-Actuator Pump Controller:** Automated control lines for Water, Tank N, Tank P, Tank K, and pH Down.
* **Growth Stage Presets:** Hardware buttons for rapid selection of **Seed**, **Veg**, and **Bloom** profiles.
* **Safety System:** Hardware switch monitoring for main power cut-off, float/water level safety, and audio/visual alarm feedback.
* **Wokwi Simulation Ready:** Full VS Code Wokwi integration for instant hardware testing without physical wiring.

---

## 📌 ESP32-S3 Hardware Pinout Table

| Hardware Component | ESP32-S3 Pin | Signal / Notes |
| :--- | :--- | :--- |
| **SSD1306 OLED Display** | GPIO 8 (SDA), GPIO 9 (SCL) | I2C Display Channel (3.3V) |
| **DS18B20 Temp Probe** | GPIO 4 | OneWire Data (Requires 4.7kΩ Pull-Up) |
| **Main Power Switch** | GPIO 6 | System Power Intercept (Active Low) |
| **Water Level Float Switch** | GPIO 5 | Water Level Detection (Active Low) |
| **Status RGB LED** | GPIO 15 (R), 16 (G), 17 (B) | Common Cathode Output |
| **Piezo Buzzer** | GPIO 18 | Alarm Audio Output |
| **Pump Outputs (LEDs)** | GPIO 10 (Water), 11 (N), 12 (P), 13 (K), 14 (pH Down) | Active High Relay/Transistor Control |
| **Growth Stage Buttons** | GPIO 39 (Seed), 40 (Veg), 21 (Bloom) | Stage Selection Inputs |
| **Target Adjust Buttons**| GPIO 41 (pH+), 42 (pH-), 45 (EC+), 46 (EC-) | Target Calibration Inputs |
| **Simulated Probe Buttons**| GPIO 1 (pH+), 2 (pH-), 3 (EC+), 7 (EC-) | Diagnostic Probe Simulation Inputs |

---

## 🛠️ Software Setup & Prerequisites

### 1. Tools Required
* [Visual Studio Code](https://code.visualstudio.com/)
* [PlatformIO IDE Extension](https://platformio.org/platformio-ide)
* [Wokwi Simulator Extension](https://wokwi.com/vscode)

### 2. Project Configuration (`platformio.ini`)
Ensure your `platformio.ini` file in the project root includes the following settings:

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
lib_deps =
    adafruit/Adafruit SSD1306 @ ^2.5.7
    adafruit/Adafruit GFX Library @ ^1.11.5
    milesburton/DallasTemperature @ ^3.11.0
    paulstoffregen/OneWire @ ^2.3.7
