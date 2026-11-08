# Verified Coriander Agronomy & Firmware Review Report

## Executive Summary

This report documents the verification and completion status of the automated hydroponics control system for Coriander (*Coriandrum sativum*) running on the ESP32-S3 platform. All software and firmware logic items have been resolved and verified through compilation tests. All items requiring physical circuit alterations or peripheral expansion have been deferred to the hardware revision phase and explicitly catalogued.

---

## 1. Firmware Logic & Safety Items (Software Phase)

| Item ID | Description | Status | Verification Detail |
|---|---|---|---|
| **2.1** | **RTC Day Counting Across Power Outages** | ✅ FIXED | `rtcAvailable` tracked on boot; `start_epoch` stored in NVS on Day 1; `currentDay` calculated dynamically from elapsed real seconds (`((now.unixtime() - startEpoch) / 86400UL) + 1`); fallback safe time provider prevents invalid RTC readings from crashing or advancing days. |
| **2.2** | **High-Temperature Dosing Lockout** | ✅ FIXED | Temperature validation split into `tempSensorConnected` (-100 °C to 85 °C) and `tempHighAlarm` (> 24 °C). Dosing is blocked only when sensor is disconnected; high water temperature triggers visual OLED indicator (`!`) and non-blocking audible warning while allowing dosing and irrigation to proceed. |
| **2.3** | **False-Positive OLED Detection & Unguarded OFF Screen** | ✅ FIXED | I2C address `0x3C` explicitly probed via `Wire.beginTransmission(0x3C)` before calling `display.begin()`; SYSTEM OFF screen routine guarded with `if (oledAvailable)`. |
| **2.4** | **Incomplete SD Logging, Header Duplication & Numeric CSV** | ✅ FIXED | SD card SPI initialization guarded by `#if TESTING_MODE`; CSV header written only if `!SD.exists("/data.csv")`; irrigation start, stop, abort, and day-advance events invoke `writeToSD()`; CSV schema standardized to 12 raw numeric columns; production telemetry interval set to 60,000 ms. |
| **2.5** | **Germination Phase Naming, [FLUSH] Badge & pH Lockout** | ✅ FIXED | Days 1–3 in `CropProfiles.h` renamed to `"Germination"` with pure water comments; `[FLUSH]` badge suppressed during Germination; pH dosing bypassed when `isCurrentlyFlushing || targetEC == 0.0` preventing false 15-attempt lockout on pure water. |
| **2.6** | **Loop-Count Float Switch Debounce** | ✅ FIXED | Replaced fast loop counter with a physical 2,000 ms continuous timestamp check (`millis() - floatFullStartTime >= 2000`) before turning off `PIN_PUMP_WATER`. |
| **3.1** | **Task Watchdog & Per-Pump Max On-Time** | ✅ FIXED | Integrated ESP32 Task Watchdog Timer (`esp_task_wdt_init(15, true)` and `esp_task_wdt_reset()`); added independent `checkPumpRunTimeSafety()` monitoring physical pin states and forcing emergency shutdown and `dosingError` if any pump exceeds its maximum runtime. |
| **3.2** | **Documentation & Pinout Sync** | ✅ FIXED | `README.md` updated with exact SPI SD pins (35–38), irrigation pump (GPIO 47), TWDT safety, 12-column CSV schema, and NVS epoch recovery; deferred hardware items clearly designated. |

---

## 2. Deferred Hardware Items (Hardware Revision Phase)

The following items are deferred from the current firmware-only pass and will be integrated during physical PCB/hardware revision:

| Item | Status | Hardware Phase Requirement & Design Note |
|---|---|---|
| **Isolated Probes** | 🔸 PLANNED (Hardware Phase) | Galvanic isolation (e.g., ADuM1201 or isolated DC-DC converter like B0505S + I2C isolator) required between analog pH and EC probes to eliminate ground loop voltage bleed and sensor crosstalk in shared nutrient water. |
| **EC Temperature Compensation** | 🔸 PLANNED (Hardware Phase) | Implement 25 °C EC normalization formula (`compensatedEC = rawEC / (1.0 + 0.02 * (temp - 25.0))`) upon integration of the calibrated analog EC circuit. |
| **BLE Mobile Companion App** | 🔸 PLANNED (Hardware Phase) | Bluetooth Low Energy telemetry broadcasting and target configuration interface for field monitoring without opening terminal. |
| **Multi-Float Plumbing** | 🔸 PLANNED (Hardware Phase) | Dual-float plumbing configuration (separate HIGH refill shutoff float and LOW reservoir empty interlock float) to replace single-float hysteresis. |
| **External Environment Control** | 🔸 PLANNED (Hardware Phase) | Microclimate tracking and control: ambient temperature, relative humidity, vapor pressure deficit (VPD), and 12h–14h photoperiod light automation to prevent coriander bolting. |

---

## 3. Coriander 45-Day Agronomic Schedule Summary

```text
Days 1–3:   Germination (Pure water only, no nutrients, no pH dosing, 2x 1-min watering)
Days 4–7:   Early Seedling (Pure water, taproot moisture retention, 2x 1-min watering)
Days 8–14:  Late Seedling (Introduction of light nutrients: EC 0.6, pH 6.0, 3x 3-min watering)
Day 15:     Transition Veg (Nutrient bump: EC 1.0, pH 6.0, 4x 5-min watering)
Days 16–25: Early Vegetative (High Nitrogen: EC 1.2, pH 6.1, 5x 8-min watering)
Days 26–35: Late Vegetative (Canopy expansion: EC 1.5, pH 6.2, 6x 10-min watering)
Days 36–45: Mature Growth (Essential oil density: EC 1.8, pH 6.2, 8x 10-min watering)
```
