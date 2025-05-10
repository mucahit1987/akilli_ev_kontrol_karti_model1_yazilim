#include <Arduino.h>
#include "config.h"          // sensör pin dizileri burada

#define SAMPLE_FREQ   1000   // Hz  (1 ms aralık)
#define TEST_SECONDS  30

static const uint8_t currentPins[] = {
  Y0_CURRENT_SENSOR_PIN,  Y1_CURRENT_SENSOR_PIN,  Y2_CURRENT_SENSOR_PIN,
  Y4_CURRENT_SENSOR_PIN,  Y5_CURRENT_SENSOR_PIN,  Y6_CURRENT_SENSOR_PIN,
  Y8_CURRENT_SENSOR_PIN,  Y9_CURRENT_SENSOR_PIN,  Y10_CURRENT_SENSOR_PIN,
  Y12_CURRENT_SENSOR_PIN, Y13_CURRENT_SENSOR_PIN, Y14_CURRENT_SENSOR_PIN
};

void testSingleSensor(uint8_t pin)
{
  Serial.print(F("\n--- Sensör PIN A"));
  Serial.print(pin);
  Serial.println(F(" test başlıyor ---"));

  const unsigned long tEnd = millis() + TEST_SECONDS * 1000UL;
  unsigned long nextStamp = millis();       // 1 ms örnekleme zamanlayıcısı
  uint32_t sampleCount = 0;

  while (millis() < tEnd)
  {
    if (millis() >= nextStamp)
    {
      int adc = analogRead(pin);
      Serial.println(adc);                  // ham değeri yaz
      nextStamp += 1000UL / SAMPLE_FREQ;
      sampleCount++;
    }
  }
  Serial.print(F("--- "));
  Serial.print(sampleCount);
  Serial.println(F(" örnek alındı ---"));
}

void testAllCurrents()
{
  const uint8_t N = sizeof(currentPins) / sizeof(currentPins[0]);

  for (uint8_t i = 0; i < N; i++)
  {
    testSingleSensor(currentPins[i]);
    delay(500);          // sensörler arası kısa nefes
  }

  Serial.println(F("\nTüm sensör testleri bitti. Reset atarak tekrar başlatabilirsiniz."));
}
