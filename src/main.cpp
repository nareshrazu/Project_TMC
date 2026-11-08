#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <SPI.h>
#include <SD.h>
#include "CropProfiles.h"
#include "RTClib.h"
#include <esp_task_wdt.h>

#define WDT_TIMEOUT_SECONDS 15

RTC_DS3231 rtc;
bool rtcAvailable = false;

DateTime getSafeRTCNow() {
  if (rtcAvailable) {
    return rtc.now();
  }
  return DateTime(2026, 1, 1, 0, 0, 0) + TimeSpan(millis() / 1000);
}

// --- SYSTEM CONFIGURATION ---
#define TESTING_MODE true
#define PUMP_ON HIGH
#define PUMP_OFF LOW

// --- ESP32-S3 PIN MAPPING ---
// Actuators
#define PIN_PUMP_WATER 10
#define PIN_PUMP_A 11
#define PIN_PUMP_B 12
#define PIN_PUMP_C 13
#define PIN_PUMP_PH 14
#define PIN_PUMP_IRRIGATE 47 

// Sensors & Switches
#define PIN_SW_POWER 6
#define PIN_SW_FLOAT 5
#define ONE_WIRE_BUS 4

// Visual & Audio
#define PIN_RGB_R 15
#define PIN_RGB_G 16
#define PIN_RGB_B 17
#define PIN_BUZZER 18
#define OLED_SDA 8
#define OLED_SCL 9

// Target Buttons
#define PIN_BTN_TGT_PH_UP 41
#define PIN_BTN_TGT_PH_DN 42
#define PIN_BTN_TGT_EC_UP 45
#define PIN_BTN_TGT_EC_DN 46

// Stage / Day Navigation Buttons
#define PIN_BTN_STAGE_SEED 39  
#define PIN_BTN_STAGE_VEG 40   
#define PIN_BTN_STAGE_BLOOM 21 

// Simulation Buttons
#define PIN_BTN_SIM_PH_UP 1
#define PIN_BTN_SIM_PH_DN 2
#define PIN_BTN_SIM_EC_UP 3
#define PIN_BTN_SIM_EC_DN 7

// SD Card SPI Pins
#define SD_MOSI 35
#define SD_SCK  36
#define SD_MISO 37
#define SD_CS   38

#define TANK_EMPTY HIGH  
#define TANK_FULL  LOW   

// --- DISPLAY SETUP ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);
Preferences preferences;

#if TESTING_MODE
  const unsigned long MIX_DELAY_NUTRIENT = 2000;
  const unsigned long MIX_DELAY_PH       = 2000;
  const unsigned long PUMP_PULSE_PH      = 2000;
  const unsigned long WATER_TIMEOUT      = 15000;
  const unsigned long TELEMETRY_INTERVAL = 1000;   // 1s in test mode
#else
  const unsigned long MIX_DELAY_NUTRIENT = 120000; // 2 minutes
  const unsigned long MIX_DELAY_PH       = 180000; // 3 minutes
  const unsigned long PUMP_PULSE_PH      = 250;
  const unsigned long WATER_TIMEOUT      = 300000;
  const unsigned long TELEMETRY_INTERVAL = 60000;  // 1 minute in production
#endif

// --- STATE MACHINE ---
enum DosingState { IDLE, DOSE_WATER, DOSE_A, MIX_A, DOSE_B, MIX_B, DOSE_C, MIX_C, DOSE_PH, MIX_PH };
DosingState currentDosingState = IDLE;

// Globals
int currentDay = 1;
int currentCycleIndex = 0;
bool isCurrentlyFlushing = false;
String currentPhaseName = "";
uint8_t dailyCycles = 0;
uint8_t cycleDurationMins = 0;
bool oledAvailable = false;

bool manualOverridePH = false;
bool manualOverrideEC = false;

float targetpH = 6.0, targetEC = 0.0;
unsigned long durA = 1000, durB = 1000, durC = 1000;
float currentpH = 7.0, currentEC = 0.0, currentTemp = 24.0;
bool tempSensorConnected = true;
bool tempHighAlarm = false;

unsigned long dosingTimer = 0, mixTimer = 0, waterFillTimer = 0;
unsigned long lastTempReq = 0, lastDisplayUpdate = 0, lastDayAdvanceMillis = 0, lastIrrigationStart = 0;
bool isIrrigating = false;
const unsigned long MILLIS_PER_DAY = 86400000UL;

int dosingAttempts = 0;
const int MAX_DOSING_ATTEMPTS = 15; // Reduced for safety
bool dosingError = false;
bool waterTimeoutError = false; // Auto-clears when float switch is full!
bool forceDisplayUpdate = false;
bool systemActive = true;
bool sdInitialized = false;
bool logDumped = false; // Prevents continuous dumping when powered off

// Hardware Interrupt Flags
volatile bool tankEmptyISRFlag = false;

// Button Debounce arrays
const int numButtons = 11;
const int buttonPins[11] = {
  PIN_BTN_TGT_PH_UP, PIN_BTN_TGT_PH_DN, PIN_BTN_TGT_EC_UP, PIN_BTN_TGT_EC_DN,
  PIN_BTN_STAGE_SEED, PIN_BTN_STAGE_VEG, PIN_BTN_STAGE_BLOOM,
  PIN_BTN_SIM_PH_UP, PIN_BTN_SIM_PH_DN, PIN_BTN_SIM_EC_UP, PIN_BTN_SIM_EC_DN
};
bool lastStates[11] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
unsigned long lastPressTime[11] = {0};

// --- FUNCTION DECLARATIONS ---
void updateDisplay();
void processInputs();
void processSequentialDosing();
void handleIrrigation();

