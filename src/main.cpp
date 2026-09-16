#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// --- RELAY CONFIGURATION ---
#define PUMP_ON HIGH
#define PUMP_OFF LOW

// --- DISPLAY SETUP ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- PINS ---
#define PIN_PUMP_A 11       // Tank N (Nitrogen focus)
#define PIN_PUMP_B 12       // Tank P (Phosphorus focus)
#define PIN_PUMP_C 13       // Tank K (Potassium focus)
#define PIN_PUMP_PH 14      // pH Down
#define PIN_PUMP_WATER 10   // Water Fill Pump
#define PIN_SW_FLOAT 5      // Float Switch (Wire between GPIO 5 and GND)

#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);

// --- FLOAT SWITCH POLARITY ---
// Change these if your physical float switch operates in reverse logic
#define TANK_EMPTY HIGH  // Pin pulled HIGH when water level is LOW (Switch OPEN)
#define TANK_FULL  LOW   // Pin pulled LOW when water level is FULL (Switch CLOSED to GND)

// Button Pins - Target Settings
#define PIN_BTN_TGT_PH_UP 35
#define PIN_BTN_TGT_PH_DN 36
#define PIN_BTN_TGT_EC_UP 37
#define PIN_BTN_TGT_EC_DN 38

// Button Pins - Growth Stages
#define PIN_BTN_STAGE_SEED 19   
#define PIN_BTN_STAGE_VEG 20    
#define PIN_BTN_STAGE_BLOOM 21  

// Button Pins - Simulation Adjustments
#define PIN_BTN_SIM_PH_UP 1
#define PIN_BTN_SIM_PH_DN 2
#define PIN_BTN_SIM_EC_UP 3
#define PIN_BTN_SIM_EC_DN 7

// --- SYSTEM CONFIGURATION ---
#define TESTING_MODE true

#if TESTING_MODE
  const unsigned long MIX_DELAY_NUTRIENT = 2000;   // 2 seconds
  const unsigned long MIX_DELAY_PH       = 3000;   // 3 seconds
  const unsigned long PUMP_PULSE_PH      = 1500;   // 1.5 seconds
#else
  const unsigned long MIX_DELAY_NUTRIENT = 120000; // 2 minutes
  const unsigned long MIX_DELAY_PH       = 180000; // 3 minutes
  const unsigned long PUMP_PULSE_PH      = 250;    // 250ms micro-burst
#endif

// --- STATE MACHINE & STAGE RATIOS ---
enum DosingState { IDLE, DOSE_WATER, DOSE_A, MIX_A, DOSE_B, MIX_B, DOSE_C, MIX_C, DOSE_PH, MIX_PH };
DosingState currentDosingState = IDLE;

enum GrowStage { SEED, VEG, BLOOM };
GrowStage currentStage = SEED;

unsigned long durA = 1500; // Tank N duration
unsigned long durB = 1500; // Tank P duration
unsigned long durC = 1000; // Tank K duration

float targetpH = 5.8, targetEC = 1.0;
float currentpH = 6.8, currentEC = 0.0, currentTemp = 24.0;

unsigned long dosingTimer = 0, mixTimer = 0, lastTempRead = 0, lastDisplayUpdate = 0;
int dosingAttempts = 0;
const int MAX_DOSING_ATTEMPTS = 10;
bool dosingError = false;
bool forceDisplayUpdate = false;

// Function Prototypes
void processInputs();
void processSequentialDosing();
void updateDisplay();
void turnOffAllPumps();
void setStageParameters(GrowStage stage);

void setup() {
  Serial.begin(115200);
  
  const int pumpPins[] = {PIN_PUMP_A, PIN_PUMP_B, PIN_PUMP_C, PIN_PUMP_PH, PIN_PUMP_WATER};
  for (int pin : pumpPins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, PUMP_OFF);
  }
  
  pinMode(PIN_SW_FLOAT, INPUT_PULLUP);

  const int inputPins[] = {
    PIN_BTN_TGT_PH_UP, PIN_BTN_TGT_PH_DN, PIN_BTN_TGT_EC_UP, PIN_BTN_TGT_EC_DN,
    PIN_BTN_STAGE_SEED, PIN_BTN_STAGE_VEG, PIN_BTN_STAGE_BLOOM,
    PIN_BTN_SIM_PH_UP, PIN_BTN_SIM_PH_DN, PIN_BTN_SIM_EC_UP, PIN_BTN_SIM_EC_DN
  };
  
  for (int i = 0; i < 11; i++) {
    pinMode(inputPins[i], INPUT_PULLUP);
  }

  tempSensor.begin();
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  setStageParameters(SEED);
  display.clearDisplay();
  display.display();
}

