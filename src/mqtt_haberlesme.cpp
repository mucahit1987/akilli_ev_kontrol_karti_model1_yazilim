#define MQTT_MAX_PACKET_SIZE 512
#include <UIPEthernet.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "mqtt_haberlesme.h"
#include "pinmap.h"
#include "tanimlamalar.h"
#include "current_sense.h"


static const byte MAC[6] = { 0xDE,0xAD,0xBE,0xEF,0xFE,FLOOR_ID[0] };
static IPAddress broker(192,168,1,113);   // Mosquitto’nun IP’sini buraya yaz
EthernetClient ethClient;
PubSubClient   mqttClient(ethClient);


void publishPowerEnergyDiscovery()
{
    char topic[64];
    char payload[512];

    for (uint8_t ch = 0; ch < NUM_Y_CHANNELS; ch++) {

        /* ------------- POWER ------------- */
        StaticJsonDocument<512> doc;                 // ← Yerel doc → her tur temiz
        JsonObject dev = doc.createNestedObject("device");
        dev["identifiers"][0] = FLOOR_ID;
        dev["name"]           = "Merisoft Kart";
        dev["model"]          = "Mega+ENC28J60";

        doc["device_class"]        = "power";
        doc["state_class"]         = "measurement";
        doc["unit_of_measurement"] = "W";
        doc["name"]                = String("Y") + ch + " Power";
        doc["state_topic"]         = String(FLOOR_ID) + "/Y" + ch + "/power";
        doc["unique_id"]           = String(FLOOR_ID) + "_pow_Y" + ch;

        snprintf(topic,sizeof(topic),
                 "homeassistant/sensor/%s/Y%u_power/config", FLOOR_ID, ch);
        serializeJson(doc, payload, sizeof(payload));
        mqttClient.publish(topic, payload, true);

        /* ------------- ENERGY ------------ */
        doc.clear();                                   // Tertemiz
        dev = doc.createNestedObject("device");
        dev["identifiers"][0] = FLOOR_ID;

        doc["device_class"]        = "energy";
        doc["state_class"]         = "total_increasing";
        doc["unit_of_measurement"] = "Wh";
        doc["name"]                = String("Y") + ch + " Energy";
        doc["state_topic"]         = String(FLOOR_ID) + "/Y" + ch + "/energy";
        doc["unique_id"]           = String(FLOOR_ID) + "_ener_Y" + ch;

        snprintf(topic,sizeof(topic),
                 "homeassistant/sensor/%s/Y%u_energy/config", FLOOR_ID, ch);
        serializeJson(doc, payload, sizeof(payload));
        mqttClient.publish(topic, payload, true);
    }
}



static void callback(char* topic, byte* payload, unsigned int len)
{
    /* -------- 1) alias'ı ayıkla (X10/Y7) -------- */
    char* p = strchr(topic,'/');      if (!p) return;
    char* alias  = ++p;               // "X10/set"
    char* slash2 = strchr(alias,'/'); if (!slash2) return;
    *slash2 = '\0';                   // alias = "X10"

    char type = alias[0];             // 'X' ya da 'Y'
    uint8_t num = atoi(alias+1);      // 0-15
    uint8_t idx = (type=='Y') ? num : 16+num;   // pinState dizin

    /* -------- 2) ON / OFF belirle -------- */
    bool on = (len>1 && payload[1]=='N');       // "ON" → true, "OFF" → false

    /* -------- 3) Aşırı ısınma kilidi -------- */
    if (type=='Y') {                            // yalnız Y-grubu modüller
        uint8_t modNo = num / 4;                // 4 çıkış = 1 modül
        if (moduleLocked[modNo] && on) {        // kilitliyken ON isteği
            char msg[48]; snprintf(msg, 48, "%s: kilit devam ediyor", alias);
            haNotify("Komut Reddedildi", msg);
            return;                             // komutu YOK SAY
        }
    }

    /* -------- 4) Donanım pinini sür -------- */
    uint8_t hwPin = (type=='Y') ? yMap[num] : xMap[num];
    digitalWrite(hwPin, on);

    /* -------- 5) Durum dizilerini güncelle -------- */
    pinState[idx] = on;
    dirty[idx]    = true;                       // MQTT publish kuyruğu
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
            //delay(500);
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
  StaticJsonDocument<768> doc;
  JsonObject dev = doc.createNestedObject("device");
  dev["identifiers"][0] = FLOOR_ID;
  dev["name"]        = "Merisoft Kontrol Kartı";
  dev["model"]       = "Mega+ENC28J60";
  dev["manufacturer"]= "Merisoft";
  dev["sw_version"]  = "0.1";

  char topic[64];
  char payload[768];

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

  publishPowerEnergyDiscovery();
}

/*********************************************************************
 * 🔔 Aşırı Isınma Uyarısı – MQTT
 *  topic: <FLOOR_ID>/alert   (ör. “up32/alert”)
 *  payload:
 *    {
 *      "module": 2,          // 0–7 arası modül no
 *      "temp":   67.3,       // °C
 *      "state":  "off"       // "off"  → çıkış kapandı
 *                           // "restored" → tekrar açıldı
 *    }
 *********************************************************************/
void mqttPublishAlert(uint8_t mod, float deg, bool closed)
{
    StaticJsonDocument<128> doc;
    doc["module"] = mod;
    doc["temp"]   = deg;
    doc["state"]  = closed ? "off" : "restored";

    char topic[48];
    sprintf(topic, "%s/alert", FLOOR_ID);   // ör. up32/alert

    char payload[128];
    serializeJson(doc, payload);

    mqttClient.publish(topic, payload, true); // retained mesaj
}

void haNotify(const char* title, const char* message)
{
    StaticJsonDocument<128> doc;
    doc["title"]   = title;
    doc["message"] = message;
    char payload[128]; serializeJson(doc, payload);

    mqttClient.publish(
      "homeassistant/service/persistent_notification/create",
      payload,         // non-retained
      false
    );
}

void mqttPublishPowerEnergy()
{
    char topic[40];
    char buf[16];

    for (uint8_t ch = 0; ch < 16; ch++) {
        /* --- Power (W) --- */
        dtostrf(Y_powerW[ch] , 0, 1, buf);
        sprintf(topic, "%s/Y%u/power",  FLOOR_ID, ch);
        mqttClient.publish(topic, buf);

        /* --- Energy (Wh) --- */
        dtostrf(Y_energyWh[ch], 0, 2, buf);
        sprintf(topic, "%s/Y%u/energy", FLOOR_ID, ch);
        mqttClient.publish(topic, buf);
    }
}