#pragma once
#include <Arduino.h>
void mqttInit();
void mqttLoop();
bool mqttConnected();
void mqttPublishDiscovery();
void mqttProcessStateQueue();   // dirty[] dizisini publish eder
void mqttPublishAlert(uint8_t mod, float deg, bool closed);
void haNotify(const char* title, const char* message);