// current_sense.cpp
// ------------------------------------------------------------
// Akım Ölçüm & Güç Hesaplama Modülü
// Donanım   : Arduino Mega 2560 + 12 × ACS712‑20 A (Y0–Y14)
// Sensör Yok: Y3, Y7, Y11, Y15 (toplam 4 kanal boş)
// Yazar     : ChatGPT (Müco sürümü)
// Amaç      :
//   1. Her çıkış (Y0‒Y15) için Irms (RMS akım) hesaplamak
//   2. Sonuçları okunabilir global float değişkenlerine yazmak
//   3. Kodun **anlaşılır** olması; satır sayısı yüksek olabilir 🤓
// ------------------------------------------------------------
// **Kavramsal Özet**
// • 50 Hz şebeke -> 20 ms periyot
// • Timer1 -> 4 kHz kesme (her 250 µs). Böylece 20 ms / 250 µs = 80 örnek
//   Her kanaldan bir periyotta 80 örnek alırsak Irms hatası < %1
// • Irms formülü:  Irms = sqrt( Σ(i²) / N )
//   - i: "gerçek akım" değil, ADC ham değeri – ofset
//   - ADC ham değeri 0‑1023 (10 bit, 5 V referans)
//   - ACS712‑20 A duyarlılığı ≈ 100 mV/A, orta nokta 2.5 V
//   - Ölçeğe dönüştürmek için: Volt = ADC * 5.0 / 1023
//                               Akım = (Volt – 2.5 V) / 0.100 V/A
// • Ofset (2.5 V) her sensörde ±1–2 LSB kayabilir ⇒ Boot'ta otomatik kalibrasyon
// • ISR sadece **toplam kare** ve **örnek sayısı** toplar → ana döngüde Irms
// ------------------------------------------------------------
// KULLANICI ARAYÜZÜ  (current_sense.h içinde deklare edilir)
// ------------------------------------------------------------
//   void initCurrentSense();           // Başlat & kalibre et & Timer aç
//   void sampleCurrentSensors();       // Ana loop'ta 10 ms'de bir çağır (Irms hesabı)
//   float getIrms(uint8_t yIndex);     // 0‑15 ⇒ amper (ölçüm yoksa 0.0)
// ------------------------------------------------------------

#include <Arduino.h>
#include "config.h"           // Y çıkış & sensör pinleri burada
#include "current_sense.h"    // Bu modülün publik prototipleri

volatile float Y_powerW [NUM_Y_CHANNELS] = {0};
volatile float Y_energyWh[NUM_Y_CHANNELS] = {0};

// --- getPower / getEnergy inline tanımları ---
float getPower (uint8_t ch) { return Y_powerW [ch]; }
float getEnergy(uint8_t ch) { return Y_energyWh[ch]; }

// ------ 1. Sabitler --------------------------------------------------
#define NUM_Y_CHANNELS 16            // Y0‑Y15
#define ANALOG_INVALID  0xFF         // curMap'te sensör olmayan işaretçi

// ADC → Volt → Amper dönüşümü için sabitler
static const float ADC_TO_VOLT = 5.0f / 1023.0f;   // 5 V referans, 10 bit ADC
static const float VOLT_TO_AMP = 1.0f / 0.100f;    // 100 mV/A (ACS712‑20 A)
static const float ADC_TO_AMP  = ADC_TO_VOLT * VOLT_TO_AMP; // birleşik katsayı

// 50 Hz periyotta örnek sayısı (4 kHz / 50 Hz = 80)
static const uint16_t SAMPLES_PER_PERIOD = 80;

// ------ 2. Donanım Haritası -----------------------------------------
// curMap[index] == ANALOG_INVALID ⇒ o Y kanalında sensör yok.
//  Arduino Mega'da A0 ≡ 54, ...  A15 ≡ 69.
static const uint8_t curMap[NUM_Y_CHANNELS] PROGMEM = {
  A1,  A3,  A5,  ANALOG_INVALID,   // Y0–Y3
  A9,  A11, A13, ANALOG_INVALID,   // Y4–Y7
  A8,  A10, A12, ANALOG_INVALID,   // Y8–Y11
  A0,  A2,  A4,  ANALOG_INVALID    // Y12–Y15
};

// ------ 3. Değişkenler ----------------------------------------------
static volatile uint32_t accSq[NUM_Y_CHANNELS];   // Σ(i²)  (ham ADC)
static volatile uint16_t sampleCnt[NUM_Y_CHANNELS];
static volatile uint8_t  curChanISR = 0;          // O anda örneklenen kanal

static uint16_t offsetADC[NUM_Y_CHANNELS];        // Kalibrasyon ofsetleri

volatile float Y_current[NUM_Y_CHANNELS];         // Irms sonucu (amper)

// ------ 4. Yardımsı Fonksiyonlar ------------------------------------
inline uint8_t nextValidChannel(uint8_t ch) {
  for (uint8_t k = 0; k < NUM_Y_CHANNELS; k++) {
    uint8_t idx = (ch + k) & 0x0F;      // 0‑15 çevrimi
    if (pgm_read_byte(curMap + idx) != ANALOG_INVALID) {
      return idx;
    }
  }
  return 0; // ulaşılmaz
}

