#ifndef CROP_PROFILES_H
#define CROP_PROFILES_H

#include <Arduino.h>

// Structure mapping a single day's growth profile, including irrigation limits
struct DailyProfile {
    uint8_t day;              // The specific day number (1 to 45)
    const char* phaseName;    // Name of the current growth phase
    float targetpH;           // pH Target
    float targetEC;           // EC Target
    unsigned long durA;       // Tank N pulse (ms)
    unsigned long durB;       // Tank P pulse (ms)
    unsigned long durC;       // Tank K pulse (ms)
    bool isFlush;             // True = Disable nutrients, fresh water only
    uint8_t irrigationCycles; // How many times per day to water
    uint8_t irrigationMins;   // How many minutes the pump stays on per cycle
};

// Complete 45-Day Coriander Schedule - Explicit Day-by-Day Configuration
const DailyProfile corianderSchedule[] = {
    // --- GERMINATION (Days 1-3) ---
    // Germination phase: Seeds rely on cotyledons. Pure water only, media kept lightly moist, no nutrients or pH adjustment.
    // { day, phaseName, targetpH, targetEC, durN, durP, durK, isFlush, irrigationCycles, irrigationMins }
    { 1,  "Germination", 6.0, 0.0, 0, 0, 0, true, 2, 1 },
    { 2,  "Germination", 6.0, 0.0, 0, 0, 0, true, 2, 1 },
    { 3,  "Germination", 6.0, 0.0, 0, 0, 0, true, 2, 1 },

    // --- EARLY SEEDLING (Days 4-7) ---
    // Minimal watering (2x a day for 1 min) to keep media moist without drowning tiny taproots.
    { 4,  "Seedling D4", 6.0, 0.0, 0, 0, 0, true, 2, 1 },
    { 5,  "Seedling D5", 6.0, 0.0, 0, 0, 0, true, 2, 1 },
    { 6,  "Seedling D6", 6.0, 0.0, 0, 0, 0, true, 2, 1 },
    { 7,  "Seedling D7", 6.0, 0.0, 0, 0, 0, true, 2, 1 },

    // --- LATE SEEDLING (Days 8-14) ---
    // True leaves form. Introduce light nutrients (EC 0.6) and bump watering slightly.
    { 8,  "Seedling D8",  6.0, 0.6, 600, 600, 400, false, 3, 3 },
    { 9,  "Seedling D9",  6.0, 0.6, 600, 600, 400, false, 3, 3 },
    { 10, "Seedling D10", 6.0, 0.6, 600, 600, 400, false, 3, 3 },
    { 11, "Seedling D11", 6.0, 0.6, 600, 600, 400, false, 3, 3 },
    { 12, "Seedling D12", 6.0, 0.6, 600, 600, 400, false, 3, 3 },
    { 13, "Seedling D13", 6.0, 0.6, 600, 600, 400, false, 3, 3 },
    { 14, "Seedling D14", 6.0, 0.6, 600, 600, 400, false, 3, 3 },

    { 15, "Trans Veg",  6.0, 1.0, 1000, 800, 800, false, 4, 5 },

    // --- EARLY VEGETATIVE (Days 16-25) ---
    // High Nitrogen focus. Watering bumped to 5x a day for 8 minutes.
    { 16, "Early Veg D16", 6.1, 1.2, 1200, 1000, 1000, false, 5, 8 },
    { 17, "Early Veg D17", 6.1, 1.2, 1200, 1000, 1000, false, 5, 8 },
    { 18, "Early Veg D18", 6.1, 1.2, 1200, 1000, 1000, false, 5, 8 },
    { 19, "Early Veg D19", 6.1, 1.2, 1200, 1000, 1000, false, 5, 8 },
    { 20, "Early Veg D20", 6.1, 1.2, 1200, 1000, 1000, false, 5, 8 },
    { 21, "Early Veg D21", 6.1, 1.2, 1200, 1000, 1000, false, 5, 8 },
    { 22, "Early Veg D22", 6.1, 1.2, 1200, 1000, 1000, false, 5, 8 },
    { 23, "Early Veg D23", 6.1, 1.2, 1200, 1000, 1000, false, 5, 8 },
    { 24, "Early Veg D24", 6.1, 1.2, 1200, 1000, 1000, false, 5, 8 },
    { 25, "Early Veg D25", 6.1, 1.2, 1200, 1000, 1000, false, 5, 8 },

    // --- LATE VEGETATIVE (Days 26-34) ---
    // Rapid foliage expansion. Max vegetative EC. Watering 6x a day for 10 minutes.
    { 26, "Late Veg D26", 6.2, 1.5, 1500, 1200, 1400, false, 6, 10 },
    { 27, "Late Veg D27", 6.2, 1.5, 1500, 1200, 1400, false, 6, 10 },
    { 28, "Late Veg D28", 6.2, 1.5, 1500, 1200, 1400, false, 6, 10 },
    { 29, "Late Veg D29", 6.2, 1.5, 1500, 1200, 1400, false, 6, 10 },
    { 30, "Late Veg D30", 6.2, 1.5, 1500, 1200, 1400, false, 6, 10 },
    { 31, "Late Veg D31", 6.2, 1.5, 1500, 1200, 1400, false, 6, 10 },
    { 32, "Late Veg D32", 6.2, 1.5, 1500, 1200, 1400, false, 6, 10 },
    { 33, "Late Veg D33", 6.2, 1.5, 1500, 1200, 1400, false, 6, 10 },
    { 34, "Late Veg D34", 6.2, 1.5, 1500, 1200, 1400, false, 6, 10 },
    { 35, "Late Veg D35", 6.2, 1.5, 1500, 1200, 1400, false, 6, 10 },

    // --- MATURE GROWTH (Days 36-42) ---
    // Peak EC to force essential oil production. Heavy transpiration demands 8x watering.
    { 36, "Mature D36",   6.2, 1.8, 1800, 1500, 1600, false, 8, 10 },
    { 37, "Mature D37",   6.2, 1.8, 1800, 1500, 1600, false, 8, 10 },
    { 38, "Mature D38",   6.2, 1.8, 1800, 1500, 1600, false, 8, 10 },
    { 39, "Mature D39",   6.2, 1.8, 1800, 1500, 1600, false, 8, 10 },
    { 40, "Mature D40",   6.2, 1.8, 1800, 1500, 1600, false, 8, 10 },
    { 41, "Mature D41",   6.2, 1.8, 1800, 1500, 1600, false, 8, 10 },
    { 42, "Mature D42",   6.2, 1.8, 1800, 1500, 1600, false, 8, 10 },
    { 43, "Mature D43",   6.2, 1.8, 1800, 1500, 1600, false, 8, 10 },
    { 44, "Mature D44",   6.2, 1.8, 1800, 1500, 1600, false, 8, 10 },
    { 45, "Mature D45",   6.2, 1.8, 1800, 1500, 1600, false, 8, 10 }
};

// Dynamically calculates to 45 so we can safely bounds-check the array
const int TOTAL_DAYS = sizeof(corianderSchedule) / sizeof(corianderSchedule[0]);

#endif // CROP_PROFILES_H