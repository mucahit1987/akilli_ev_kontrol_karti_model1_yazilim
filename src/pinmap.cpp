#include <Arduino.h>
#include "config.h"
#include "pinmap.h"

void setupAllPins() {
  // -------------------------
  // 🟧 Yüksek güçlü çıkışlar
  // -------------------------
  int yPins[] = {
    Y0_PIN, Y1_PIN, Y2_PIN, Y3_PIN, Y4_PIN, Y5_PIN, Y6_PIN, Y7_PIN,
    Y8_PIN, Y9_PIN, Y10_PIN, Y11_PIN, Y12_PIN, Y13_PIN, Y14_PIN, Y15_PIN
  };
  for (int i = 0; i < 16; i++) {
    pinMode(yPins[i], OUTPUT);
    digitalWrite(yPins[i], LOW);
  }

  // -------------------------
  // 🟦 Düşük güçlü çıkışlar
  // -------------------------
  int xPins[] = {
    X0_PIN, X1_PIN, X2_PIN, X3_PIN, X4_PIN, X5_PIN, X6_PIN, X7_PIN,
    X8_PIN, X9_PIN, X10_PIN, X11_PIN, X12_PIN, X13_PIN, X14_PIN, X15_PIN
  };
  for (int i = 0; i < 16; i++) {
    pinMode(xPins[i], OUTPUT);
    digitalWrite(xPins[i], LOW);
  }

  // -------------------------
  // 🌬️ Fan çıkışı
  // -------------------------
  pinMode(FAN_TRIAC_PIN, OUTPUT);
  digitalWrite(FAN_TRIAC_PIN, LOW);

  // -------------------------
  // 🧠 Zero Cross giriş
  // -------------------------
  pinMode(ZERO_CROSS_PIN, INPUT);

  // -------------------------
  // 📈 Akım sensörü girişleri
  // -------------------------
  int currentSensorPins[] = {
    Y0_CURRENT_SENSOR_PIN, Y1_CURRENT_SENSOR_PIN, Y2_CURRENT_SENSOR_PIN,
    Y4_CURRENT_SENSOR_PIN, Y5_CURRENT_SENSOR_PIN, Y6_CURRENT_SENSOR_PIN,
    Y8_CURRENT_SENSOR_PIN, Y9_CURRENT_SENSOR_PIN, Y10_CURRENT_SENSOR_PIN,
    Y12_CURRENT_SENSOR_PIN, Y13_CURRENT_SENSOR_PIN, Y14_CURRENT_SENSOR_PIN
  };
  for (int i = 0; i < 12; i++) {
    pinMode(currentSensorPins[i], INPUT);
  }

  // -------------------------
  // 🌡️ Termistör girişleri
  // -------------------------
  int thermistorPins[] = {
    MODULE1_THERMISTOR_PIN, MODULE2_THERMISTOR_PIN,
    MODULE3_THERMISTOR_PIN, MODULE4_THERMISTOR_PIN
  };
  for (int i = 0; i < 4; i++) {
    pinMode(thermistorPins[i], INPUT);
  }
}
