/**
 * temperature_control.cpp — Rev 7.1 (clean, readable)
 * ------------------------------------------------------------
 *  › 4 modül (Y‑grupları) için 3‑kademeli fan + aşırı ısınma koruması
 *    – Her modül bağımsız kilitlenip (Lvl‑3) çözülebilir
 *    – Kilitlenirken açık pin maskesi saklanır; soğuyunca yalnız o pinler açılır
 *    – Terminal override: “T<mod> <deg>/OFF”   (örn. T1 55 ↵ / T1 OFF ↵)
 *    – Home Assistant bildirimleri: haNotify()
 *
 *  Müco / ChatGPT (3 Haz 2025)
 * ------------------------------------------------------------*/

#include <Arduino.h>
#include "config.h"
#include "temperature_control.h"
#include "pinmap.h"
#include "mqtt_haberlesme.h"
#include "tanimlamalar.h"

// ---- Yardımcılar (dosyanın başına uygun bir yere) ----
extern void cs_pauseADC();
extern void cs_resumeADC();

static inline uint16_t readADC_fast(uint8_t pin) {
  (void)analogRead(pin);      // MUX otursun diye boş okuma
  return analogRead(pin);     // gerçek örnek
}

// Opsiyonel: tek atımlık sapmaları söndürmek için median-of-3
static inline uint16_t med3(uint16_t a, uint16_t b, uint16_t c) {
  if (a > b) { uint16_t t=a; a=b; b=t; }
  if (b > c) { uint16_t t=b; b=c; c=t; }
  if (a > b) { uint16_t t=a; a=b; b=t; }
  return b;
}


//--------------------------------------------------------------
//  İLERİ BİLDİRİMLER (forward declarations)
//--------------------------------------------------------------

void zeroCrossISR();                         // ZCD kesmesi ISR
void setFanSpeed(uint8_t pct);               // Fan PWM fonksiyonu
static uint8_t fanLevel = 0;   // 0 = kapalı, 1 = PWM (%), 2 = tam hız

//--------------------------------------------------------------
//  KONFİG‑MAKROLAR‑MAKROLAR
//--------------------------------------------------------------
#define TEMP_SERIAL_DEBUG   1      // 1 → Seri monitöre sıcaklık yaz
#define ADC_MAX             1023.0f
#define R0_25C              10000.0f    // 25 °C NTC direnci (Ω)
#define BETA_NTC            3950.0f
#define R_SERIES            10000.0f    // Bölücü seri direnci (Ω)

//--------------------------------------------------------------
//  HARİCİ GLOBALLER
//--------------------------------------------------------------
extern volatile bool moduleLocked[8];   // main.cpp içinde TANIMLI

//--------------------------------------------------------------
//  DAHİLİ GLOBAL DURUMLAR
//--------------------------------------------------------------
static bool     zcdEnabled   = false;   // Zero‑cross kesmesi açık mı?
static uint16_t gateDelayUs  = 9800;    // 0 % → 9800 µs   100 % → 200 µs
static volatile bool     zcFlag      = false; // Yeni yarım periyot geldi mi?
static volatile uint32_t zcTimestamp = 0;     // micros() damgası

static bool anyLocked = false;                 // Bir modül bile kilitli mi?

/* Sensör & override dizileri */
static float realTemp[4]     = {0};            // NTC’den okunan °C
static float overrideTemp[4] = {0};            // Terminal değeri
static bool  overrideEn [4]  = {false};        // true → override aktif

/* Kilitlenme öncesi açık pin maskesi (bit0 → Y0/Y4/…) */
static uint8_t preMask[4]    = {0};

//--------------------------------------------------------------
//  YARDIMCI FONKSİYONLAR
//--------------------------------------------------------------
/** ADC → °C */
static float adcToC(uint16_t adc)
{
    if (adc == 0 || adc >= 1023) return NAN;         // kopuk / kısa devre

    float rNTC = R_SERIES * (float)adc / (ADC_MAX - (float)adc);
    float invT = 1.0f / 298.15f + log(rNTC / R0_25C) / BETA_NTC;
    return (1.0f / invT) - 273.15f;                  // Kelvin → °C
}

