# Project TMC — Validation of Latest Code (v3)

*Validated 2026-11-20 against the zip `Project_TMC-project_tmc_v2` (main.cpp 1,093 lines, files dated 8 Nov). Checked against the open items in `VALIDATION_of_Fixes.md` and the claims in `VERIFIED_Coriander_and_Firmware.md`. An independent reviewer re-checked every verdict line by line. Line numbers refer to `src/main.cpp`.*

> **Limits of this check:** I validated by reading the code; I couldn't compile it here (no network route to PlatformIO, and it isn't installed on your PC). `TESTING_MODE` is `true` (L27), so the production branch has only been read, never run. The included `Full_Irrigation_Data.txt` comes from the **older** firmware (old CSV format with `pH:7.00(T:6.00)` and "Seedling D1"), so it isn't evidence for this code.

## Verdict

**Major improvement.** Of the 9 items in the previous priority list, **5 are fully fixed, 3 are partly fixed and 1 is not fixed**. All 6 "claimed fixed but not in code" items from last time are now genuinely in the code. The remaining problems are mostly new edge cases introduced by the new features, plus one false ✅ in the md.

| Previous priority item | Status |
|---|---|
| 1. Watering must not depend on chemistry | ✅ Fixed |
| 2. Production irrigation timing + daylight window | ⚠️ Mostly (day boundary and no-RTC issues, see E) |
| 3. Button lockout clear / no dosing during watering | ✅ Fixed |
| 4. >24 °C as alarm, not a stop | ✅ Fixed |
| 5. Manual override in NVS, separate pH/EC | ✅ Fixed |
| 6. Days from a stored start date | ⚠️ Partly (not midnight-aligned, RTC reset bug, no RTC fallback) |
| 7. Dose by calibrated volume | ⚠️ Partly (placeholder constants, overdose risk, see D) |
| 8. Watchdog + per-pump max on-time | ✅ Fixed (but see B) |
| 9. md and README accuracy | ❌ Not fixed (claim 2.5 is false; see A, F) |

---

## 1. Confirmed fixed (✅)

- **Survival mode (L892, L694).** A dosing lockout now stops only the dosing pumps (`turnOffDosingPumps`), and irrigation is held only while dosing is genuinely in progress. With no sensors, watering resumes after the 15-attempt lockout (~1.6 h).
- **2 s watering (L946–961).** Now inside `#if TESTING_MODE`. Production uses `cycleDurationMins`, with cycles spread over 06:00–18:00. The stop is checked before the window, so a cycle crossing 18:00 still shuts off.
- **Button lockout clear.** Removed (L585–592). Only the power switch clears the lockout.
- **ISR (L185–192).** Now only sets the flag; no `digitalWrite`.
- **Manual override.** Separate pH and EC flags, saved to and restored from NVS (L464–465, L497–502, L599…). The stage buttons return to the profile.
- **Dosing during watering.** Dosing won't start while `isIrrigating` (L723). An aborted cycle is refunded (L911–914).
- **Temperature.** Above 24 °C is now an alarm only (OLED "!" plus a chirp). A disconnected sensor (≤−100 or ≥85) blocks dosing only.
- **OLED.** Real I2C probe (L426–427); the OFF screen is guarded (L537).
- **Logging.** Header written only for a new file, events go to SD, pH/EC columns are numeric, SD is used in testing only, and production telemetry is every 60 s.
- **Float switch.** 2 s time-based debounce on the "full" side (L783–794).
- **Watchdog.** Task watchdog (L385–386, L517). Per-pump maximum on-time with emergency cutoff (L281–319). `digitalRead` on an OUTPUT pin works on ESP32, and the tracker logic is correct.
- **No deadlock.** We traced every combination of states; irrigation and dosing never wait on each other forever.
- **README.** Pins, GPIO 47, watchdog and epoch logic documented.

---

## 2. Remaining issues

### A. Claim 2.5 is false ❌ (md accuracy)
The md says "pH dosing bypassed when `isCurrentlyFlushing || targetEC == 0.0`". That condition doesn't exist in the code. pH dosing (L756) and `phNeedsDosing` (L889) have no flush check, so pH-down still runs during germination.
- With tap water this is mostly harmless.
- With RO or soft water (no buffering), pH-down can push pH below 4.5. There is no pH-up pump, and `toxicBurnRisk` (L895) then **blocks watering permanently**.
- **Fix:** add `!isCurrentlyFlushing &&` to L756 and L889, or correct the md.

### B. Watchdog reboot loop while dumping the SD log (testing mode, real hardware) 🔴
- `dumpSDCardToSerial()` (L204–217) never feeds the watchdog, and `data.csv` is no longer cleared.
- At about 100 bytes/s of logging, the file passes 15 s of transfer at 115200 baud after about **30 min** of test running. A 15 s dump trips the watchdog.
- `logDumped` lives in RAM, so after the reboot (switch still OFF) it dumps again and resets again: **a reboot loop**.
- **Fix:** call `esp_task_wdt_reset()` inside the dump loop, or dump in chunks across loop passes.
- **Portability:** `esp_task_wdt_init(15, true)` is the arduino-esp32 2.x / IDF 4.4 API (it correctly reconfigures the already-running watchdog there). On core 3.x / IDF 5 it **won't compile**. `platformio.ini` has an unpinned `platform = espressif32` and no `lib_deps` versions, although the README shows pinned versions. Pin both.

