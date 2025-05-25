#include <Arduino.h>
#include "config.h"
#include "pinmap.h"
#include "temperature_control.h"
#include "mqtt_haberlesme.h"
#include "test_util.h"
#include <avr/wdt.h>
#include "tanimlamalar.h"


uint32_t lastFanMs = 0;

bool pinState[32];          // Çıkışın son durumu
volatile bool dirty[32];    // Yayınlanacaklar kuyruğu

void setup() {
  Serial.begin(9600);
  setupAllPins();                     // Röleler & triac çıkışları
  mqttInit();                         // Ethernet + MQTT
  mqttPublishDiscovery();             // HA’ya ilk tanıtım
  //setupZeroCrossInterrupt();          // Fan faz kontrolü
  wdt_enable(WDTO_2S);
}

void loop() {
  mqttLoop();                         // MQTT canlı tut
  mqttProcessStateQueue();    // dirty[] kuyruğunu boşalt
  

  // ---- Fan kontrol (2 s’de bir) ----
  //if (millis()-lastFanMs>2000) {
  //    float temps[4]; readAllModuleTemperatures(temps);
  //    uint8_t duty = map(constrain(max(max(temps[0],temps[1]),max(temps[2],temps[3])),35,70),35,70,10,100);
  //    setFanSpeed(duty);
  //    lastFanMs = millis();
  //}

  wdt_reset();
}