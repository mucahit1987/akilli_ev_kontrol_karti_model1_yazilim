#ifndef CONFIG_H
#define CONFIG_H

#define FLOOR_ID  "up32"     // alt kat kartına yüklerken "down32" yap

#define MQTT_USER  "mqttuser"
#define MQTT_PASS  "123"

// ------------------------------
// 🟧 YÜKSEK GÜÇLÜ TRIYAK ÇIKIŞLARI
// ------------------------------
#define Y0_PIN 3      
#define Y1_PIN 5    
#define Y2_PIN 7   
#define Y3_PIN 9  
#define Y4_PIN 11  
#define Y5_PIN 12  
#define Y6_PIN 15  
#define Y7_PIN 17  
#define Y8_PIN 19  
#define Y9_PIN 10  
#define Y10_PIN 23 
#define Y11_PIN 25
#define Y12_PIN 27 
#define Y13_PIN 29 
#define Y14_PIN 31 
#define Y15_PIN 30 

// ------------------------------
// 🟦 DÜŞÜK GÜÇLÜ TRIYAK ÇIKIŞLARI
// ------------------------------
#define X0_PIN 28  
#define X1_PIN 26  
#define X2_PIN 24  
#define X3_PIN 22  
#define X4_PIN 8  
#define X5_PIN 18  
#define X6_PIN 16  
#define X7_PIN 14  
#define X8_PIN 46  
#define X9_PIN 44  
#define X10_PIN 42 
#define X11_PIN 40 
#define X12_PIN 38 
#define X13_PIN 36 
#define X14_PIN 34 
#define X15_PIN 32 

// akım sensör pinleri

#define Y0_CURRENT_SENSOR_PIN A1   
#define Y1_CURRENT_SENSOR_PIN A3  
#define Y2_CURRENT_SENSOR_PIN A5  
#define Y4_CURRENT_SENSOR_PIN A9  
#define Y5_CURRENT_SENSOR_PIN A11  
#define Y6_CURRENT_SENSOR_PIN A13  
#define Y8_CURRENT_SENSOR_PIN A8  
#define Y9_CURRENT_SENSOR_PIN A10  
#define Y10_CURRENT_SENSOR_PIN A12 
#define Y12_CURRENT_SENSOR_PIN A0 
#define Y13_CURRENT_SENSOR_PIN A2 
#define Y14_CURRENT_SENSOR_PIN A4 

// TERMİSTÖR PİNLERİ

#define MODULE1_THERMISTOR_PIN A7 
#define MODULE2_THERMISTOR_PIN A15 
#define MODULE3_THERMISTOR_PIN A14 
#define MODULE4_THERMISTOR_PIN A6 

// -------------------------------------
// 🌬️ FAN KONTROL & ZERO CROSS TANIMLARI
// -------------------------------------
#define ZERO_CROSS_PIN 21 // scl
#define FAN_TRIAC_PIN 20 // sda

#endif // CONFIG_H
