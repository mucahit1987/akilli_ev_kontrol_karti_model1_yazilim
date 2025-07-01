/**
 *  temperature_control.h
 *  ---------------------
 *  – Sıcaklık sensörlerini oku,
 *  – 3 kademeli fan kontrol FSM’ini çalıştır,
 *  – Aşırı ısınan modülü kapat / geri aç,
 *  – MQTT’ye uyarı gönder.
 */
#pragma once
#include <Arduino.h>

void initTemperatureControl();   // setup()’tan çağır
void updateTemperatureControl(); // döngüde ~2 sn’de bir çağır
void serviceTriac();
