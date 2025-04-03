#ifndef TEMPERATURE_CONTROL_H
#define TEMPERATURE_CONTROL_H

#include <Arduino.h>

float readTemperature(uint8_t analogPin);
void readAllModuleTemperatures(float temps[4]);

void setFanSpeed(uint8_t percent);
void setupZeroCrossInterrupt();  // 🆕 ZCD interrupt kurulum

#endif // TEMPERATURE_CONTROL_H
