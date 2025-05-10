#include <Arduino.h>
#include "config.h"
#include "pinmap.h"
#include "temperature_control.h"
#include "test_util.h"
#include "current_test.h"

float moduleTemps[4];

void setup() {
  Serial.begin(9600);
  setupAllPins();
  setupZeroCrossInterrupt();     // 💥 ZCD interrupt tanımlandı
  setFanSpeed(30);               // 🔧 Fan hızı %30 başlatıldı
}

void loop() {
  testAllCurrents();
  while (1);   // test bitince dur
}
