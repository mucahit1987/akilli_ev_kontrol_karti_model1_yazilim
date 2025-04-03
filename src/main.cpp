#include <Arduino.h>
#include "config.h"
#include "pinmap.h"
#include "temperature_control.h"

float moduleTemps[4];

void setup() {
  Serial.begin(9600);
  setupAllPins();
  setupZeroCrossInterrupt();     // 💥 ZCD interrupt tanımlandı
  setFanSpeed(30);               // 🔧 Fan hızı %50 başlatıldı
}

void loop() {
  
}