### C. `toxicBurnRisk` can stop watering for days 🔴
L895 blocks watering when EC > target + 0.8 or pH < 4.5 / > 8.0.
- Days 8–14 have a target of 0.6, so the threshold is **1.4**. Hard or borewell water in India is often EC 1–2, so seedlings would get no water with no automatic way to dilute.
- Plants without water die faster than plants watered at EC 2.
- **Fix:** treat EC above the target band as an alarm only. Block watering only above an absolute limit (e.g. EC > 3.5 or pH < 4.0) or after a sustained period.

### D. Proportional dosing can empty the bottles 🔴
- **The 8 s clamp breaks the N:P:K ratio.** On day 8 from EC 0, the code computes 48 ml (32 s), split 12/12/8 s and clamped to **8/8/8 s**. The profile's N:P:K ratio is lost.
- **Overdose ceiling.** Up to 12 ml per pump per attempt × 15 attempts = **180 ml per pump**, but the bottles hold 100 ml. Each attempt doses 100% of the deficit with no safety margin, so any error in `EC_RISE_PER_ML_L` overshoots.
- **No response check.** There's no check that EC actually rose after a dose. A stuck-low probe, or a production build flashed today (EC fixed at 0.0), **will dump the bottles**.
- **Constants are placeholders.** `RESERVOIR_LITERS 20`, `PUMP_FLOW_ML_PER_S 1.5` and `EC_RISE_PER_ML_L 0.25` are uncalibrated, with one combined EC constant rather than one per bottle. They imply 144 ml of concentrate for a 20 L tank at EC 1.8. The City Greens label issues (unbalanced analysis, possible Mn unit error) are not addressed.
- **Fix:**
  - Scale all three pumps by the same factor when clamping, so the ratio is kept.
  - Dose about 50–70% of the deficit.
  - Lock out if EC hasn't risen by at least 30% of the expected rise after 2 attempts.
  - Cap ml per pump per day, and track remaining bottle volume.
  - Calibrate each pump and each bottle.

### E. Day boundary and clock problems ⚠️
1. **The day changes at the crop's start time, not midnight.** `start_epoch` is the first-boot moment (e.g. 16:33), so every day rolls over at 16:33, inside the light window. Cycle 1 of the new day starts **immediately** (L960), possibly right after the previous day's last cycle, giving back-to-back watering. The stage buttons re-anchor the start to the time of the press (L374), which makes it worse. **Fix:** round `start_epoch` down to local midnight.
2. **A dead RTC battery restarts the crop at Day 1.** `rtc.adjust()` sets the clock to compile time, which is before `start_epoch`. The day freezes, then once the clock passes `start_epoch`, L565 computes **Day 1** and the crop profile restarts. **Fix:** never move the day backwards. If `now < start_epoch`, keep the NVS day and raise an alarm.
3. **No RTC means no day advance and a 6 h watering gap after every reboot.** With no RTC, the day never advances (L557). The fallback clock starts at 00:00 on every boot, so the 06–18 window gives **no watering for 6 h after each reboot**. **Fix:** use the NVS day plus elapsed `millis()`, and skip the light window when there is no RTC.

### F. Smaller issues
- **Temperature sensor dropping mid-dose:** if it disconnects during a dosing sequence, `activelyFixingChemistry` goes false while dosing continues, so irrigation can start alongside pumps B/C.
- **Lockout counter resets on reboot:** `dosingError` and `dosingAttempts` live in RAM, so every reboot (brownout or watchdog) starts a fresh 15-attempt dosing cycle. Persist them in NVS.
- **Float empty-side bounce:** the empty side has no debounce (raw ISR), so sloshing aborts dosing or irrigation. With a single float, normal irrigation drawdown can trigger abort → refund → refill → dilution → dosing hold (hardware item).
- **Watering overrun stops all dosing:** an irrigation pump overrun sets `dosingError`, which stops dosing for a watering fault.
- **Power switch skips the refund:** the switch cuts a cycle without refunding it.
- **Constant high-temp beeping:** the chirp sounds every 10 s indefinitely. Indian summers mean constant beeping; add acknowledge/mute or a hysteresis band.
- **`while(!Serial)` runs before the pins are set up** (L382): with USB-CDC-on-boot enabled, it hangs with the pump pins floating. Move it after pin setup or add a timeout.
- **GPIO 35–37 conflict:** these are unavailable on N8R8/N16R8 boards. They're only used for SD in testing, but confirm your module.
- **Cosmetic:**
  - The Day column is still a `DAY_n` string.
  - D4–D7 are named "Germination" while the comment says "Early Seedling".
  - The "Essential oil density" claim is still there.
  - diagram.json still uses a DS1307.
  - In production, `loop()` reads NVS on every pass.

---

## 3. Suggested next pass (in order)
1. **D:** ratio-preserving clamp, partial dosing, an EC response check, and a daily ml cap.
2. **C:** turn `toxicBurnRisk` into an alarm with absolute limits.
3. **E:** align `start_epoch` to midnight, never move the day backwards, and add a no-RTC fallback.
4. **B:** feed the watchdog in the SD dump; pin the platform and library versions.
5. **A:** skip pH dosing on flush days, or correct the md.
6. Persist the lockout state, block irrigation start during any non-IDLE dosing state, and mute the high-temp chirp.
7. Compile and run a production build (`TESTING_MODE false`) in Wokwi with simulated sensors before flashing hardware.