// ------ 5. Timer1 Kesmesi -------------------------------------------
ISR(TIMER1_COMPA_vect) {
  curChanISR = nextValidChannel(curChanISR);
  uint8_t analogPin = pgm_read_byte(curMap + curChanISR);

  uint16_t raw = analogRead(analogPin);
  int32_t diff = (int32_t)raw - (int32_t)offsetADC[curChanISR];
  accSq[curChanISR]     += (uint32_t)(diff * diff);
  sampleCnt[curChanISR]++;

  curChanISR = (curChanISR + 1) & 0x0F;  // sıradaki kanal
}

static void setupTimer1() {
  cli();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;
  OCR1A  = 499;            // 4 kHz
  TCCR1B |= (1 << WGM12);  // CTC
  TCCR1B |= (1 << CS11);   // prescaler 8
  TIMSK1 |= (1 << OCIE1A); // interrupt enable
  sei();
}

// ------ 6. Kamuya Açık Fonksiyonlar ---------------------------------
void initCurrentSense() {
  for (uint8_t i = 0; i < NUM_Y_CHANNELS; i++) {
    offsetADC[i] = 0;
    accSq[i]     = 0;
    sampleCnt[i] = 0;
    Y_current[i] = 0.0f;
  }

  for (uint8_t i = 0; i < NUM_Y_CHANNELS; i++) {
    uint8_t pin = pgm_read_byte(curMap + i);
    if (pin != ANALOG_INVALID) pinMode(pin, INPUT);
  }

  const uint16_t CAL_SAMPLES = 200;
  for (uint8_t ch = 0; ch < NUM_Y_CHANNELS; ch++) {
    uint8_t pin = pgm_read_byte(curMap + ch);
    if (pin == ANALOG_INVALID) continue;

    uint32_t sum = 0;
    for (uint16_t s = 0; s < CAL_SAMPLES; s++) {
      sum += analogRead(pin);
      delayMicroseconds(200);
    }
    offsetADC[ch] = sum / CAL_SAMPLES;
  }

  // >>> BURAYA DEBUG SATIRLARINI EKLE <<<
    Serial.print(F("Offset Y0 raw = "));
    Serial.println(offsetADC[0]);   // gerekirse diğer kanallar da

  setupTimer1();
}

// dosyanın uygun bir yerine ekle (ör. setupTimer1() fonksiyonunun hemen altı)
static volatile uint8_t cs_pause_depth = 0;

void cs_pauseADC() {
  uint8_t s = SREG; cli();
  if (cs_pause_depth == 0) {
    // Timer1 Compare A kesmesini kapat (ISR(TIMER1_COMPA_vect) çalışmasın)
    TIMSK1 &= ~(1 << OCIE1A);
  }
  cs_pause_depth++;
  SREG = s;
}

void cs_resumeADC() {
  uint8_t s = SREG; cli();
  if (cs_pause_depth > 0) {
    cs_pause_depth--;
    if (cs_pause_depth == 0) {
      TIMSK1 |= (1 << OCIE1A);
    }
  }
  SREG = s;
}

// 6.2. sampleCurrentSensors  (ana döngü => 10 ms'de bir çağrılmalı)
void sampleCurrentSensors()
{
  noInterrupts();                 // Tutarlı okuma için ISR’i kısa süre durdur
  for (uint8_t ch = 0; ch < NUM_Y_CHANNELS; ch++)
  {
    uint16_t n = sampleCnt[ch];
    if (n >= SAMPLES_PER_PERIOD)  // 80 örnek → ~1 şebeke periyodu
    {
      float meanSq   = (float)accSq[ch] / (float)n;     // Σ(i²)/N
      float IrmsAdc  = sqrtf(meanSq);                   // ADC LSB cinsinden
      float IrmsAmp  = IrmsAdc * ADC_TO_AMP;            // Amper’e dönüştür
      /* <<< ÖLÜ BÖLGE TAM BURAYA >>> */
      if (IrmsAmp < 0.50f)              // 350 mA’nin altını “gürültü” say
        IrmsAmp = 0.0f;

      Y_current[ch]  = IrmsAmp;                         // Dışarıya sun

      accSq[ch]      = 0;                               // Yeni periyot için sıfırla
      sampleCnt[ch]  = 0;
    }
  }
  interrupts();
}

// 6.3. getIrms  🪄  (kolay erişim yardımcı fonksiyon)
float getIrms(uint8_t yIndex)
{
  if (yIndex >= NUM_Y_CHANNELS) return 0.0f;
  return Y_current[yIndex];
}


/* -------------------------------------------------
 * energyTick(dtMs)
 *   Her saniye (dtMs ≈ 1000) ana döngüden çağrılacak.
 *   – dtMs → saat cinsine dönüştürüp Wh entegrasyonu yapar.
 * -------------------------------------------------*/
void energyTick(uint32_t dtMs)
{
    float dtH = (float)dtMs / 3600000.0f;   // ms → saat
    for (uint8_t ch = 0; ch < NUM_Y_CHANNELS; ch++) {
        float P = getIrms(ch) * 230.0f;     // 230 V varsay
        Y_powerW [ch] = P;
        Y_energyWh[ch] += P * dtH;
    }
}