void loop() {
  processInputs();
  processSequentialDosing();
  
  if (millis() - lastDisplayUpdate > 500 || forceDisplayUpdate) {
    updateDisplay();
    lastDisplayUpdate = millis();
    forceDisplayUpdate = false;
  }
}

void setStageParameters(GrowStage stage) {
  currentStage = stage;
  switch (stage) {
    case SEED:
      targetEC = 1.0;
      targetpH = 5.8;
      durA = 1500;
      durB = 1500;
      durC = 1000;
      break;

    case VEG:
      targetEC = 1.6;
      targetpH = 5.8;
      durA = 4000;
      durB = 2000;
      durC = 2000;
      break;

    case BLOOM:
      targetEC = 2.2;
      targetpH = 6.2;
      durA = 1000;
      durB = 4000;
      durC = 3500;
      break;
  }
}

void turnOffAllPumps() {
  digitalWrite(PIN_PUMP_A, PUMP_OFF);
  digitalWrite(PIN_PUMP_B, PUMP_OFF);
  digitalWrite(PIN_PUMP_C, PUMP_OFF);
  digitalWrite(PIN_PUMP_PH, PUMP_OFF);
  digitalWrite(PIN_PUMP_WATER, PUMP_OFF);
}

void processInputs() {
  static bool lastStates[11] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
  static unsigned long lastPressTime[11] = {0};
  
  const int pins[11] = {
    PIN_BTN_TGT_PH_UP, PIN_BTN_TGT_PH_DN, PIN_BTN_TGT_EC_UP, PIN_BTN_TGT_EC_DN,
    PIN_BTN_STAGE_SEED, PIN_BTN_STAGE_VEG, PIN_BTN_STAGE_BLOOM,
    PIN_BTN_SIM_PH_UP, PIN_BTN_SIM_PH_DN, PIN_BTN_SIM_EC_UP, PIN_BTN_SIM_EC_DN
  };

  for (int i = 0; i < 11; i++) {
    bool currentState = digitalRead(pins[i]);

    if (currentState == LOW && lastStates[i] == HIGH) {
      if (millis() - lastPressTime[i] > 200) { 
        lastPressTime[i] = millis();
        forceDisplayUpdate = true;
        dosingError = false;
        dosingAttempts = 0;

        switch(i) {
          case 0: targetpH += 0.05; break;
          case 1: targetpH -= 0.05; break;
          case 2: targetEC += 0.05; break;
          case 3: targetEC -= 0.05; break;
          case 4: setStageParameters(SEED); break;
          case 5: setStageParameters(VEG);  break;
          case 6: setStageParameters(BLOOM); break;
          case 7: currentpH += 0.05; break;
          case 8: currentpH -= 0.05; break;
          case 9: currentEC += 0.05; break;
          case 10: currentEC -= 0.05; break;
        }
      }
    }
    lastStates[i] = currentState;
  }

  if (millis() - lastTempRead >= 2000) {
    float tempC = tempSensor.getTempCByIndex(0);
    if (tempC > -50.0) currentTemp = tempC;
    tempSensor.requestTemperatures(); 
    lastTempRead = millis();
  }
}

