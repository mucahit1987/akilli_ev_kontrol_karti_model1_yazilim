#pragma once
#include <Arduino.h>

#define NUM_Y_CHANNELS 16
extern volatile float Y_current[NUM_Y_CHANNELS];   // Irms  (A)
extern volatile float Y_powerW [NUM_Y_CHANNELS];   // P     (W)
extern volatile float Y_energyWh[NUM_Y_CHANNELS];  // ∑Wh

void  initCurrentSense();          // Timer + kalibrasyon
void  sampleCurrentSensors();      // 10 ms’de bir (Irms hesabı)
void  energyTick(uint32_t dtMs);   // dt = ms  (ana döngüde 1 s)
float getIrms (uint8_t ch);        // A
float getPower(uint8_t ch);        // W
float getEnergy(uint8_t ch);       // Wh
