#include <Wire.h>
#include <DHT20.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>

// Définition des broches
#define MOISTURE_SENSOR_PIN 2 // GPIO36 (ADC1_CH0) pour le capteur d'humidité
#define LED_PIN 15             // GPIO15 pour la LED

// Informations WiFi
const char* ssid = "dlx_iot";
const char* password = "komabawho?36";

// Webhook Google Apps Script
const char* host = "script.google.com";
const char* scriptId = "AKfycbxJMwXlo3Cdo1LPIz02y5S6NJR44tTNEP1SFbKD2sAO40PCm7e3nfDZIJODEBtenqjhsg";
const int port = 443;

// Configuration NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 32400, 60000); // UTC+9 (Japon, JST)

// Définir les capteurs DHT20
DHT20 DHT1;
DHT20 DHT2;
WiFiClientSecure client;

// Multiplexer function
void TCA9548A(uint8_t bus) {
  Wire.beginTransmission(0x70); // Adresse TCA9548A
  Wire.write(1 << bus);       // Sélectionner le bus
  Wire.endTransmission();
  delay(10); // Délai pour stabiliser
}

// Lire les valeurs des capteurs
bool readValues(DHT20& DHT, int bus, float& temperature, float& humidity) {
  TCA9548A(bus);
  int status = DHT.read();
  if (status == DHT20_OK) {
    temperature = DHT.getTemperature();
    humidity = DHT.getHumidity();
    Serial.print("Capteur ");
    Serial.print(bus);
    Serial.print(": Temp = ");
    Serial.print(temperature, 1);
    Serial.print(" °C, Hum = ");
    Serial.print(humidity, 1);
    Serial.println(" %");
    return true;
  } else {
    Serial.print("Capteur ");
    Serial.print(bus);
    Serial.print(": Erreur de lecture : ");
    switch (status) {
      case DHT20_ERROR_CHECKSUM: Serial.println("Erreur de checksum"); break;
      case DHT20_ERROR_CONNECT: Serial.println("Erreur de connexion"); break;
      case DHT20_MISSING_BYTES: Serial.println("Octets manquants"); break;
      case DHT20_ERROR_BYTES_ALL_ZERO: Serial.println("Tous les octets à zéro"); break;
      case DHT20_ERROR_READ_TIMEOUT: Serial.println("Timeout de lecture"); break;
      case DHT20_ERROR_LASTREAD: Serial.println("Lecture trop rapide"); break;
      default: Serial.println("Erreur inconnue"); break;
    }
    return false;
  }
}

// Connexion WiFi
bool connectWiFi() {
  Serial.print("Connexion au WiFi...");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connecté !");
    return true;
  } else {
    Serial.println("Échec de la connexion WiFi !");
    return false;
  }
}

void setup() {
  Serial.begin(9600); // Baud rate plus élevé pour ESP32
  while (!Serial); // Attendre le port série
  Wire.begin(); // Initialiser I2C (SDA=21, SCL=22 par défaut sur ESP32)
  delay(1000);

  // Initialisation capteur 1
  TCA9548A(0);
  delay(10);
  if (!DHT1.begin()) {
    Serial.println("Erreur: capteur DHT20 non détecté sur bus 0 !");
    while (1);
  } else {
    Serial.println("Validation: capteur DHT20 détecté sur bus 0 !");
  }

  // Initialisation capteur 2
  TCA9548A(1);
  delay(10);
  if (!DHT2.begin()) {
    Serial.println("Erreur: capteur DHT20 non détecté sur bus 1 !");
    while (1);
  } else {
    Serial.println("Validation: capteur DHT20 détecté sur bus 1 !");
  }

  // Configurer les broches
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Éteindre la LED
  pinMode(MOISTURE_SENSOR_PIN, INPUT);

  // Connexion WiFi
  connectWiFi();
  delay(2000);
  if (connectWiFi()) {
    // Configurer l'heure via NTP
    configTime(32400, 0, "pool.ntp.org"); // UTC+9, pas de DST
    timeClient.begin();
    while (!connectWiFi()) {
      if (timeClient.update()) {
        Serial.println("Heure NTP synchronisée !");
        break;
      }
      Serial.println("Échec de la mise à jour NTP !");
      delay(1000);
    }
  } else {
    Serial.println("WiFi non connecté, heure non synchronisée.");
  }

  // Ignorer la vérification du certificat SSL (pour simplifier)
  client.setInsecure();
}

