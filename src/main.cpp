/*********************************************************************
 *  main.cpp  –  Merisoft Kontrol Kartı  (Arduino Mega 2560)
 * -------------------------------------------------------------------
 *  • setupAllPins()            → tüm çıkışları LOW yapar
 *  • MQTT/Ethernet başlatır    → mqttInit / mqttLoop
 *  • Fan & sıcaklık FSM’i      → updateTemperatureControl  (≈2 s)
 *  • Triyak tetiklemesi        → serviceTriac (her döngü)
 *  • Watch-Dog (2 s)           → kilitlenmeye karşı güvence
 *********************************************************************/

#include <Arduino.h>
#include <avr/wdt.h>

#include "config.h"
#include "pinmap.h"               // setupAllPins()
#include "temperature_control.h"  // init… / update… / serviceTriac
#include "mqtt_haberlesme.h"      // mqttInit / mqttLoop / mqttProcess…
#include "tanimlamalar.h"         // pinState[] / dirty[] global dizileri
#include "current_sense.h"   

bool          pinState[32] = {false};     // Home Assistant gösterimi
volatile bool dirty[32]    = {false};     // Publish kuyruğu işareti

volatile bool moduleLocked[8] = {false};   // hepsi açık


/* ---------- Zamanlayıcılar ---------- */
static uint32_t tFan = 0;          // Son fan-FSM güncellemesi (ms)

static uint32_t tCurrent = 0;   // 10 ms Irms
static uint32_t tEnergy  = 0;   // 1000 ms enerji
static uint32_t tMqtt    = 0;   // 10 s MQTT publish

/*********************************************************************
 *  SETUP  –  yalnızca 1 kez
 *********************************************************************/
void setup()
{
    Serial.begin(9600);

    /* 1) Donanım pinlerini hazırla */
    setupAllPins();                        // Röle-triyak çıkışlarını LOW

    /* 2) Fan kontrol alt-sistemi */
    initTemperatureControl();              // ZCD pini + triyağı yapılandır

    /* 3) Ethernet + MQTT */
    mqttInit();                            // ENC28J60 & broker bağlantısı
    mqttPublishDiscovery();                // Home Assistant auto-discovery

    initCurrentSense(); // akım ölçümü başlat

    

    /* 4) Watch-Dog (2 s) */
    wdt_enable(WDTO_2S);
}

/*********************************************************************
 *  LOOP  –  sonsuz döngü
 *********************************************************************/
void loop()
{
    /****  A) Triyak tetiklemesini servis et  ****/
    serviceTriac();                        // Her döngüde çağır ↻

    /****  B) MQTT işle  ****/
    mqttLoop();                            // mesaj al-gönder
    mqttProcessStateQueue();               // kirli çıkışları publish et

    /****  C) Fan & sıcaklık FSM’i  ****/
    if (millis() - tFan >= 2000)           // ≈ 2 s
    {
        updateTemperatureControl();
        tFan = millis();
    }

//    akım sensörleri ksımı güç hesaplama vs.

          /* 10 ms: Irms örneklerini topla */
    if (millis() - tCurrent >= 10) {
        tCurrent = millis();
        sampleCurrentSensors();
    }

    /* 1 s: güç + enerji entegrasyonu */
    if (millis() - tEnergy >= 1000) {
        uint32_t dt = millis() - tEnergy;
        tEnergy = millis();
        energyTick(dt);               // Wh güncelle
    }

    /* 10 s: MQTT’ye gönder */
    if (millis() - tMqtt >= 10000) {  // her 10 s
        tMqtt = millis();
        mqttPublishPowerEnergy();     // AZ SONRA tanımlayacağız
    }

    wdt_reset();
}
