#define MQTT_MAX_PACKET_SIZE 512
#include <UIPEthernet.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "mqtt_haberlesme.h"
#include "pinmap.h"
#include "tanimlamalar.h"




/* ---------- Donanım pin eşleme tabloları ---------- */
static const uint8_t yMap[16] = {
  Y0_PIN,  Y1_PIN,  Y2_PIN,  Y3_PIN,
  Y4_PIN,  Y5_PIN,  Y6_PIN,  Y7_PIN,
  Y8_PIN,  Y9_PIN,  Y10_PIN, Y11_PIN,
  Y12_PIN, Y13_PIN, Y14_PIN, Y15_PIN
};

static const uint8_t xMap[16] = {
  X0_PIN,  X1_PIN,  X2_PIN,  X3_PIN,
  X4_PIN,  X5_PIN,  X6_PIN,  X7_PIN,
  X8_PIN,  X9_PIN,  X10_PIN, X11_PIN,
  X12_PIN, X13_PIN, X14_PIN, X15_PIN
};


static const byte MAC[6] = { 0xDE,0xAD,0xBE,0xEF,0xFE,FLOOR_ID[0] };
static IPAddress broker(192,168,1,113);   // Mosquitto’nun IP’sini buraya yaz
EthernetClient ethClient;
PubSubClient   mqttClient(ethClient);


static void callback(char* topic, byte* payload, unsigned int len)
{
    // up32/X10/set  ➜ alias = "X10"
    char* p = strchr(topic,'/'); if (!p) return;
    char* alias = ++p;             // "X10/set"
    char* slash2 = strchr(alias,'/'); if (!slash2) return;
    *slash2 = '\0';                // alias = "X10"

    char type = alias[0];          // 'X' ya da 'Y'
    uint8_t num = atoi(alias+1);   // 0-15
    uint8_t idx = (type=='Y') ? num : 16+num;

    bool on = (len>1 && payload[1]=='N');
    uint8_t hwPin = (type=='Y') ? yMap[num] : xMap[num];
    digitalWrite(hwPin, on);

    pinState[idx] = on;            // ① yeni durum
    dirty[idx]    = true;          // ② kuyruk işaretle
}

void mqttInit()
{
  Ethernet.begin(MAC);                 // DHCP
  mqttClient.setServer(broker,1883);
  mqttClient.setCallback(callback);
}

void reconnect()
{
    // Availability konusunu tek satırda oluşturuyoruz
    String lwtTopic = String(FLOOR_ID) + "/status";

    while (!mqttClient.connected()) {
        if (mqttClient.connect(
                FLOOR_ID,                  // Client ID
                MQTT_USER, MQTT_PASS,      // Kullanıcı / Parola
                lwtTopic.c_str(), 0, true, // LWT topic
                "offline"))                // LWT mesajı
        {
            mqttClient.publish(lwtTopic.c_str(), "online", true);
            mqttPublishDiscovery();
            mqttClient.subscribe( (String(FLOOR_ID) + "/+/set").c_str() );
        } else {
            delay(500);
        }
    }
}

void mqttLoop()
{
  if (!mqttClient.connected()) reconnect();
  mqttClient.loop();
}

void mqttProcessStateQueue()
{
    for (uint8_t i = 0; i < 32; i++) {
        if (dirty[i] && mqttClient.connected()) {
            dirty[i] = false;

            /* --- DÜZELTİLEN KISIM --- */
            char alias[4];
            if (i < 16)  sprintf(alias, "Y%u", i);      // Y0 … Y15
            else         sprintf(alias, "X%u", i - 16); // X0 … X15
            /* -------------------------------- */

            char topic[32];
            sprintf(topic, "%s/%s/state", FLOOR_ID, alias);
            mqttClient.publish(topic, pinState[i] ? "ON" : "OFF", true);
            break;                     // Döngü başına 1 paket
        }
    }
}

void mqttPublishDiscovery()
{
  StaticJsonDocument<256> doc;
  JsonObject dev = doc.createNestedObject("device");
  dev["identifiers"][0] = FLOOR_ID;
  dev["name"]        = "Merisoft Kontrol Kartı";
  dev["model"]       = "Mega+ENC28J60";
  dev["manufacturer"]= "Merisoft";
  dev["sw_version"]  = "0.1";

  char topic[64];
  char payload[400];

  for (uint8_t i = 0; i < 32; i++) {
      doc["name"]          = (i < 16) ? String("Y") + i   : String("X") + (i-16);
      doc["command_topic"] = String(FLOOR_ID) + "/" + ((i<16)?"Y":"X") + String(i%16, DEC) + "/set";
      doc["state_topic"]   = String(FLOOR_ID) + "/" + ((i<16)?"Y":"X") + String(i%16, DEC) + "/state";
      doc["unique_id"]     = String(FLOOR_ID) + "_" + ((i<16)?"Y":"X") + String(i%16, DEC);
      doc["payload_on"]    = "ON";
      doc["payload_off"]   = "OFF";

      snprintf(topic, sizeof(topic),
               "homeassistant/switch/%s_%c%02u/config",
               FLOOR_ID, (i<16?'Y':'X'), i%16);

      size_t len = serializeJson(doc, payload, sizeof(payload));
      mqttClient.publish(topic, (uint8_t*)payload, len, true);

      doc.clear();                       // sıradaki döngü için temizle
      dev = doc.createNestedObject("device");   // “device” bloğunu tekrar ekle
      dev["identifiers"][0] = FLOOR_ID;
      dev["name"]        = "Merisoft Kontrol Kartı";
      dev["model"]       = "Mega+ENC28J60";
      dev["manufacturer"]= "Merisoft";
      dev["sw_version"]  = "0.1";
  }
}