void loop() {
  // Vérifier/reconnecter WiFi
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Mettre à jour l'heure NTP toutes les 10 minutes
  static unsigned long lastNTPUpdate = 0;
  if (millis() - lastNTPUpdate >= 600000 && WiFi.status() == WL_CONNECTED) {
    if (timeClient.update()) {
      Serial.println("Heure NTP mise à jour !");
      lastNTPUpdate = millis();
    } else {
      Serial.println("Échec de la mise à jour NTP !");
    }
  }

  // Obtenir l'heure actuelle
  struct tm timeinfo;
  bool heure = getLocalTime(&timeinfo);
  while(!heure){
    Serial.println("heure non recuperer");
    heure = getLocalTime(&timeinfo);
  }
  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;

  // Contrôler la LED (8h00-13h00 JST)
  if (currentHour >= 18 && currentHour < 6) {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("LED allumée (8h00-14h00)");
  } else {
    digitalWrite(LED_PIN, LOW);
    Serial.println("LED éteinte (hors 8h00-14h00)");
  }

  // Afficher l'heure
  Serial.print("Heure actuelle : ");
  Serial.print(currentHour);
  Serial.print(":");
  if (currentMinute < 10) Serial.print("0");
  Serial.println(currentMinute);

  // Lire les capteurs
  float temp1, hum1, temp2, hum2;
  bool sensor1_ok = readValues(DHT1, 0, temp1, hum1);
  bool sensor2_ok = readValues(DHT2, 1, temp2, hum2);
  int moistureValue = analogRead(MOISTURE_SENSOR_PIN);
  int moisturePercentage = map(moistureValue, 4095, 0, 100, 0); // ESP32 ADC est 12 bits (0-4095)
  Serial.println("moistureValue = " + String(moistureValue));
  Serial.println("moisture = " + String(moisturePercentage));

  // Envoi des données
  if (sensor1_ok || sensor2_ok) {
    int works = 0;
    int maxAttempts = 3;
    while (works == 0 && maxAttempts > 0 && WiFi.status() == WL_CONNECTED) {
      Serial.println("Tentative d'envoi des données...");
      if (client.connect(host, port)) {
        String url = "/macros/s/" + String(scriptId) + "/exec?temp1=" + (sensor1_ok ? String(temp1, 1) : "N/A") +
                     "&hum1=" + (sensor1_ok ? String(hum1, 1) : "N/A") +
                     "&temp2=" + (sensor2_ok ? String(temp2, 1) : "N/A") +
                     "&hum2=" + (sensor2_ok ? String(hum2, 1) : "N/A") +
                     "&moist=" + String(moisturePercentage);

        client.println("GET " + url + " HTTP/1.1");
        client.println("Host: " + String(host));
        client.println("Connection: close");
        client.println();

        // Lire la réponse
        String response = "";
        while (client.connected() || client.available()) {
          if (client.available()) {
            response += client.readStringUntil('\n');
            response += "\n";
          }
        }
        Serial.println("Réponse du serveur : ");
        Serial.println(response);
        works = 1;
        client.stop();
      } else {
        Serial.println("Échec de la connexion au serveur.");
        maxAttempts--;
        delay(1000);
      }
    }
    if (works == 0) {
      Serial.println("Échec de l'envoi des données.");
    }
  } else {
    Serial.println("Aucun capteur n'a pu être lu.");
  }

  // Attendre 5 minutes (ou utiliser deep sleep)
  Serial.println("Attente de 5 minutes...");
  delay(300000); // 5 minutes
  // Option : Deep sleep pour économiser l'énergie
  // esp_sleep_enable_timer_wakeup(300000000); // 5 minutes en microsecondes
  // esp_deep_sleep_start();
}