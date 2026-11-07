# Project TMC - Smart Hydroponics & Automated Dosing Controller

![ESP32-S3](https://img.shields.io/badge/Board-ESP32--S3-blue.svg)
![PlatformIO](https://img.shields.io/badge/Environment-PlatformIO-orange.svg)
![Wokwi](https://img.shields.io/badge/Simulator-Wokwi-green.svg)
![Language](https://img.shields.io/badge/Code-C%2B%2B-00599C.svg)

An automated hydroponics monitoring and dosing system built on the **ESP32-S3 DevKitC-1**. Features non-blocking multi-sensor tracking, stage-based growth presets, a 6-actuator pump controller, integrated industrial safety watchdogs, and a dual-zone SSD1306 OLED interface.

---

## ⚡ Safety Watchdogs & Hardware Features

* **ESP32 Task Watchdog Timer (TWDT):** Industrial hardware watchdog timer (15-second timeout with panic reset) monitoring firmware execution in `loop()`.
* **Per-Pump Maximum On-Time Cutoffs:** Independent physical runtime safety cutoff for each actuator pin (Nutrients: 10s, pH Down: 5s, Water Fill: `WATER_TIMEOUT + 5s`, Irrigation: max cycle + 30s) preventing continuous run overflows.
* **Master Kill Switch (GPIO 6):** Physical switch intercept that immediately turns off all pump actuators, turns off the status RGB LED, and halts software execution.
* **Water Fill Watchdog:** Auto-shuts off the water pump and enters a red lockout error state with an audio alarm if the float switch isn't triggered within the maximum time limit (`WATER_TIMEOUT`).
* **Relay Boot-Glitch Protection:** Pre-sets all pump control pins to `LOW` before initializing them as `OUTPUT`, preventing relay clicks during ESP32 startup.
* **Float Switch Debouncing:** Uses a 2,000 ms continuous timestamp check to prevent relay chatter caused by water surface ripples.
* **Multi-Color RGB Status LED:**
  * 🟢 **Green:** Idle / System Monitoring
  * 🔵 **Blue:** Filling Water Reservoir
  * 🟡 **Yellow:** Active Nutrient or pH Dosing
  * 🔴 **Red:** System Lockout Error / Tank Empty
* **Non-Blocking Architecture:** Asynchronous DS18B20 temperature reads and non-blocking `millis()` timing loops keep input responses instant.
* **Strict Error Lockout Recovery:** Prevents oscillating failure loops by clearing the dosing attempt counter only when both pH and EC values return safely inside their target deadbands.

---

### 📌 ESP32-S3 Pinout Alignment

| Hardware Module | ESP32-S3 Pin | Hardware Mode / Logic | Description |
| --- | --- | --- | --- |
| **OLED Display & RTC Module** | GPIO 8 (SDA), GPIO 9 (SCL) | I2C Channel (0x3C / 0x68) | Shared I2C Bus for 128x64 SSD1306 & DS3231 RTC (probed on boot) |
| **SD Card Module (SPI)** | GPIO 36 (SCK), GPIO 37 (MISO), GPIO 35 (MOSI), GPIO 38 (CS) | Hardware SPI Bus | MicroSD Card for CSV Telemetry Logging (enabled in `TESTING_MODE`) |
| **Master Power Switch** | GPIO 6 | Digital Input (`INPUT_PULLUP`) | Active LOW (GND Switched) |
| **Float Level Switch** | GPIO 5 | Digital Input (`INPUT_PULLUP`) | Active LOW (GND Switched, hardware interrupt on CHANGE) |
| **Temp Sensor (DS18B20)** | GPIO 4 | OneWire Bus | Requires 4.7kΩ pull-up to 3.3V |
| **Status RGB LED** | GPIO 15 (R), 16 (G), 17 (B) | Common Cathode Output | Active High |
| **Alarm Buzzer** | GPIO 18 | Piezo Output | 1kHz Tone Feedback |
| **Water Valve Pump** | GPIO 10 | Digital Output | Active High |
| **Tank N Pump (Pump A)** | GPIO 11 | Digital Output | Active High |
| **Tank P Pump (Pump B)** | GPIO 12 | Digital Output | Active High |
| **Tank K Pump (Pump C)** | GPIO 13 | Digital Output | Active High |
| **pH Down Pump** | GPIO 14 | Digital Output | Active High |
| **Irrigation Pump** | GPIO 47 | Digital Output | Active High (Main reservoir watering pump) |
| **Target Adjust Buttons** | GPIO 41 (pH+), 42 (pH-), 45 (EC+), 46 (EC-) | Digital Input (`INPUT_PULLUP`) | Active LOW |
| **Growth Stage Buttons** | GPIO 39 (Seed), 40 (Veg), 21 (Bloom) | Digital Input (`INPUT_PULLUP`) | Active LOW |
| **Probe Sim Buttons** | GPIO 1 (pH+), 2 (pH-), 3 (EC+), 7 (EC-) | Digital Input (`INPUT_PULLUP`) | Active LOW (Diagnostic) |

---

# ESP32 Automated Hydroponic Dosing & Irrigation System

An enterprise-grade, state-machine-driven hydroponic management system built for ESP32-S3. Designed for precise nutrient dosing, automated multi-cycle irrigation, real-time RTC logging, and strict safety interlocks.

## 🚀 Key Features

* **Real-Time Clock (RTC) Timestamp Tracking:** Stores a `start_epoch` timestamp in NVS on Day 1 and computes `currentDay = ((now.unixtime() - startEpoch) / 86400UL) + 1` dynamically, ensuring accurate day progression even across multi-day power cuts and reboot cycles.
* **State-Aware Telemetry Stream:** Periodic continuous telemetry tracking (1-second intervals in testing mode, 60-second intervals in production) logging system states (`MIXING`, `DOSING`, `FILLING`, `WATERING`, or `METRIC_TICK`).
* **Intelligent Safety Interlocks:** Prevents irrigation from firing unless reservoir chemistry (pH and EC) is fully stabilized within defined deadband tolerances (bypassed on flush/germination days).
* **Non-Volatile Storage (NVS) Recovery:** Gracefully recovers manual overrides and active crop day state across power outages, with automatic memory clearing during test builds.
* **Automated Day Advancement:** Built-in testing hooks to seamlessly simulate multi-day crop cycles and dynamic nutrient profile transitions.
* **SD Card CSV Logging:** Standardized numeric telemetry logging capturing structured comma-separated data for performance analysis and plant health tracking without duplicate headers.

---

## 📊 CSV Telemetry Format

All logs written to the SD Card and Serial monitor follow this standardized schema:

```csv
Timestamp,Day,Phase,EventType,PumpName,DurationMs,Attempt,pH,Target_pH,EC,Target_EC,TempC
```

### Event Types:

* `PULSE_ON` / `PULSE_OFF`: Trigger events for individual dosing/irrigation pumps.
* `DOSING`: Active state snapshot while a nutrient or pH pump is running.
* `MIXING`: Active countdown state allowing the reservoir to stabilize.
* `WATERING`: Active state snapshot during irrigation cycles.
* `METRIC_TICK`: Standard heartbeat log during idle monitoring periods.
* `IRRIGATION_ABORT`: Emitted when an active irrigation cycle is aborted due to chemistry or hardware fault.
* `TEST_DAY_ADVANCE`: Automated day advance marker in testing mode.

---

## 🛠️ Hardware Requirements

* **Microcontroller:** ESP32-S3 Development Board
* **Clock Source:** DS3231 Real-Time Clock (RTC) module via I2C (`0x68`)
* **Display:** SSD1306 128x64 OLED display via I2C (`0x3C`)
* **Storage:** MicroSD Card Module (SPI pins 35–38, used in `TESTING_MODE`)
* **Actuators:** Relay-controlled peristaltic dosing pumps (Nutrient A/N, B/P, C/K, pH Down), water refill valve, and main irrigation pump.
* **Sensors:** Dallas DS18B20 digital temperature probe, float level switch, analog pH probe, analog EC probe.

---

## ⚙️ Configuration & Modes

* **`TESTING_MODE` (`true`):** Accelerates test cycles, forces minimum 2-second pump pulses to guarantee telemetry capture, enables SD logging on pins 35–38, logs at 1 Hz, and wipes NVS memory on boot to prioritize `CropProfiles.h` updates.
* **Production Mode (`false`):** Enforces strict RTC timestamp-based day tracking, 60-second telemetry streaming, precise millimeter-accurate micro-dosing profiles, and persistent NVS power-loss recovery.

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
    adafruit/RTClib @ ^2.1.3
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
├── diagram.json                       # Wokwi simulation diagram & pin connection map
├── platformio.ini                     # PlatformIO environment & library configuration
├── include/
│   └── CropProfiles.h                 # 45-day coriander crop schedule & dosing pulse profiles
├── src/
│   └── main.cpp                       # Main control loops, state machine, watchdogs, and logging
├── VERIFIED_Coriander_and_Firmware.md # Verification report & deferred hardware roadmap
└── README.md                          # Project documentation
```
