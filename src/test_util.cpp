#include <Arduino.h>
#include "config.h"

void testAllOutputs() {
  int outputPins[] = {
    // 🟧 Yüksek güçlü çıkışlar
    Y0_PIN, Y1_PIN, Y2_PIN, Y3_PIN, Y4_PIN, Y5_PIN, Y6_PIN, Y7_PIN,
    Y8_PIN, Y9_PIN, Y10_PIN, Y11_PIN, Y12_PIN, Y13_PIN, Y14_PIN, Y15_PIN,

    // 🟦 Düşük güçlü çıkışlar
    X0_PIN, X1_PIN, X2_PIN, X3_PIN, X4_PIN, X5_PIN, X6_PIN, X7_PIN,
    X8_PIN, X9_PIN, X10_PIN, X11_PIN, X12_PIN, X13_PIN, X14_PIN, X15_PIN
  };

  const int count = sizeof(outputPins) / sizeof(outputPins[0]);

  for (int i = 0; i < count; i++) {
    digitalWrite(outputPins[i], HIGH);
    Serial.print("Pin HIGH: ");
    Serial.println(outputPins[i]);
    delay(20);

    digitalWrite(outputPins[i], LOW);
    Serial.print("Pin LOW: ");
    Serial.println(outputPins[i]);
    //delay(5);
  }
}