static float readNTC(uint8_t pin) { return adcToC(analogRead(pin)); }

static inline float readNTC_median3_atomic(uint8_t pin) {
  cs_pauseADC();                               // Timer1 akım ISR'ını duraklat
  uint16_t x = readADC_fast(pin);
  uint16_t y = readADC_fast(pin);
  uint16_t z = readADC_fast(pin);
  cs_resumeADC();                              // tekrar devreye al
  uint16_t m = med3(x, y, z);
  return adcToC(m);
}

/** 4 Y‑çıkışını topluca aç/kapat */
static void switchModule(uint8_t mod, bool on)
{
    uint8_t base = mod * 4;
    for (uint8_t i = 0; i < 4; ++i) {
        uint8_t idx = base + i;
        digitalWrite(yMap[idx], on ? HIGH : LOW);
        pinState[idx] = on;
        dirty[idx]    = true;
    }
}
inline void disableModule(uint8_t m) { switchModule(m, false); }
inline void enableModule (uint8_t m) { switchModule(m, true ); }

/** ZCD kesmesini yönet */
static void attachZCD()
{ attachInterrupt(digitalPinToInterrupt(ZERO_CROSS_PIN), zeroCrossISR, RISING); zcdEnabled = true; }
static void detachZCD()
{ detachInterrupt(digitalPinToInterrupt(ZERO_CROSS_PIN)); zcdEnabled = false; }

//--------------------------------------------------------------
//  KURULUM
//--------------------------------------------------------------
void initTemperatureControl()
{
    pinMode(FAN_TRIAC_PIN, OUTPUT);
    digitalWrite(FAN_TRIAC_PIN, LOW);

    pinMode(ZERO_CROSS_PIN, INPUT);
    attachZCD();          // kesme hazırla (başta hemen kapatılacak)
    detachZCD();

    Serial.println(F("[T cmd]  T<mod> <deg>  |  T<mod> OFF"));
}

