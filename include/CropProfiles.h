#ifndef CROP_PROFILES_H
#define CROP_PROFILES_H

struct CropProfile {
  const char* cropName;
  const char* stageName;
  float targetpH;
  float targetEC;
  unsigned long durA; // ms
  unsigned long durB; // ms
  unsigned long durC; // ms
};

const CropProfile cropDatabase[] = {
  // Fruiting Vegetables
  {"Tomato",     "Seedling",   6.0, 1.2, 1500, 1500, 1000},
  {"Tomato",     "Vegetative", 6.0, 2.0, 3500, 2000, 2000},
  {"Tomato",     "Bloom",      6.2, 3.0, 2000, 4500, 5000},
  {"Capsicum",   "Vegetative", 6.0, 1.6, 3000, 1800, 1800},
  {"Capsicum",   "Bloom",      6.2, 2.2, 1800, 3500, 4000},
  {"Cucumber",   "Vegetative", 5.8, 1.7, 3000, 2000, 2000},
  {"Cucumber",   "Fruiting",   6.0, 2.2, 2500, 3000, 3500},

  // Leafy Greens & Herbs
  {"Palak",      "Full",       6.5, 2.0, 2500, 1500, 2000},
  {"Coriander",  "Full",       6.0, 1.4, 1500, 1000, 1200},
  {"Lettuce",    "Full",       5.8, 1.3, 1200,  800, 1000},
  {"Mint",       "Full",       6.4, 1.8, 2000, 1200, 1800},

  // Fruit & Solanaceous
  {"Strawberry", "Vegetative", 5.8, 1.2, 1500, 1000, 1000},
  {"Strawberry", "Fruit",      6.0, 1.6, 1000, 2500, 3000},
  {"Chilli",     "Vegetative", 6.0, 1.7, 3000, 1800, 1800},
  {"Chilli",     "Bloom",      6.2, 2.5, 2000, 3500, 4000}
};

const int TOTAL_CROPS = sizeof(cropDatabase) / sizeof(cropDatabase[0]);

#endif