// --- HARDWARE UTILITIES ---
void setRGB(bool r, bool g, bool b) {
  digitalWrite(PIN_RGB_R, r ? HIGH : LOW);
  digitalWrite(PIN_RGB_G, g ? HIGH : LOW);
  digitalWrite(PIN_RGB_B, b ? HIGH : LOW);
}

void beep(int duration) { tone(PIN_BUZZER, 1000, duration); }

void turnOffAllPumps() {
  digitalWrite(PIN_PUMP_A, PUMP_OFF);
  digitalWrite(PIN_PUMP_B, PUMP_OFF);
  digitalWrite(PIN_PUMP_C, PUMP_OFF);
  digitalWrite(PIN_PUMP_PH, PUMP_OFF);
  digitalWrite(PIN_PUMP_WATER, PUMP_OFF);
  digitalWrite(PIN_PUMP_IRRIGATE, PUMP_OFF);
  isIrrigating = false;
}

void turnOffDosingPumps() {
  digitalWrite(PIN_PUMP_A, PUMP_OFF);
  digitalWrite(PIN_PUMP_B, PUMP_OFF);
  digitalWrite(PIN_PUMP_C, PUMP_OFF);
  digitalWrite(PIN_PUMP_PH, PUMP_OFF);
  digitalWrite(PIN_PUMP_WATER, PUMP_OFF);
}

// Float Switch Hardware Interrupt
void IRAM_ATTR floatSwitchISR() {
  int state = gpio_get_level((gpio_num_t)PIN_SW_FLOAT); 
  if (state == TANK_EMPTY) {
    tankEmptyISRFlag = true;
  } else {
    tankEmptyISRFlag = false;
  }
}

// --- SD CARD UTILITIES ---
void writeToSD(String dataLine) {
  if (!sdInitialized) return;
  File dataFile = SD.open("/data.csv", FILE_APPEND);
  if (dataFile) {
    dataFile.println(dataLine);
    dataFile.close();
  }
}

void dumpSDCardToSerial() {
  if (!sdInitialized) return;
  File dataFile = SD.open("/data.csv");
  if (dataFile) {
    Serial.println("\n--- BEGIN TEST RUN DATA (data.csv) ---");
    while (dataFile.available()) {
      Serial.write(dataFile.read());
    }
    dataFile.close();
    Serial.println("--- END TEST RUN DATA ---\n");
  } else {
    Serial.println("Error opening data.csv for readout.");
  }
}

void logPulseEvent(const char* pumpName, unsigned long durationMs, int attempt, bool starting) {
  DateTime now = getSafeRTCNow(); // Grab the exact time the event occurred
  
  char logBuf[160];
  snprintf(logBuf, sizeof(logBuf), "%04d-%02d-%02d %02d:%02d:%02d,DAY_%d,%s,%s,%s,%lu,%d,%.2f,%.2f,%.2f,%.2f,%.1f",
           now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(),
           currentDay, currentPhaseName.c_str(), starting ? "PULSE_ON" : "PULSE_OFF", 
           pumpName, durationMs, attempt, currentpH, targetpH, currentEC, targetEC, currentTemp);
           
  Serial.println(logBuf);
  writeToSD(String(logBuf));
}

// Telemetry Metrics Stream
void streamTelemetryMetrics() {
  static unsigned long lastTelemetry = 0;
  if (millis() - lastTelemetry >= TELEMETRY_INTERVAL) {
    lastTelemetry = millis();
    
    DateTime now = getSafeRTCNow(); 
    
    // Default values for idle state
    const char* evtType = "METRIC_TICK";
    const char* pumpName = "NONE";
    
    // Dynamically update the log based on what the state machine is doing
    switch (currentDosingState) {
      case DOSE_WATER: evtType = "FILLING"; pumpName = "WATER";   break;
      case DOSE_A:     evtType = "DOSING";  pumpName = "TANK_N";  break;
      case MIX_A:      evtType = "MIXING";  pumpName = "TANK_N";  break;
      case DOSE_B:     evtType = "DOSING";  pumpName = "TANK_P";  break;
      case MIX_B:      evtType = "MIXING";  pumpName = "TANK_P";  break;
      case DOSE_C:     evtType = "DOSING";  pumpName = "TANK_K";  break;
      case MIX_C:      evtType = "MIXING";  pumpName = "TANK_K";  break;
      case DOSE_PH:    evtType = "DOSING";  pumpName = "PH_DOWN"; break;
      case MIX_PH:     evtType = "MIXING";  pumpName = "PH_DOWN"; break;
      default:
        // If dosing is IDLE but irrigation is running
        if (isIrrigating) { evtType = "WATERING"; pumpName = "IRRIG_PUMP"; }
        break;
    }
    
    char metricBuf[160];
    snprintf(metricBuf, sizeof(metricBuf), "%04d-%02d-%02d %02d:%02d:%02d,DAY_%d,%s,%s,%s,0,0,%.2f,%.2f,%.2f,%.2f,%.1f",
             now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(),
             currentDay, currentPhaseName.c_str(), evtType, pumpName, currentpH, targetpH, currentEC, targetEC, currentTemp);
             
    Serial.println(metricBuf);
    writeToSD(String(metricBuf));
  }
}

// --- NVS & PROFILES ---
// isBoot flag prevents overwriting manual user adjustments during power recovery
// --- PER-PUMP RUNTIME SAFETY MONITOR ---
struct PumpSafetyTracker {
  int pin;
  const char* name;
  unsigned long maxAllowedMs;
  unsigned long activeStartTime;
};