//--------------------------------------------------------------
//  ANA GÜNCELLEME (≈ 2 s)
//--------------------------------------------------------------
void updateTemperatureControl()
{
    //----------------------------------
    // 1) Terminal komutlarını işle
    //----------------------------------
    if (Serial.available()) {
        String ln = Serial.readStringUntil('\n'); ln.trim();
        if (ln.length() >= 2 && ln[0] == 'T') {
            int m = ln[1] - '0'; if (m >= 0 && m <= 3) {
                if (ln.endsWith("OFF")) {
                    overrideEn[m] = false;
                } else {
                    float v = ln.substring(2).toFloat();
                    overrideEn[m]   = true;
                    overrideTemp[m] = v;
                }
            }
        }
    }

    //----------------------------------
    // 2) Sıcaklık okuma
    //----------------------------------
    realTemp[0] = readNTC_median3_atomic(MODULE1_THERMISTOR_PIN);
    realTemp[1] = readNTC_median3_atomic(MODULE2_THERMISTOR_PIN);
    realTemp[2] = readNTC_median3_atomic(MODULE3_THERMISTOR_PIN);
    realTemp[3] = readNTC_median3_atomic(MODULE4_THERMISTOR_PIN);

    float T[4];
    for (uint8_t i = 0; i < 4; ++i)
        T[i] = overrideEn[i] ? overrideTemp[i] : realTemp[i];

#if TEMP_SERIAL_DEBUG
    Serial.print(F("T[°C]: "));
    for (uint8_t i = 0; i < 4; ++i) {
        Serial.print(T[i], 1);
        if (i < 3) Serial.print(',');
    }
    Serial.print(F("  fanLevel="));
    Serial.println(fanLevel);     // 0,1,2 olarak yazar

#endif

    //----------------------------------
    // 3) En sıcak modül + fan hızı
    //----------------------------------
    uint8_t hot = 0;
    for (uint8_t i = 1; i < 4; ++i) if (T[i] > T[hot]) hot = i;
    float Tmax = T[hot];

    /*-------------------------------------------------------------*/
/*  F A N   S E V İ Y E   S E Ç İ M İ  (Histerezisli)          */
/*-------------------------------------------------------------*/
switch (fanLevel)
{
case 0:   // Kapalı
    if (Tmax >= FAN_LVL1_IN_C) {
        fanLevel = 1;                     // 0 ➜ 1
        attachZCD();
        setFanSpeed(FAN_LVL1_SPEED_PCT);
    }
    break;

case 1:   // PWM
    if (Tmax >= FAN_LVL2_IN_C) {
        fanLevel = 2;                     // 1 ➜ 2
        detachZCD();
        digitalWrite(FAN_TRIAC_PIN, HIGH);
    }
    else if (Tmax <= FAN_LVL1_OUT_C) {
        fanLevel = 0;                     // 1 ➜ 0
        detachZCD();
        digitalWrite(FAN_TRIAC_PIN, LOW);
    }
    break;

case 2:   // Tam hız
    if (Tmax <= FAN_LVL2_OUT_C) {
        fanLevel = 1;                     // 2 ➜ 1
        attachZCD();
        setFanSpeed(FAN_LVL1_SPEED_PCT);
    }
    break;
}


    //----------------------------------
    // 4) Lvl‑3 kilitleme (modül bazlı)
    //----------------------------------
    if (Tmax >= FAN_LVL3_LIMIT_C && !moduleLocked[hot]) {
        /* Açık pin maskesini sakla */
        uint8_t mask = 0;
        for (uint8_t i = 0; i < 4; ++i) {
            uint8_t idx = hot * 4 + i;
            if (pinState[idx]) mask |= (1 << i);
        }
        preMask[hot] = mask;

        /* Kilit & pinleri kapat */
        moduleLocked[hot] = true;
        disableModule(hot);

        mqttPublishAlert(hot, Tmax, true);
        haNotify("Aşırı Isınma", "Modül kapatıldı");
    }

    //----------------------------------
    // 5) Kilit çözme (her modül ayrı)
    //----------------------------------
    for (uint8_t m = 0; m < 4; ++m) {
        if (moduleLocked[m] && T[m] <= FAN_LVL3_LIMIT_C - FAN_RESTORE_HYST) {
            moduleLocked[m] = false;

            /* Yalnız önceden açık pinleri yeniden HIGH yap */
            for (uint8_t i = 0; i < 4; ++i) {
                uint8_t idx = m * 4 + i;
                bool on = preMask[m] & (1 << i);
                digitalWrite(yMap[idx], on ? HIGH : LOW);
                pinState[idx] = on;
                dirty[idx]    = true;
            }
            mqttPublishAlert(m, T[m], false);
            haNotify("Isı Normal", "Modül açıldı");
        }
    }

    //----------------------------------
    // 6) anyLocked güncelle (fan tam hızda kalsın)
    //----------------------------------
    anyLocked = false;
    for (uint8_t m = 0; m < 4; ++m) if (moduleLocked[m]) anyLocked = true;
}

//--------------------------------------------------------------
//  ZCD → bayrak + tetik servisleri
//--------------------------------------------------------------
void zeroCrossISR() { zcTimestamp = micros(); zcFlag = true; }

void serviceTriac()
{   
    if (fanLevel == 2) return;
    if (fanLevel == 0) return;
    if (!zcFlag) return;
    if (micros() - zcTimestamp < gateDelayUs) return;

    digitalWrite(FAN_TRIAC_PIN, HIGH);
    delayMicroseconds(100);
    digitalWrite(FAN_TRIAC_PIN, LOW);

    zcFlag = false;
}

void setFanSpeed(uint8_t pct)
{
    pct = constrain(pct, 0, 100);
    gateDelayUs = map(100 - pct, 0, 100, 200, 9800);
}

/*********************************************************************/
