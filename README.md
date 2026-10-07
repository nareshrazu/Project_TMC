# Project TMC - Smart Hydroponics & Automated Dosing Controller

![ESP32-S3](https://img.shields.io/badge/Board-ESP32--S3-blue.svg)
![PlatformIO](https://img.shields.io/badge/Environment-PlatformIO-orange.svg)
![Wokwi](https://img.shields.io/badge/Simulator-Wokwi-green.svg)
![Language](https://img.shields.io/badge/Code-C%2B%2B-00599C.svg)

An automated hydroponics monitoring and dosing system built on the **ESP32-S3 DevKitC-1**. Features non-blocking multi-sensor tracking, stage-based growth presets, a 5-actuator pump controller, integrated industrial safety watchdogs, and a dual-zone SSD1306 OLED interface.

---

## ⚡ Safety Watchdogs & Hardware Features

* **Master Kill Switch (GPIO 6):** Physical switch intercept that immediately turns off all pump actuators, turns off the status RGB LED, and halts software execution.
* **Water Fill Watchdog:** Auto-shuts off the water pump and enters a red lockout error state with an audio alarm if the float switch isn't triggered within the maximum time limit (`WATER_TIMEOUT`).
* **Relay Boot-Glitch Protection:** Pre-sets all pump control pins to `LOW` before initializing them as `OUTPUT`, preventing relay clicks during ESP32 startup.
* **Float Switch Debouncing:** Uses a 10-cycle consecutive read check to prevent relay chatter caused by water surface ripples.
* **Multi-Color RGB Status LED:**
  * 🟢 **Green:** Idle / System Monitoring
  * 🔵 **Blue:** Filling Water Reservoir
  * 🟡 **Yellow:** Active Nutrient or pH Dosing
  * 🔴 **Red:** System Lockout Error
* **Non-Blocking Architecture:** Asynchronous DS18B20 temperature reads and non-blocking `millis()` timing loops keep input responses instant.
* **Strict Error Lockout Recovery:** Prevents oscillating failure loops by clearing the dosing attempt counter only when both pH and EC values return safely inside their target deadbands.

---

### 📌 ESP32-S3 Pinout Alignment

| Hardware Module | ESP32-S3 Pin | Hardware Mode / Logic | Description |
| --- | --- | --- | --- |
| **OLED Display & RTC Module** | GPIO 8 (SDA), GPIO 9 (SCL) | I2C Channel (0x3C / 0x68) | Shared I2C Bus for 128x64 SSD1306 & DS3231 RTC |
| **SD Card Module (SPI)** | GPIO 12 (SCK), GPIO 13 (MISO), GPIO 11 (MOSI), GPIO 10 (CS)* | SPI Bus / Digital Output | MicroSD Card for CSV Telemetry Logging *(Note: Adjust pins if using custom hardware SPI mappings)* |
| **Master Power Switch** | GPIO 6 | Digital Input (`INPUT_PULLUP`) | Active LOW (GND Switched) |
| **Float Level Switch** | GPIO 5 | Digital Input (`INPUT_PULLUP`) | Active LOW (GND Switched) |
| **Temp Sensor (DS18B20)** | GPIO 4 | OneWire Bus | Requires 4.7kΩ pull-up to 3.3V |
| **Status RGB LED** | GPIO 15 (R), 16 (G), 17 (B) | Common Cathode Output | Active High |
| **Alarm Buzzer** | GPIO 18 | Piezo Output | 1kHz Tone Feedback |
| **Water Valve Pump** | GPIO 10 | Digital Output | Active High |
| **Tank N Pump (Pump A)** | GPIO 11 | Digital Output | Active High |
| **Tank P Pump (Pump B)** | GPIO 12 | Digital Output | Active High |
| **Tank K Pump (Pump C)** | GPIO 13 | Digital Output | Active High |
| **pH Down Pump** | GPIO 14 | Digital Output | Active High |
| **Target Adjust Buttons** | GPIO 41 (pH+), 42 (pH-), 45 (EC+), 46 (EC-) | Digital Input (`INPUT_PULLUP`) | Active LOW |
| **Growth Stage Buttons** | GPIO 39 (Seed), 40 (Veg), 21 (Bloom) | Digital Input (`INPUT_PULLUP`) | Active LOW |
| **Probe Sim Buttons** | GPIO 1 (pH+), 2 (pH-), 3 (EC+), 7 (EC-) | Digital Input (`INPUT_PULLUP`) | Active LOW (Diagnostic) |

---

# ESP32 Automated Hydroponic Dosing & Irrigation System

An enterprise-grade, state-machine-driven hydroponic management system built for ESP32-S3. Designed for precise nutrient dosing, automated multi-cycle irrigation, real-time RTC logging, and strict safety interlocks.

## 🚀 Key Features

* **Real-Time Clock (RTC) Integration:** Migrated away from volatile `millis()` uptime trackers to real-world timestamps for accurate historical auditing and CSV logging.
* **State-Aware Telemetry Stream:** 1-second continuous telemetry tracking that dynamically logs system states (`MIXING`, `DOSING`, `FILLING`, `WATERING`, or `METRIC_TICK`).
* **Intelligent Safety Interlocks:** Prevents irrigation from firing unless reservoir chemistry (pH and EC) is fully stabilized within defined deadband tolerances.
* **Non-Volatile Storage (NVS) Recovery:** Gracefully recovers manual overrides and active crop day state across power outages, with automatic memory clearing during test builds.
* **Automated Day Advancement:** Built-in testing hooks to seamlessly simulate multi-day crop cycles and dynamic nutrient profile transitions.
* **SD Card CSV Logging:** Standardized telemetry logging capturing structured comma-separated data for performance analysis and plant health tracking.

---

## 📊 CSV Telemetry Format

All logs written to the SD Card and Serial monitor follow this standardized schema:

```csv
Timestamp,Day,Phase,EventType,PumpName,DurationMs,Attempt,pH(Target),EC(Target),Temp

```

### Event Types:

* `PULSE_ON` / `PULSE_OFF`: Trigger events for individual dosing/irrigation pumps.
* `DOSING`: Active state snapshot while a nutrient or pH pump is running.
* `MIXING`: Active countdown state allowing the reservoir to stabilize.
* `WATERING`: Active state snapshot during irrigation cycles.
* `METRIC_TICK`: Standard heartbeat log during idle monitoring periods.

---

## 🛠️ Hardware Requirements

* **Microcontroller:** ESP32-S3 Development Board
* **Clock Source:** DS3231 or compatible Real-Time Clock (RTC) module via I2C
* **Storage:** MicroSD Card Module (SPI)
* **Actuators:** Relay-controlled peristaltic dosing pumps (Nutrient A/N, B/P, C/K, pH Down) and main irrigation pump.
* **Sensors:** pH sensor, EC (Electrical Conductivity) sensor, temperature sensor.

---

## ⚙️ Configuration & Modes

* **`TESTING_MODE` (`true`):** Accelerates test cycles, forces minimum 2-second pump pulses to guarantee telemetry capture, and wipes NVS memory on boot to prioritize `CropProfiles.h` updates.
* **Production Mode (`false`):** Enforces strict RTC-based midnight rollovers, precise millimeter-accurate micro-dosing profiles, and persistent NVS power-loss recovery.

---

## 🛠️ Software Prerequisites

* [Visual Studio Code](https://code.visualstudio.com/)
* **VS Code Extensions:**
  * [PlatformIO IDE](https://platformio.org/platformio-ide)
  * [Wokwi Simulator](https://wokwi.com/vscode)

### `platformio.ini` Dependencies

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
```

---

## 🚀 How to Build & Run

### Option 1: Wokwi Simulator in VS Code

1. Open the repository root folder in VS Code.
2. Open the Command Palette (`Ctrl+Shift+P` or `Cmd+Shift+P`).
3. Run **Wokwi: Start Simulator** (uses `diagram.json`).

### Option 2: Flash to Physical Hardware

1. Connect your ESP32-S3 board to your PC via USB.
2. Click **Build** and **Upload** in PlatformIO.
3. Open **Serial Monitor** at `115200` baud rate to inspect system telemetry.

---

## 📁 Repository Structure

```text
Project_TMC/
├── diagram.json          # Wokwi simulation diagram & pin connection map
├── platformio.ini        # PlatformIO environment & library configuration
├── include/
│   └── CropProfiles.h    # Target pH, EC, and dosing pulse datasets
├── src/
│   └── main.cpp          # Main control loops, state machine, and watchdogs
└── README.md             # Project documentation

```

```

<ElicitationsGroup message="What would you like to work on next for Project_TMC?">
  <Elicitation label="Review CropProfiles.h header code" query="Can you generate the matching CropProfiles.h file to ensure all crop profiles match main.cpp?"/>
  <Elicitation label="Create a standard .gitignore file" query="Can you create a standard .gitignore file for PlatformIO, VS Code, and C++ to keep the GitHub repo clean?"/>
</ElicitationsGroup>

```