void processSequentialDosing() {
  if (dosingError) {
    turnOffAllPumps();
    currentDosingState = IDLE;
    return;
  }

  if (currentDosingState != IDLE && currentDosingState != DOSE_WATER) {
    bool shouldAbort = false;
    if ((currentDosingState == MIX_A || currentDosingState == MIX_B || currentDosingState == MIX_C) && (currentEC >= targetEC)) {
      shouldAbort = true;
    }
    if ((currentDosingState == DOSE_PH || currentDosingState == MIX_PH) && (currentpH <= targetpH)) {
      shouldAbort = true;
    }
    if (shouldAbort) {
      turnOffAllPumps();
      currentDosingState = IDLE;
      dosingAttempts = 0;
      forceDisplayUpdate = true;
      return; 
    }
  }

  const float PH_DEADBAND = 0.20; 
  const float EC_DEADBAND = 0.15; 

  switch (currentDosingState) {
    case IDLE:
      // Trigger water filling if float switch detects empty tank
      if (digitalRead(PIN_SW_FLOAT) == TANK_EMPTY) { 
        currentDosingState = DOSE_WATER;
      }
      else if (currentEC < (targetEC - EC_DEADBAND)) {
        if (dosingAttempts >= MAX_DOSING_ATTEMPTS) {
          dosingError = true;
        } else {
          currentDosingState = DOSE_A;
          dosingTimer = millis();
          dosingAttempts++;
        }
      }
      else if (currentpH > (targetpH + PH_DEADBAND)) {
        if (dosingAttempts >= MAX_DOSING_ATTEMPTS) {
          dosingError = true;
        } else {
          currentDosingState = DOSE_PH;
          dosingTimer = millis();
          dosingAttempts++;
        }
      } 
      else {
        dosingAttempts = 0; 
      }
      break;

    case DOSE_WATER:
      digitalWrite(PIN_PUMP_WATER, PUMP_ON);
      // Stop filling as soon as tank reaches full level
      if (digitalRead(PIN_SW_FLOAT) == TANK_FULL) {
        digitalWrite(PIN_PUMP_WATER, PUMP_OFF);
        currentDosingState = IDLE;
      }
      break;

    case DOSE_A:
      digitalWrite(PIN_PUMP_A, PUMP_ON);
      if (millis() - dosingTimer >= durA) {
        digitalWrite(PIN_PUMP_A, PUMP_OFF);
        currentEC += 0.2; 
        mixTimer = millis();
        currentDosingState = MIX_A;
      }
      break;

    case MIX_A:
      if (millis() - mixTimer >= MIX_DELAY_NUTRIENT) {
        currentDosingState = DOSE_B;
        dosingTimer = millis();
      }
      break;

    case DOSE_B:
      digitalWrite(PIN_PUMP_B, PUMP_ON);
      if (millis() - dosingTimer >= durB) {
        digitalWrite(PIN_PUMP_B, PUMP_OFF);
        currentEC += 0.2; 
        mixTimer = millis();
        currentDosingState = MIX_B;
      }
      break;

    case MIX_B:
      if (millis() - mixTimer >= MIX_DELAY_NUTRIENT) {
        currentDosingState = DOSE_C;
        dosingTimer = millis();
      }
      break;

    case DOSE_C:
      digitalWrite(PIN_PUMP_C, PUMP_ON);
      if (millis() - dosingTimer >= durC) {
        digitalWrite(PIN_PUMP_C, PUMP_OFF);
        currentEC += 0.2; 
        mixTimer = millis();
        currentDosingState = MIX_C;
      }
      break;

    case MIX_C:
      if (millis() - mixTimer >= MIX_DELAY_NUTRIENT) {
        currentDosingState = IDLE;
      }
      break;

    case DOSE_PH:
      digitalWrite(PIN_PUMP_PH, PUMP_ON);
      if (millis() - dosingTimer >= PUMP_PULSE_PH) {
        digitalWrite(PIN_PUMP_PH, PUMP_OFF);
        currentpH -= 0.15; 
        mixTimer = millis();
        currentDosingState = MIX_PH;
      }
      break;

    case MIX_PH:
      if (millis() - mixTimer >= MIX_DELAY_PH) {
        currentDosingState = IDLE;
      }
      break;
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // --- VERTICAL SPLIT (pH vs EC) ---
  display.drawFastVLine(63, 0, 34, SSD1306_WHITE); 

  // ================= LEFT BOX: pH =================
  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print("pH");
  
  display.setCursor(42, 0);
  if (currentpH > targetpH + 0.05) display.print("HI");
  else if (currentpH < targetpH - 0.05) display.print("LO");
  else display.print("OK");

  display.setTextSize(2);
  display.setCursor(2, 10);
  display.print(currentpH, 1);

  display.setTextSize(1);
  display.setCursor(2, 26);
  display.print("Tgt:"); display.print(targetpH, 1);

  // ================= RIGHT BOX: EC =================
  display.setTextSize(1);
  display.setCursor(67, 0);
  display.print("EC");
  
  display.setCursor(107, 0);
  if (currentEC > targetEC + 0.05) display.print("HI");
  else if (currentEC < targetEC - 0.05) display.print("LO");
  else display.print("OK");

  display.setTextSize(2);
  display.setCursor(67, 10);
  display.print(currentEC, 1);

  display.setTextSize(1);
  display.setCursor(67, 26);
  display.print("Tgt:"); display.print(targetEC, 1);

  // --- DIVIDER LINE 1 (Above Stage) ---
  display.drawFastHLine(0, 35, 128, SSD1306_WHITE);

  // ================= STAGE & TEMP ROW =================
  display.setTextSize(1);
  display.setCursor(2, 38);
  if (currentStage == SEED) display.print("[SEED]");
  else if (currentStage == VEG) display.print("[VEG]");
  else display.print("[BLOOM]");
  
  display.setCursor(75, 38);
  display.print(currentTemp, 1);
  display.print("C");

  // --- DIVIDER LINE 2 (Above SYS Status) ---
  display.drawFastHLine(0, 48, 128, SSD1306_WHITE);

  // ================= BOTTOM FOOTER: SYS STATUS =================
  display.setCursor(2, 52);
  if (dosingError) {
    display.print("ERR: LOCKOUT!");
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