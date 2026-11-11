#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

const char* WIFI_SSID = "VOTRE_WIFI";
const char* WIFI_MDP  = "VOTRE_MOT_DE_PASSE";
const char* BROKER    = "broker.hivemq.com";
const int   PORT      = 1883;

// Préfixe unique : le broker public est partagé
const char* T_TEMP   = "monprojet123/serre/temperature";
const char* T_HUM    = "monprojet123/serre/humidite";
const char* T_VENTIL = "monprojet123/serre/ventilateur";
const char* T_MODE   = "monprojet123/serre/mode";
const char* T_STATUT = "monprojet123/serre/statut";
const char* T_CMD    = "monprojet123/serre/commande";

const int PIN_DHT    = 4;
const int PIN_RELAIS = 26;
const int PIN_BOUTON = 15;

const float SEUIL_HAUT = 30.0;  // allume au-dessus
const float SEUIL_BAS  = 28.0;  // éteint en dessous (hystérésis)

DHT dht(PIN_DHT, DHT22);
WiFiClient wifi;
PubSubClient mqtt(wifi);

bool modeAuto = true;
bool ventilateur = false;
unsigned long tMesure = 0, tReseau = 0, tBouton = 0;

void publierMode() {
  if (mqtt.connected()) mqtt.publish(T_MODE, modeAuto ? "AUTO" : "MANUEL", true);
}

void appliquer(bool etat) {
  ventilateur = etat;
  digitalWrite(PIN_RELAIS, etat ? HIGH : LOW);
  if (mqtt.connected()) mqtt.publish(T_VENTIL, etat ? "ON" : "OFF", true);
}

void recevoir(char* topic, byte* payload, unsigned int n) {
  String msg;
  for (unsigned int i = 0; i < n; i++) msg += (char)payload[i];
  if (msg == "AUTO") {
    modeAuto = true;
  } else if (msg == "ON") {
    modeAuto = false; appliquer(true);
  } else if (msg == "OFF") {
    modeAuto = false; appliquer(false);
  }
  publierMode();
}

void gererReseau() {
  if (millis() - tReseau < 5000) return;
  tReseau = millis();
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_MDP);
    return;
  }
  if (!mqtt.connected()) {
    String id = "serre-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    // last will : "hors-ligne" publié par le broker si la carte disparaît
    if (mqtt.connect(id.c_str(), NULL, NULL, T_STATUT, 0, true, "hors-ligne")) {
      mqtt.publish(T_STATUT, "en-ligne", true);
      mqtt.subscribe(T_CMD);
      publierMode();
      mqtt.publish(T_VENTIL, ventilateur ? "ON" : "OFF", true);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_RELAIS, OUTPUT);
  pinMode(PIN_BOUTON, INPUT_PULLUP);
  dht.begin();
  WiFi.mode(WIFI_STA);
  mqtt.setServer(BROKER, PORT);
  mqtt.setCallback(recevoir);
}

void loop() {
  gererReseau();
  if (mqtt.connected()) mqtt.loop();

  // Bouton : passe en manuel et bascule le ventilateur
  if (digitalRead(PIN_BOUTON) == LOW && millis() - tBouton > 300) {
    tBouton = millis();
    modeAuto = false;
    appliquer(!ventilateur);
    publierMode();
  }

  // Mesure et régulation toutes les 2 s (le DHT22 ne va pas plus vite)
  if (millis() - tMesure > 2000) {
    tMesure = millis();
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (isnan(t) || isnan(h)) {
      Serial.println("Erreur de lecture DHT");
      return;
    }
    if (modeAuto) {
      if (t > SEUIL_HAUT && !ventilateur) appliquer(true);
      if (t < SEUIL_BAS && ventilateur)  appliquer(false);
    }
    if (mqtt.connected()) {
      mqtt.publish(T_TEMP, String(t, 1).c_str());
      mqtt.publish(T_HUM,  String(h, 1).c_str());
    }
  }
}