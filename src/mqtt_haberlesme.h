#pragma once
#include <Arduino.h>
void mqttInit();
void mqttLoop();
bool mqttConnected();
void mqttPublishDiscovery();
void mqttProcessStateQueue();   // dirty[] dizisini publish eder
