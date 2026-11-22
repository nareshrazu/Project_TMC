# Fixes for items 1–3 of VALIDATION_v3_Latest_Code.md

Applies to the 8 Nov `main.cpp`. Only `src/main.cpp` changed; `CropProfiles.h` is untouched. To apply, either replace `src/main.cpp` with the one in this folder, or run `git apply TMC_v3_fixes.patch` from the repo root.

**Before flashing hardware:** this was syntax-checked with g++ against stub Arduino headers (both `TESTING_MODE` true and false, zero warnings), and the dosing planner was unit-tested on a PC. It has **not** been compiled with the ESP32 toolchain or run in Wokwi yet. Run `pio run` and a Wokwi pass first.

## 1. Dosing safety
- **New `planNutrientDose()`:**
  - Doses **60% of the EC deficit** per attempt, so a wrong calibration constant undershoots instead of overshooting.
  - Splits the dose by the profile's A:B:C ratio.
  - Scales **all three pumps together** for the 250 ms–8 s pulse limits, so the ratio is kept. Day 8 from EC 0 now gives 7.2/7.2/4.8 s, where before it was 8/8/8 s.
  - The planner now runs in TESTING_MODE too, so Wokwi exercises the real dosing path.
- **Daily cap of 40 ml per bottle** (`MAX_ML_PER_PUMP_PER_DAY`):
  - Counted after every pulse, including pulses cut short by an empty tank, an overrun or the power switch.
  - Saved in NVS, so a reboot doesn't reset it. Resets each crop day.
  - When the cap is reached, EC dosing pauses until the next day; there is no lockout and watering continues. The OLED shows `WARN: DOSE CAP TODAY`.
  - A full refill to EC 1.8 needs about 49 ml per bottle at the current constants, so it reaches target over 2 days.
- **EC response check:** if 2 doses in a row raise EC by less than 30% of the expected rise, dosing locks out with `ERR: NO EC RISE!`. This catches a stuck probe, an empty bottle or a blocked pump. It only judges doses expected to raise EC by at least 0.10, so probe noise can't trigger it.
- **Lockout reason on the OLED:** `PUMP LOCKOUT`, `PUMP OVERRUN` or `NO EC RISE`.
- **Attempt counter** resets each crop day and while the cap pause is active.
- **Calibration constants at the top of `main.cpp` still need measuring:**
  - `RESERVOIR_LITERS`
  - `PUMP_FLOW_ML_PER_S[3]`: run each pump 60 s into a measuring cylinder.
  - `EC_RISE_PER_ML_L`: add 1 ml/L of the mix to your water and record the EC rise.

## 2. Watering is no longer blocked by moderate EC/pH
- Watering is held only beyond **absolute limits**: EC > 3.5, pH < 4.0 or pH > 8.5. The OLED shows `ERR: WATER UNSAFE!`.
- EC more than 0.8 above target, or pH outside 5.0–7.5, is now an **alarm only** (beep plus `WARN: pH/EC OUT BAND`). Hard tap water no longer stops seedlings being watered.
- Target buttons are now limited to EC ≤ 3.0 and pH 5.0–7.5, so a manual target can't push the system into blocking itself. Saved targets from older firmware are clamped to these limits at boot.

## 3. Clock
- **Day boundary is local midnight.** `start_epoch` is aligned to 00:00, so watering cycle 1 starts at 06:00 instead of at the time the crop was started. Older firmware's `start_epoch` is migrated at boot without skipping a day.
- **The crop never moves backwards.** If the RTC lost power (dead coin cell) or reads earlier than the start date, the saved day is kept, the start date is re-anchored, and the OLED shows `WARN: CHECK CLOCK`.
- **Corrupted RTC reads are rejected.** The clock is read twice, and the time is trusted only if the year is 2025–2099 and both reads agree. Normal operation accepts only a +1 day rollover. A bigger jump forward, or a clock that went backwards, must be seen on 10 checks in a row before anything changes.
- **Day changes wait** for any A→B→C dosing sequence to finish.
- **No RTC:** the day advances every 24 h of uptime, and the 06:00–18:00 window is skipped, so there's no 6 h dry spell after each reboot.
- The clock is checked once a second instead of every loop pass, which removes the NVS read on every loop.

## Not changed (still open from the validation report)
- **B:** the SD-dump watchdog reboot loop, and the unpinned platform and library versions.
- **A:** pH dosing still runs on flush/germination days.
- The lockout state is still held in RAM.
- The float switch's empty side has no debounce.
- There is still no pH/EC sensor driver. In a production build, the EC response check will lock out after 2 doses until one is added, which is the intended behaviour.
