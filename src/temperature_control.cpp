#include "temperature_control.h"
#include "config.h"
#include <math.h> // log()

volatile uint8_t fanSpeed = 0;

void setFanSpeed(uint8_t percent) {
  if (percent > 100) percent = 100;
  fanSpeed = percent;
}


float readTemperature(uint8_t analogPin) {
  const float BETA = 3950.0;
  const float R0 = 10000.0;
  const float SERIES_RESISTOR = 10000.0;
  const float ADC_MAX = 1023.0;

  int adcValue = analogRead(analogPin);

  if (adcValue <= 0) return -1000.0;
  if (adcValue >= 1023) return 1000.0;

  float voltageRatio = (ADC_MAX / adcValue) - 1.0;
  float resistance = SERIES_RESISTOR / voltageRatio;

  float temperatureKelvin = 1.0 / (1.0 / 298.15 + log(resistance / R0) / BETA);
  float temperatureCelsius = temperatureKelvin - 273.15;

  return temperatureCelsius;
}

void readAllModuleTemperatures(float temps[4]) {
  temps[0] = readTemperature(MODULE1_THERMISTOR_PIN);
  temps[1] = readTemperature(MODULE2_THERMISTOR_PIN);
  temps[2] = readTemperature(MODULE3_THERMISTOR_PIN);
  temps[3] = readTemperature(MODULE4_THERMISTOR_PIN);
}

// 🧠 Zero Cross ISR
void zeroCrossISR() {
    // Gecikmeyi % hız değerine göre hesapla
    // 100% hız → 200µs gecikme, 0% hız → 9800µs gecikme
    uint16_t delayTime = map(fanSpeed, 0, 100, 9800, 200);
  
    delayMicroseconds(delayTime);
  
    // Triyakı tetikle (fan açılır)
    digitalWrite(FAN_TRIAC_PIN, HIGH);
    delayMicroseconds(100);  // Gate pulse süresi
    digitalWrite(FAN_TRIAC_PIN, LOW);
  }
  
  // 🔧 Setup fonksiyonu içinde çağrılacak tanım
  void setupZeroCrossInterrupt() {
    attachInterrupt(digitalPinToInterrupt(ZERO_CROSS_PIN), zeroCrossISR, RISING);
  }