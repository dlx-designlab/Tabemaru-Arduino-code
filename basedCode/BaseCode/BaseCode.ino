#include <Wire.h>
#include <DHT20.h>
#include <WiFiS3.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <RTC.h>

// Définition de la broche analogique
#define MOISTURE_SENSOR_PIN A0

const int ledPin = A3; // Broche A3 connectée à la base du transistor

// Remplace par tes infos WiFi
const char* ssid = "dlx_iot";
const char* password = "komabawho?36";

// Webhook Google Apps Script
const char* host = "script.google.com";
const char* scriptId = "AKfycbxJMwXlo3Cdo1LPIz02y5S6NJR44tTNEP1SFbKD2sAO40PCm7e3nfDZIJODEBtenqjhsg";
const int port = 443;

// Définir le serveur NTP et le décalage horaire (en secondes)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 32400, 60000); // 32400 = UTC+9 (Japon, JST)

DHT20 DHT1;
DHT20 DHT2;
WiFiSSLClient client;

// Multiplexer function
void TCA9548A(uint8_t bus) {
  Wire.beginTransmission(0x70); // TCA9548A address
  Wire.write(1 << bus);       // Send byte to select bus
  Wire.endTransmission();
  delay(10); // Petit délai pour stabiliser le bus
}

// Read temp and humidity sensor values
bool readValues(DHT20& DHT, int bus, float& temperature, float& humidity) {
  TCA9548A(bus);
  int status = DHT.read();
  if (status == DHT20_OK) {
    temperature = DHT.getTemperature();
    humidity = DHT.getHumidity();
    Serial.print("Sensor ");
    Serial.print(bus);
    Serial.print(": Temp = ");
    Serial.print(temperature, 1);
    Serial.print(" °C, Hum = ");
    Serial.print(humidity, 1);
    Serial.println(" %");
    return true;
  } else {
    Serial.print("Sensor ");
    Serial.print(bus);
    Serial.print(": Error reading data: ");
    switch (status) {
      case DHT20_ERROR_CHECKSUM:
        Serial.println("Checksum error");
        break;
      case DHT20_ERROR_CONNECT:
        Serial.println("Connect error");
        break;
      case DHT20_MISSING_BYTES:
        Serial.println("Missing bytes");
        break;
      case DHT20_ERROR_BYTES_ALL_ZERO:
        Serial.println("All bytes read zero");
        break;
      case DHT20_ERROR_READ_TIMEOUT:
        Serial.println("Read timeout");
        break;
      case DHT20_ERROR_LASTREAD:
        Serial.println("Error read too fast");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
    return false;
  }
}

// Fonction pour établir une connexion WiFi stable
bool connectWiFi() {
  Serial.print("Connexion au WiFi...");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { // Max 20 tentatives (10s)
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
  Serial.begin(9600);
  while (!Serial); // Attendre que le port série soit prêt
  Wire.begin();
  delay(1000); // Attendre que l'I2C soit prêt

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

  // Configurer la broche A3 comme sortie
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); // Éteindre la LED au démarrage

  // Connexion WiFi initiale
  if (connectWiFi()) {
    // Initialiser le client NTP
    timeClient.begin();

    // Synchroniser l'RTC avec NTP
    int ntpAttempts = 0;
    bool ntpSuccess = false;
    while (ntpAttempts < 3 && !ntpSuccess) { // Essayer 3 fois
      if (timeClient.update()) {
        unsigned long epochTime = timeClient.getEpochTime();
        Serial.print("Heure NTP (epoch) : ");
        Serial.println(epochTime);
        RTCTime currentTime(epochTime);
        if (RTC.setTime(currentTime)) {
          Serial.println("RTC synchronisé avec succès !");
          RTCTime verifyTime;
          RTC.getTime(verifyTime);
          Serial.print("Heure RTC après synchro : ");
          Serial.print(verifyTime.getHour());
          Serial.print(":");
          if (verifyTime.getMinutes() < 10) Serial.print("0");
          Serial.println(verifyTime.getMinutes());
          ntpSuccess = true;
        } else {
          Serial.println("Échec de la synchronisation RTC !");
        }
      } else {
        Serial.println("Échec de la mise à jour NTP !");
      }
      ntpAttempts++;
      delay(1000);
    }

    // Solution de secours si NTP échoue
    if (!ntpSuccess) {
      Serial.println("Synchronisation NTP échouée, réglage manuel de l'RTC à 12:11 JST, 5 juin 2025");
      RTCTime manualTime(2025, Month::JUNE, 5, 12, 11, 0, DayOfWeek::THURSDAY, SaveLight::SAVING_TIME_ACTIVE);
      RTC.setTime(manualTime);
    }
  } else {
    Serial.println("WiFi non connecté, réglage manuel de l'RTC à 12:11 JST, 5 juin 2025");
    RTCTime manualTime(2025, Month::JUNE, 5, 12, 11, 0, DayOfWeek::THURSDAY, SaveLight::SAVING_TIME_ACTIVE);
    RTC.setTime(manualTime);
  }

  // Initialiser l'RTC
  RTC.begin();
}

void loop() {
  // Vérifier ou reconnecter WiFi
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Mettre à jour l'heure NTP toutes les 10 minutes
  static unsigned long lastNTPUpdate = 0;
  if (millis() - lastNTPUpdate >= 600000 && WiFi.status() == WL_CONNECTED) { // 10 minutes
    if (timeClient.update()) {
      unsigned long epochTime = timeClient.getEpochTime();
      Serial.print("Heure NTP (epoch) : ");
      Serial.println(epochTime);
      RTCTime currentTime(epochTime);
      if (RTC.setTime(currentTime)) {
        Serial.println("RTC mis à jour avec succès !");
        RTCTime verifyTime;
        RTC.getTime(verifyTime);
        Serial.print("Heure RTC après mise à jour : ");
        Serial.print(verifyTime.getHour());
        Serial.print(":");
        if (verifyTime.getMinutes() < 10) Serial.print("0");
        Serial.println(verifyTime.getMinutes());
      } else {
        Serial.println("Échec de la mise à jour RTC !");
      }
      lastNTPUpdate = millis();
    } else {
      Serial.println("Échec de la mise à jour NTP !");
    }
  }

  // Récupérer l'heure actuelle depuis l'RTC
  RTCTime currentTime;
  RTC.getTime(currentTime);
  int currentHour = currentTime.getHour();
  int currentMinute = currentTime.getMinutes();

  // Contrôler la LED selon la plage horaire (8h00 à 20h00 JST)
  if (currentHour >= 8 && currentHour < 13) {
    digitalWrite(ledPin, HIGH); // Allumer la LED
    Serial.println("LED allumée (entre 8h00 et 20h00)");
  } else {
    digitalWrite(ledPin, LOW); // Éteindre la LED
    Serial.println("LED éteinte (hors 8h00-20h00)");
  }

  // Afficher l'heure actuelle
  Serial.print("Heure actuelle (RTC) : ");
  Serial.print(currentHour);
  Serial.print(":");
  if (currentMinute < 10) Serial.print("0");
  Serial.println(currentMinute);

  // Lecture des capteurs
  float temp1, hum1, temp2, hum2;
  bool sensor1_ok = readValues(DHT1, 0, temp1, hum1);
  bool sensor2_ok = readValues(DHT2, 1, temp2, hum2);
  int moistureValue = analogRead(MOISTURE_SENSOR_PIN);
  int moisturePercentage = map(moistureValue, 1023, 0, 100, 0);
  Serial.println("moistureValue = " + String(moistureValue));
  Serial.println("moisture = " + String(moisturePercentage));

  // Envoi des données si au moins un capteur est OK
  if (sensor1_ok || sensor2_ok) {
    int works = 0;
    int maxAttempts = 3; // Limiter à 3 tentatives
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
        works = 1; // Marquer comme réussi
        client.stop(); // Fermer la connexion
      } else {
        Serial.println("Échec de la connexion au serveur.");
        maxAttempts--;
        delay(1000); // Attendre avant de réessayer
      }
    }
    if (works == 0) {
      Serial.println("Échec de l'envoi des données après plusieurs tentatives.");
    }
  } else {
    Serial.println("Aucun capteur n'a pu être lu.");
  }

  // Attendre 5 minutes avant la prochaine itération
  Serial.println("Attente de 5 minutes...");
  delay(300000); // 5 minutes
}