void checkPumpRunTimeSafety() {
  #if TESTING_MODE
    unsigned long maxIrrig = 10000UL;
  #else
    unsigned long maxIrrig = (cycleDurationMins > 0 ? (cycleDurationMins * 60000UL + 30000UL) : 15000UL);
  #endif

  static PumpSafetyTracker trackers[] = {
    { PIN_PUMP_A,        "TANK_N",     10000UL, 0 },
    { PIN_PUMP_B,        "TANK_P",     10000UL, 0 },
    { PIN_PUMP_C,        "TANK_K",     10000UL, 0 },
    { PIN_PUMP_PH,       "PH_DOWN",     5000UL, 0 },
    { PIN_PUMP_WATER,    "WATER_FILL",  WATER_TIMEOUT + 5000UL, 0 },
    { PIN_PUMP_IRRIGATE, "IRRIGATION",  10000UL, 0 }
  };
  trackers[4].maxAllowedMs = WATER_TIMEOUT + 5000UL;
  trackers[5].maxAllowedMs = maxIrrig;

  unsigned long currentMillis = millis();

  for (int i = 0; i < 6; i++) {
    bool isPinActive = (digitalRead(trackers[i].pin) == PUMP_ON);
    if (isPinActive) {
      if (trackers[i].activeStartTime == 0) {
        trackers[i].activeStartTime = currentMillis;
      } else if (currentMillis - trackers[i].activeStartTime > trackers[i].maxAllowedMs) {
        Serial.printf("EMERGENCY SAFETY CUTOFF: Pump %s exceeded max runtime (%lu ms)! Forcing shutdown.\n", 
                      trackers[i].name, trackers[i].maxAllowedMs);
        turnOffAllPumps();
        dosingError = true;
        beep(1000);
        setRGB(1, 0, 0);
        trackers[i].activeStartTime = 0;
      }
    } else {
      trackers[i].activeStartTime = 0;
    }
  }
}

void loadProfileForDay(int day, bool isBoot = false) {
  if (day < 1) day = 1;
  if (day > TOTAL_DAYS) day = TOTAL_DAYS;
  
  currentDay = day;
  int idx = currentDay - 1; 

  isCurrentlyFlushing = corianderSchedule[idx].isFlush;
  dailyCycles = corianderSchedule[idx].irrigationCycles;
  cycleDurationMins = corianderSchedule[idx].irrigationMins;
  currentPhaseName = corianderSchedule[idx].phaseName;

  if (!isCurrentlyFlushing) {
    durA = corianderSchedule[idx].durA;
    durB = corianderSchedule[idx].durB;
    durC = corianderSchedule[idx].durC;

    // --- NEW TESTING OVERRIDE ---
    #if TESTING_MODE
      // Force 2-second runs so the 1-second telemetry always logs "DOSING"
      if (durA > 0) durA = 2000;
      if (durB > 0) durB = 2000;
      if (durC > 0) durC = 2000;
    #endif
    // ----------------------------

  } else {
    durA = 0; durB = 0; durC = 0;
  }

  // Update targets and NVS independently so overriding pH doesn't freeze the EC schedule (and vice versa)
  if (!isBoot && !manualOverridePH) {
    targetpH = corianderSchedule[idx].targetpH;
    preferences.putFloat("target_ph", targetpH);
  }
  if (!isBoot && !manualOverrideEC) {
    targetEC = corianderSchedule[idx].targetEC;
    preferences.putFloat("target_ec", targetEC);
  }

  // Reset daily irrigation state on any day transition (buttons, RTC rollover, or test fast-forward)
  if (!isBoot) {
    if (isIrrigating) {
      digitalWrite(PIN_PUMP_IRRIGATE, PUMP_OFF);
      isIrrigating = false;
    }
    currentCycleIndex = 0;
    preferences.putInt("cycle_idx", 0);
    lastIrrigationStart = millis();
  }
  
  preferences.putInt("day", currentDay);
  if (rtcAvailable && !isBoot) {
    uint32_t newStart = getSafeRTCNow().unixtime() - (unsigned long)(currentDay - 1) * 86400UL;
    preferences.putULong("start_epoch", newStart);
  }
  forceDisplayUpdate = true;
}

