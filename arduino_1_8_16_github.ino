#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <ArduinoMqttClient.h>


#include <ESP8266WiFi.h>


#define ESP8266_LED (5)
#define RELAY_SIGNAL_PIN (16)
#define SERIAL_DEBUG
#define TLS_DEBUG
#define MQTT_HOST  "Adresse_IP_Broker_Mosquitto"


/* Certificate Authority info */
/* CA Cert in PEM format */
const char caCert[] PROGMEM = R"EOF(
CERTIFICAT
)EOF";


/* MQTT broker cert SHA1 fingerprint, used to validate connection to right server D9:58:E2:20:BC:12:C4:4D:ED:0F:9E:CC:73:83:8B:31:1B:0B:47:9A*/
// const uint8_t mqttCertFingerprint[] = {0xC6, 0xBA, 0xDE, 0x96, 0x7B, 0x94, 0x29, 0x86, 0xAD, 0xC7, 0xC6, 0xFA, 0x8D, 0xE8, 0xDB, 0x66, 0xFF, 0x61, 0x02, 0x86};


X509List caCertX509(caCert);
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
Adafruit_MQTT_Client mqtt(&espClient, MQTT_HOST, ID_Port, "IDENTIFIANT", "MOTDEPASSE");
char clientId[32] = "ESP8266Client-";


int compteur = 0;


#ifdef TLS_DEBUG
bool verifytls() {
    bool success = false;


#ifdef SERIAL_DEBUG
    // WiFi.ping();
    Serial.print("Verifying TLS connection to ");
    Serial.println(MQTT_HOST);
#endif


    success = espClient.connect(MQTT_HOST, ID_Port);


#ifdef SERIAL_DEBUG
    if (success) {
        Serial.println("Connection complete, valid cert, valid fingerprint.");
    } else {
        Serial.println("Connection failed!");
    }
#endif


    return success;
}
#endif


// Fonction permettant de récupérer la bonne version de TLS 1.3
bool verifyTLSVersion(WiFiClientSecure &client) {
  // Unfortunately, BearSSL doesn't currently expose a function to retrieve
  // the negotiated TLS version directly.


  // Alternative approach: Check if the connection was successful
  // (indirectly implies TLS handshake completed).
  if (client.connected()) {
    Serial.println("** TLS handshake completed (TLS version unknown). **");
    Serial.println("** Warning: Unable to verify specific TLS version (BearSSL limitation). **");
    return true;  // Continue assuming TLS is established (be cautious)
  } else {
    Serial.println("** Erreur : Échec de la connexion au serveur MQTT (TLS handshake might have failed). **");
    return false;
  }
}

int reconnectAttempts = 0; // Counter for reconnect attempts


void reconnect() {
  while (!mqttClient.connected()) {
    #ifdef SERIAL_DEBUG
      Serial.print("Attempting MQTT broker connection...");
    #endif


    if (mqttClient.connect(clientId)) {
      #ifdef SERIAL_DEBUG
        Serial.println("connected");
      #endif
      reconnectAttempts = 0; // Reset counter on successful connection
      mqttClient.subscribe("Commande");
    } else {
      #ifdef SERIAL_DEBUG
        Serial.print("Failed, rc=");
        Serial.print(mqttClient.state());
        Serial.println(". Trying again in 5 seconds...");
      #endif
      delay(500);
      reconnectAttempts++;


      // Add restart logic here if needed
      if (reconnectAttempts >= 2) {
        // ESP.restart(); // Restart ESP8266 after 5 failed attempts
      }
    }
  }
}


void setup() {
    pinMode(RELAY_SIGNAL_PIN, OUTPUT);
    pinMode(ESP8266_LED, OUTPUT);


#ifdef SERIAL_DEBUG
    Serial.setDebugOutput(true);
    Serial.begin(115200);
    Serial.println();
#endif


    WiFi.mode(WIFI_STA);
    WiFi.begin("Wifi_SSID", "Wifi_Password");


    while (WiFi.status() != WL_CONNECTED) {
        delay(10);
    }


#ifdef SERIAL_DEBUG
    Serial.println();
    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());
    espClient.setTrustAnchors(&caCertX509);
    espClient.allowSelfSignedCerts();
    // espClient.setFingerprint(mqttCertFingerprint);


#endif


   
#ifdef TLS_DEBUG
    // Spécifier la version TLS 1.2
    verifytls();
#endif


    mqttClient.setServer(MQTT_HOST, ID_Port);
    mqttClient.setCallback(subCallback);


    sprintf(clientId + strlen(clientId), "%X", random(0xffff));
}


void loop() {
    if (!mqttClient.connected()) {
        reconnect();
    }


    if (compteur > 10) {
        mqttClient.publish("Test", "OK");
        compteur = 0;
    }
    compteur++;
    mqttClient.loop();
}


void subCallback(char *topic, byte *payload, unsigned int length) {
    static int pinStatus = LOW;
    StaticJsonDocument<256> doc;
    deserializeJson(doc, (char *)payload);
    JsonObject root = doc.as<JsonObject>();


#ifdef SERIAL_DEBUG
    serializeJson(root, Serial);
    Serial.println();
#endif
    Serial.printf("Topic:%s,%s\n", topic, (char *)payload);


    if (!root["set"].isNull()) {
        if (root["set"] == "toggle") {
            pinStatus = !pinStatus;
        } else if (root["set"] == "on") {
            pinStatus = HIGH;
        } else if (root["set"] == "off") {
            pinStatus = LOW;
        } else {
            return;
        }


        digitalWrite(RELAY_SIGNAL_PIN, pinStatus);
        mqttClient.publish("Test", pinStatus == LOW ? "off" : "on");
    } else if (!root["get"].isNull()) {
        if (root["get"] == "status") {
            mqttClient.publish("Test", pinStatus == LOW ? "off" : "on");
        }
    }
}