void setup() {
  Serial.begin(115200); 
  while (!Serial) { delay(10); } // Wait for Wokwi terminal handshake

  // Initialize ESP32 Task Watchdog Timer (TWDT)
  esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
  esp_task_wdt_add(NULL); // Subscribe Arduino loopTask
  
  Serial.println("\n--- SYSTEM BOOTING: DOSING CONTROL ACTIVE ---"); 

  const int outputPins[] = {PIN_PUMP_A, PIN_PUMP_B, PIN_PUMP_C, PIN_PUMP_PH, PIN_PUMP_WATER, PIN_PUMP_IRRIGATE, PIN_RGB_R, PIN_RGB_G, PIN_RGB_B, PIN_BUZZER};
  for (int pin : outputPins) {
    bool isLEDorBuzzer = (pin == PIN_RGB_R || pin == PIN_RGB_G || pin == PIN_RGB_B || pin == PIN_BUZZER);
    digitalWrite(pin, isLEDorBuzzer ? LOW : PUMP_OFF); 
    pinMode(pin, OUTPUT);
  }

  pinMode(PIN_SW_POWER, INPUT_PULLUP);
  pinMode(PIN_SW_FLOAT, INPUT_PULLUP);
  
  // Attach Safety Interrupt
  attachInterrupt(digitalPinToInterrupt(PIN_SW_FLOAT), floatSwitchISR, CHANGE);
  tankEmptyISRFlag = (digitalRead(PIN_SW_FLOAT) == TANK_EMPTY);

  for (int i = 0; i < numButtons; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  Wire.begin(OLED_SDA, OLED_SCL);

  // Initialize the RTC Module
  if (!rtc.begin()) {
    Serial.println("Warning: RTC module not found!");
    rtcAvailable = false;
  } else {
    Serial.println("RTC Initialized!");
    rtcAvailable = true;
    // If the coin cell battery is empty or new, set time to the moment you compiled the code
    if (rtc.lostPower()) {
      Serial.println("RTC lost power, setting time...");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }
  // Increase I2C Clock speed to 400kHz for faster OLED drawing
  // Wire.setClock(400000); 

  Wire.beginTransmission(0x3C);
  oledAvailable = (Wire.endTransmission() == 0);
  if (oledAvailable) {
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.println("OLED Failed");
      oledAvailable = false;
    } else {
      Serial.println("OLED Initialized!");
    }
  } else {
    Serial.println("Warning: OLED not detected at I2C 0x3C!");
  }

  // Init SD Card (TESTING_MODE only)
  #if TESTING_MODE
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (SD.begin(SD_CS)) {
      sdInitialized = true;
      Serial.println("SD Card Initialized");
    } else {
      Serial.println("SD Card Failed");
    }
  #endif

  tempSensor.begin();
  tempSensor.setWaitForConversion(false); 
  tempSensor.requestTemperatures();
  lastTempReq = millis();
  
  // NVS Memory Boot Recovery
  preferences.begin("grow_data", false); 

  #if TESTING_MODE
    preferences.clear(); // Wipes memory on every boot so CropProfiles.h is always used
  #endif
  
  currentDay = preferences.getInt("day", 1);
  currentCycleIndex = preferences.getInt("cycle_idx", 0);
  manualOverridePH = preferences.getBool("man_ovr_ph", false);
  manualOverrideEC = preferences.getBool("man_ovr_ec", false);

  uint32_t startEpoch = preferences.getULong("start_epoch", 0);
  if (rtcAvailable) {
    if (startEpoch == 0) {
      startEpoch = getSafeRTCNow().unixtime();
      preferences.putULong("start_epoch", startEpoch);
    }
  #if !TESTING_MODE
    uint32_t nowEpoch = getSafeRTCNow().unixtime();
    if (startEpoch > 0 && nowEpoch >= startEpoch) {
      int calculatedDay = ((nowEpoch - startEpoch) / 86400UL) + 1;
      if (calculatedDay > TOTAL_DAYS) calculatedDay = TOTAL_DAYS;
      if (calculatedDay != currentDay) {
        currentDay = calculatedDay;
        currentCycleIndex = 0;
        preferences.putInt("cycle_idx", 0);
      }
    }
  #endif
  }

  loadProfileForDay(currentDay, true); 
  
  // Recover manual user overrides if they exist; otherwise, initialize cleanly to silence errors
  if (!preferences.isKey("target_ph")) {
    preferences.putFloat("target_ph", corianderSchedule[currentDay-1].targetpH);
  }
  if (!preferences.isKey("target_ec")) {
    preferences.putFloat("target_ec", corianderSchedule[currentDay-1].targetEC);
  }
  
  if (manualOverridePH) {
    targetpH = preferences.getFloat("target_ph");
  }
  if (manualOverrideEC) {
    targetEC = preferences.getFloat("target_ec");
  }

  #if TESTING_MODE
  if (sdInitialized) {
    // Write CSV header only if /data.csv does not already exist
    if (!SD.exists("/data.csv")) {
      writeToSD("Timestamp,Day,Phase,EventType,PumpName,DurationMs,Attempt,pH,Target_pH,EC,Target_EC,TempC");
      Serial.println("CSV Header written to SD card.");
    }
    Serial.println("Timestamp,Day,Phase,EventType,PumpName,DurationMs,Attempt,pH,Target_pH,EC,Target_EC,TempC");
  }
  #endif
}

void loop() {
  esp_task_wdt_reset(); // Feed Task Watchdog
  checkPumpRunTimeSafety(); // Independent maximum pump on-time safety cutoff

  systemActive = (digitalRead(PIN_SW_POWER) == LOW); 

  if (!systemActive) {
    turnOffAllPumps();
    setRGB(0, 0, 0); 
    currentDosingState = IDLE;

    dosingError = false;   // Clears the red error lockout
    waterTimeoutError = false;
    dosingAttempts = 0;    // Resets the 15-attempt counter
    
    // Dump SD Card logs when the system is switched off
    if (!logDumped) {
      dumpSDCardToSerial();
      logDumped = true; 
    }
    
    if (oledAvailable && (millis() - lastDisplayUpdate > 500)) {
      display.clearDisplay();
      display.setTextSize(2);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(10, 25);
      display.print(F("SYSTEM OFF"));
      display.display();
      lastDisplayUpdate = millis();
      forceDisplayUpdate = true;
    }
    return; // Block execution
  }

  logDumped = false; // Reset dump flag while system is running
  processInputs();
  processSequentialDosing();
  handleIrrigation();

  #if !TESTING_MODE
    // PRODUCTION: Automatic 24-Hour Day Advancement via RTC start_epoch timestamp
    if (rtcAvailable) {
      uint32_t startEpoch = preferences.getULong("start_epoch", 0);
      if (startEpoch == 0) {
        startEpoch = getSafeRTCNow().unixtime();
        preferences.putULong("start_epoch", startEpoch);
      }
      DateTime now = getSafeRTCNow();
      if (now.unixtime() >= startEpoch) {
        int calculatedDay = ((now.unixtime() - startEpoch) / 86400UL) + 1;
        if (calculatedDay > TOTAL_DAYS) calculatedDay = TOTAL_DAYS;
        if (calculatedDay != currentDay) {
          loadProfileForDay(calculatedDay); 
        }
      }
    }
  #endif
  
  // Update UI Non-Blockingly
  if (millis() - lastDisplayUpdate > 500 || forceDisplayUpdate) {
    updateDisplay();
    lastDisplayUpdate = millis();
    forceDisplayUpdate = false;
  }

  // Generate 1Hz Continuous Plotter Metrics
  streamTelemetryMetrics();
}

void processInputs() {
  for (int i = 0; i < numButtons; i++) {
    bool currentState = digitalRead(buttonPins[i]);
    if (currentState == LOW && lastStates[i] == HIGH) {
      if (millis() - lastPressTime[i] > 200) { 
        lastPressTime[i] = millis();
        forceDisplayUpdate = true;
        beep(50); 

        switch(i) {
          case 0: // PIN_BTN_TGT_PH_UP
            targetpH += 0.05; 
            if (targetpH > 8.0) targetpH = 8.0; 
            manualOverridePH = true;
            preferences.putBool("man_ovr_ph", true);
            preferences.putFloat("target_ph", targetpH); 
            break;
            
          case 1: // PIN_BTN_TGT_PH_DN
            targetpH -= 0.05; 
            if (targetpH < 4.0) targetpH = 4.0; 
            manualOverridePH = true;
            preferences.putBool("man_ovr_ph", true);
            preferences.putFloat("target_ph", targetpH); 
            break;
            
          case 2: // PIN_BTN_TGT_EC_UP
            targetEC += 0.1;  
            if (targetEC > 4.0) targetEC = 4.0; 
            manualOverrideEC = true;
            preferences.putBool("man_ovr_ec", true);
            preferences.putFloat("target_ec", targetEC); 
            break;
            
          case 3: // PIN_BTN_TGT_EC_DN
            targetEC -= 0.1;  
            if (targetEC < 0.0) targetEC = 0.0; 
            manualOverrideEC = true;
            preferences.putBool("man_ovr_ec", true);
            preferences.putFloat("target_ec", targetEC); 
            break;
          
          // Stage/Day buttons also serve as "Return to Profile" (clearing manual overrides)
          case 4: // PIN_BTN_STAGE_SEED (Reset to Day 1 Profile)
            manualOverridePH = false;
            manualOverrideEC = false;
            preferences.putBool("man_ovr_ph", false);
            preferences.putBool("man_ovr_ec", false);
            loadProfileForDay(1); 
            break;
            
          case 5: // PIN_BTN_STAGE_VEG (Next Day + Return to Profile)
            manualOverridePH = false;
            manualOverrideEC = false;
            preferences.putBool("man_ovr_ph", false);
            preferences.putBool("man_ovr_ec", false);
            loadProfileForDay(currentDay + 1); 
            break;
            
          case 6: // PIN_BTN_STAGE_BLOOM (Prev Day + Return to Profile)
            manualOverridePH = false;
            manualOverrideEC = false;
            preferences.putBool("man_ovr_ph", false);
            preferences.putBool("man_ovr_ec", false);
            loadProfileForDay(currentDay - 1); 
            break;
          
          case 7: currentpH += 0.05; break;
          case 8: currentpH -= 0.05; break;
          case 9: currentEC += 0.05; break;
          case 10: currentEC -= 0.05; break;
        }
      }
    }
    lastStates[i] = currentState;
  }

  if (millis() - lastTempReq >= 2000) {
    float tempC = tempSensor.getTempCByIndex(0);
    
    // The Dallas library returns -127.0 if the sensor is disconnected, 85.0 is power-on error state
    if (tempC <= -100.0 || tempC >= 85.0) {
      tempSensorConnected = false; // Sensor unplugged or dead
    } else {
      tempSensorConnected = true;
      currentTemp = tempC;
    }
    
    tempHighAlarm = (tempSensorConnected && currentTemp > 24.0);
    
    tempSensor.requestTemperatures(); // Request next reading asynchronously 
    lastTempReq = millis();
  }

  // Audio warning for high water temperature (intermittent chirp without blocking)
  static unsigned long lastHighTempBeep = 0;
  if (tempHighAlarm && (millis() - lastHighTempBeep >= 10000)) {
    beep(100);
    lastHighTempBeep = millis();
  }
}

void processSequentialDosing() {
  if (waterTimeoutError && !tankEmptyISRFlag) {
    waterTimeoutError = false;
    forceDisplayUpdate = true;
  }

  if (dosingError || waterTimeoutError) {
    turnOffDosingPumps();
    setRGB(1, 0, 0); 
    currentDosingState = IDLE;
    return;
  }

  // Safety Abort
  if (tankEmptyISRFlag && currentDosingState != IDLE && currentDosingState != DOSE_WATER) {
    turnOffAllPumps();
    currentDosingState = IDLE;
    return;
  }

  const float PH_DEADBAND = 0.20; 
  const float EC_DEADBAND = 0.10; // Tightened deadband
  const float RESERVOIR_LITERS   = 20.0f;
  const float PUMP_FLOW_ML_PER_S = 1.5f;
  const float EC_RISE_PER_ML_L   = 0.25f;

  if (tankEmptyISRFlag) setRGB(1, 0, 0); // RED = Empty Alert
  else if (currentDosingState == IDLE) setRGB(0, 1, 0); // GREEN = Nominal
  else if (currentDosingState == DOSE_WATER) setRGB(0, 0, 1); // BLUE = Fill
  else setRGB(1, 1, 0); // YELLOW = Mixing/Dosing

  static unsigned long floatFullStartTime = 0;

  switch (currentDosingState) {
    case IDLE:
      // Defect 1.6: Never start chemical dosing or filling while water is actively being sent to the plants
      if (isIrrigating) {
        break;
      }

      if (tankEmptyISRFlag) { 
        currentDosingState = DOSE_WATER;
        waterFillTimer = millis();
      }
      else if (!tempSensorConnected) {
        // Temperature sensor disconnected - Prevent dosing for safety
      }
      else if (!isCurrentlyFlushing && currentEC < (targetEC - EC_DEADBAND)) {
        #if !TESTING_MODE
          // Defect 1.7: Scale pulse durations proportionally to EC deficit and reservoir volume
          int idx = currentDay - 1;
          float totalRatio = corianderSchedule[idx].durA + corianderSchedule[idx].durB + corianderSchedule[idx].durC;
          float totalMl = ((targetEC - currentEC) / EC_RISE_PER_ML_L) * RESERVOIR_LITERS;
          float totalMs = (totalMl / PUMP_FLOW_ML_PER_S) * 1000.0f;
          if (totalRatio > 0) {
            durA = constrain((unsigned long)(totalMs * (corianderSchedule[idx].durA / totalRatio)), 250UL, 8000UL);
            durB = constrain((unsigned long)(totalMs * (corianderSchedule[idx].durB / totalRatio)), 250UL, 8000UL);
            durC = constrain((unsigned long)(totalMs * (corianderSchedule[idx].durC / totalRatio)), 250UL, 8000UL);
          }
        #endif

        if (dosingAttempts >= MAX_DOSING_ATTEMPTS) dosingError = true;
        else { 
          currentDosingState = DOSE_A; 
          dosingTimer = millis(); 
          dosingAttempts++; 
          logPulseEvent("TANK_N", durA, dosingAttempts, true); // EVENT LOG
        }
      }
      else if (currentpH > (targetpH + PH_DEADBAND)) {
        if (dosingAttempts >= MAX_DOSING_ATTEMPTS) dosingError = true;
        else { 
          currentDosingState = DOSE_PH; 
          dosingTimer = millis(); 
          dosingAttempts++; 
          logPulseEvent("PH_DOWN", PUMP_PULSE_PH, dosingAttempts, true); // EVENT LOG
        }
      } 
      else if ((isCurrentlyFlushing || currentEC >= (targetEC - EC_DEADBAND)) && 
               (currentpH <= (targetpH + PH_DEADBAND))) {
        dosingAttempts = 0; 
      }
      break;

    case DOSE_WATER:
      if (millis() - waterFillTimer > WATER_TIMEOUT) {
        turnOffAllPumps();
        waterTimeoutError = true; // Use waterTimeoutError instead of dosingError
        beep(1000); 
        currentDosingState = IDLE;
        floatFullStartTime = 0;
        break;
      }
      
      digitalWrite(PIN_PUMP_WATER, PUMP_ON);
      
      if (digitalRead(PIN_SW_FLOAT) == TANK_FULL) {
        if (floatFullStartTime == 0) {
          floatFullStartTime = millis();
        } else if (millis() - floatFullStartTime >= 2000) {
          digitalWrite(PIN_PUMP_WATER, PUMP_OFF);
          tankEmptyISRFlag = false; // Guarantee ISR flag matches verified physical pin state
          currentDosingState = IDLE;
          floatFullStartTime = 0;
        }
      } else {
        floatFullStartTime = 0;
      }
      break;

    case DOSE_A:
      digitalWrite(PIN_PUMP_A, PUMP_ON);
      if (millis() - dosingTimer >= durA) {
        digitalWrite(PIN_PUMP_A, PUMP_OFF);
        logPulseEvent("TANK_N", durA, dosingAttempts, false); // EVENT LOG
        #if TESTING_MODE
          currentEC += 0.1; 
        #endif
        mixTimer = millis();
        currentDosingState = MIX_A;
      }
      break;

    case MIX_A:
      if (millis() - mixTimer >= MIX_DELAY_NUTRIENT) {
        currentDosingState = DOSE_B;
        dosingTimer = millis();
        logPulseEvent("TANK_P", durB, dosingAttempts, true); // EVENT LOG
      }
      break;

    case DOSE_B:
      digitalWrite(PIN_PUMP_B, PUMP_ON);
      if (millis() - dosingTimer >= durB) {
        digitalWrite(PIN_PUMP_B, PUMP_OFF);
        logPulseEvent("TANK_P", durB, dosingAttempts, false); // EVENT LOG
        #if TESTING_MODE
          currentEC += 0.1; 
        #endif
        mixTimer = millis();
        currentDosingState = MIX_B;
      }
      break;

    case MIX_B:
      if (millis() - mixTimer >= MIX_DELAY_NUTRIENT) {
        currentDosingState = DOSE_C;
        dosingTimer = millis();
        logPulseEvent("TANK_K", durC, dosingAttempts, true); // EVENT LOG
      }
      break;

    case DOSE_C:
      digitalWrite(PIN_PUMP_C, PUMP_ON);
      if (millis() - dosingTimer >= durC) {
        digitalWrite(PIN_PUMP_C, PUMP_OFF);
        logPulseEvent("TANK_K", durC, dosingAttempts, false); // EVENT LOG
        #if TESTING_MODE
          currentEC += 0.1; 
        #endif
        mixTimer = millis();
        currentDosingState = MIX_C;
      }
      break;

    case MIX_C:
      if (millis() - mixTimer >= MIX_DELAY_NUTRIENT) currentDosingState = IDLE;
      break;

    case DOSE_PH:
      digitalWrite(PIN_PUMP_PH, PUMP_ON);
      if (millis() - dosingTimer >= PUMP_PULSE_PH) {
        digitalWrite(PIN_PUMP_PH, PUMP_OFF);
        logPulseEvent("PH_DOWN", PUMP_PULSE_PH, dosingAttempts, false); // EVENT LOG
        #if TESTING_MODE
          currentpH -= 0.15; 
        #endif
        mixTimer = millis();
        currentDosingState = MIX_PH;
      }
      break;

    case MIX_PH:
      if (millis() - mixTimer >= MIX_DELAY_PH) currentDosingState = IDLE;
      break;
  }
}

void handleIrrigation() {
  // 1. Check if it's a NEW DAY to reset the cycle counter
  static int lastIrrigationDay = currentDay;
  if (lastIrrigationDay != currentDay) {
    currentCycleIndex = 0;
    preferences.putInt("cycle_idx", 0);
    lastIrrigationDay = currentDay;
    lastIrrigationStart = millis(); // Reset timer for the new day
  }
  // Define acceptable target ranges
  const float PH_DEADBAND = 0.20; 
  const float EC_DEADBAND = 0.10; 

  bool ecNeedsDosing = (!isCurrentlyFlushing && (currentEC < targetEC - EC_DEADBAND));
  bool phNeedsDosing = (currentpH > targetpH + PH_DEADBAND);
  // Hold off STARTING irrigation if chemistry needs fixing, 
  // but if already watering (!isIrrigating is false), let the current cycle finish unless a dosing pump is actually active!
  bool activelyFixingChemistry = (!dosingError && !waterTimeoutError && tempSensorConnected && (currentDosingState != IDLE || (!isIrrigating && (ecNeedsDosing || phNeedsDosing))));

  // Lethal overdose guard: block watering ONLY if water is dangerously acidic/alkaline or over-salted
  bool toxicBurnRisk = (currentpH < 4.5 || currentpH > 8.0) || (!isCurrentlyFlushing && currentEC > (targetEC + 0.80));

  // Survival Mode: Wait while healthy dosing is in progress, but NEVER block watering on dosingError/temp faults unless water is toxic
  if (!systemActive || dailyCycles == 0 || cycleDurationMins == 0 || tankEmptyISRFlag || activelyFixingChemistry || toxicBurnRisk) {
    if (isIrrigating) {
      digitalWrite(PIN_PUMP_IRRIGATE, PUMP_OFF);
      DateTime now = getSafeRTCNow();
      char logBuf[160];
      snprintf(logBuf, sizeof(logBuf), "%04d-%02d-%02d %02d:%02d:%02d,DAY_%d,%s,PULSE_OFF,IRRIGATION_ABORT,%lu,1,%.2f,%.2f,%.2f,%.2f,%.1f",
               now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(), 
               currentDay, currentPhaseName.c_str(), 0UL, currentpH, targetpH, currentEC, targetEC, currentTemp);
      Serial.println(logBuf);
      writeToSD(String(logBuf));
      
      isIrrigating = false;
      // Defect 1.6: Refund the aborted cycle so the plants get their full watering when the fault clears
      if (currentCycleIndex > 0) {
        currentCycleIndex--;
        preferences.putInt("cycle_idx", currentCycleIndex);
      }
    }
    return;
  }

  // 2. STOP if we have already completed all cycles for today!
  if (currentCycleIndex >= dailyCycles && !isIrrigating) {
    
    #if TESTING_MODE
      // Fast-forward testing: Wait 6 seconds after the final watering, then advance to tomorrow
      if (millis() - lastIrrigationStart >= 6000) {

        // ONLY advance if we haven't reached the end of the crop profile yet
        if (currentDay < TOTAL_DAYS) {
          loadProfileForDay(currentDay + 1);
          
          // Log the automated day transition
          DateTime now = getSafeRTCNow();
          char logBuf[160];
          snprintf(logBuf, sizeof(logBuf), "%04d-%02d-%02d %02d:%02d:%02d,DAY_%d,%s,TEST_DAY_ADVANCE,NONE,0,0,%.2f,%.2f,%.2f,%.2f,%.1f",
                   now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(), 
                   currentDay, currentPhaseName.c_str(), currentpH, targetpH, currentEC, targetEC, currentTemp);
          Serial.println(logBuf);
          writeToSD(String(logBuf));
        }
      }
    #endif
    
    return; // Block irrigation until the day officially transitions
  }

  // 3. Calculate Timers (Testing vs Production)
  #if TESTING_MODE
    unsigned long intervalMs = 2000;
    unsigned long durationMs = 2000;
    unsigned long effectiveIntervalMs = intervalMs;
    bool withinLightWindow = true;
  #else
    const unsigned long LIGHT_HOURS = 12UL; // 06:00 to 18:00 daylight window
    DateTime now = getSafeRTCNow();
    bool withinLightWindow = (now.hour() >= 6 && now.hour() < 18);

    unsigned long durationMs = (unsigned long)cycleDurationMins * 60000UL;
    unsigned long slotMs = (LIGHT_HOURS * 3600000UL) / dailyCycles;
    unsigned long intervalMs = (slotMs > durationMs) ? (slotMs - durationMs) : 60000UL;
    // Start Cycle 1 as soon as the 06:00 window opens; space subsequent cycles by intervalMs
    unsigned long effectiveIntervalMs = (currentCycleIndex == 0) ? 0UL : intervalMs;
  #endif

  // 4. Stop irrigation FIRST (so a cycle crossing 18:00 always shuts off safely!)
  if (isIrrigating && (millis() - lastIrrigationStart >= durationMs)) {
    isIrrigating = false;
    digitalWrite(PIN_PUMP_IRRIGATE, PUMP_OFF);
    lastIrrigationStart = millis(); 

    DateTime now = getSafeRTCNow();
    char logBuf[160];
    snprintf(logBuf, sizeof(logBuf), "%04d-%02d-%02d %02d:%02d:%02d,DAY_%d,%s,PULSE_OFF,IRRIGATION_C%d_%d,%lu,1,%.2f,%.2f,%.2f,%.2f,%.1f",
             now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(), 
             currentDay, currentPhaseName.c_str(), currentCycleIndex, dailyCycles, durationMs, currentpH, targetpH, currentEC, targetEC, currentTemp);
    Serial.println(logBuf);
    writeToSD(String(logBuf));
    forceDisplayUpdate = true;
    return;
  }

  // 5. Start irrigation (only during daylight window in production)
  if (!isIrrigating && withinLightWindow && (millis() - lastIrrigationStart >= effectiveIntervalMs)) {
    isIrrigating = true;
    currentCycleIndex++;
    preferences.putInt("cycle_idx", currentCycleIndex);
    lastIrrigationStart = millis(); 

    digitalWrite(PIN_PUMP_IRRIGATE, PUMP_ON);
    
    DateTime now = getSafeRTCNow();
    char logBuf[160];
    snprintf(logBuf, sizeof(logBuf), "%04d-%02d-%02d %02d:%02d:%02d,DAY_%d,%s,PULSE_ON,IRRIGATION_C%d_%d,%lu,1,%.2f,%.2f,%.2f,%.2f,%.1f",
             now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(), 
             currentDay, currentPhaseName.c_str(), currentCycleIndex, dailyCycles, durationMs, currentpH, targetpH, currentEC, targetEC, currentTemp);
    Serial.println(logBuf);
    writeToSD(String(logBuf));
    forceDisplayUpdate = true;
  }
}

void updateDisplay() {
  if (!oledAvailable) return; // Skip drawing if screen is dead
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("D"); display.print(currentDay); display.print(":");
  display.print(currentPhaseName);
  
  if (isCurrentlyFlushing && currentPhaseName != "Germination") {
    display.setCursor(85, 0);
    display.print("[FLUSH]");
  }

  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  // pH Box
  display.setCursor(2, 12); display.print("pH ");
  if (currentpH > targetpH + 0.20) display.print("HI");
  else if (currentpH < targetpH - 0.20) display.print("LO");
  else display.print("OK");

  display.setTextSize(2);
  display.setCursor(2, 21); display.print(currentpH, 1);
  display.setTextSize(1);
  display.setCursor(2, 36); display.print("T:"); display.print(targetpH, 1);

  display.drawFastVLine(63, 10, 35, SSD1306_WHITE);

  // EC Box
  display.setCursor(67, 12); display.print("EC ");
  if (currentEC > targetEC + 0.10) display.print("HI");
  else if (currentEC < targetEC - 0.10) display.print("LO");
  else display.print("OK");

  display.setTextSize(2);
  display.setCursor(67, 21); display.print(currentEC, 1);
  display.setTextSize(1);
  display.setCursor(67, 36); display.print("T:"); display.print(targetEC, 1);

  display.drawFastHLine(0, 46, 128, SSD1306_WHITE);

  // Bottom Row 1: Temp & Irrigation Status
  display.setCursor(2, 48);
  display.print(currentTemp, 1); display.print("C");
  if (tempHighAlarm) display.print("!");

  display.setCursor(55, 48);
  if (isIrrigating) {
    display.print("IRG "); display.print(currentCycleIndex); display.print("/"); display.print(dailyCycles);
  } else {
    display.print("IRRIG: IDLE");
  }

  // Bottom Row 2: Errors or Dosing State Machine Status
  display.setCursor(2, 57);
  if (dosingError) {
    display.print("ERR: PUMP LOCKOUT!");
  } else if (waterTimeoutError) {
    display.print("ERR: FILL TIMEOUT!");
  } else if (!tempSensorConnected) {
    display.print("ERR: TEMP SENSOR!");
  } else if (tankEmptyISRFlag) {
    display.print("ERR: TANK EMPTY!");
  } else if (tempHighAlarm && !isIrrigating && currentDosingState == IDLE) {
    display.print("WARN: HIGH TEMP >24C");
  } else if (isIrrigating) {
    unsigned long elapsedSecs = (millis() - lastIrrigationStart) / 1000UL;
    #if TESTING_MODE
      unsigned long totalSecs = 2UL;
    #else
      unsigned long totalSecs = (unsigned long)cycleDurationMins * 60UL;
    #endif
    unsigned long remainingSecs = (totalSecs > elapsedSecs) ? (totalSecs - elapsedSecs) : 0UL;
    display.print("WATERING REM: "); display.print(remainingSecs); display.print("s   ");
  } else {
    display.print("SYS: ");
    switch (currentDosingState) {
      case IDLE:       display.print("MONITORING"); break;
      case DOSE_WATER: display.print("FILLING TANK"); break;
      case DOSE_A:     display.print("DOSING N"); break;
      case MIX_A:      display.print("MIXING N"); break;
      case DOSE_B:     display.print("DOSING P"); break;
      case MIX_B:      display.print("MIXING P"); break;
      case DOSE_C:     display.print("DOSING K"); break;
      case MIX_C:      display.print("MIXING K"); break;
      case DOSE_PH:    display.print("DOSING pH-"); break;
      case MIX_PH:     display.print("MIXING pH-"); break;
    }
  }

  display.display();
}