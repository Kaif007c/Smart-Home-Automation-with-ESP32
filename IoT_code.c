#include <WiFi.h>
#include <Firebase_ESP_Client.h>

#include <SPI.h>
#include <MFRC522.h>

#include <DHT.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ================= WIFI =================

#define WIFI_SSID "LOQ"
#define WIFI_PASSWORD "*********"

// ================= FIREBASE =================

#define API_KEY "AIzaSyCDdclqzQIF3zRMATkugjwDuLGHG17r6xQ"

#define DATABASE_URL "https://smarthome-ffc1f-default-rtdb.firebaseio.com"

// ================= RFID =================

#define SS_PIN 5
#define RST_PIN 2

MFRC522 rfid(SS_PIN, RST_PIN);

// Replace with your UID
String authorizedUID = "93A4210C";

// ================= DHT =================

#define DHTPIN 4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// ================= PINS =================

#define LED_PIN     25
#define RELAY_PIN   26
#define IR_PIN      27

#define MQ2_PIN     34
#define LDR_PIN     35

// ================= LCD =================

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= FIREBASE OBJECTS =================

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool signupOK = false;

bool systemActive = false;

// ======================================

void setup()
{
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(IR_PIN, INPUT);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(RELAY_PIN, HIGH);

  dht.begin();

  SPI.begin();
  rfid.PCD_Init();

  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.print("Connecting...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", ""))
  {
    signupOK = true;
    Serial.println("Firebase Connected");
  }

  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  lcd.clear();
  lcd.print("Smart Home");
  delay(1500);
}

// ======================================

void loop()
{
  int irState = digitalRead(IR_PIN);

  // Person Detected
  if (!systemActive && irState == HIGH)
  {
    lcd.clear();
    lcd.print("Scan RFID");

    if (rfid.PICC_IsNewCardPresent() &&
        rfid.PICC_ReadCardSerial())
    {
      String scannedUID = "";

      for (byte i = 0; i < rfid.uid.size; i++)
      {
        if (rfid.uid.uidByte[i] < 0x10)
          scannedUID += "0";

        scannedUID += String(rfid.uid.uidByte[i], HEX);
      }

      scannedUID.toUpperCase();

      if (scannedUID == authorizedUID)
      {
        systemActive = true;

        lcd.clear();
        lcd.print("Access Granted");

        Firebase.RTDB.setString(
          &fbdo,
          "/SmartHome/lastRFID",
          scannedUID);

        Firebase.RTDB.setBool(
          &fbdo,
          "/SmartHome/occupancy",
          true);

        delay(1500);
      }

      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }
  }

  // Person Left
  if (systemActive && irState == LOW)
  {
    systemActive = false;

    digitalWrite(LED_PIN, LOW);
    digitalWrite(RELAY_PIN, HIGH);

    Firebase.RTDB.setBool(
      &fbdo,
      "/SmartHome/occupancy",
      false);

    lcd.clear();
    lcd.print("System OFF");
  }

  if (systemActive)
  {
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();

    int ldr = analogRead(LDR_PIN);
    int gas = analogRead(MQ2_PIN);

    bool fanOn = false;
    bool lightOn = false;

    // FAN LOGIC

    if (temp > 25)
    {
      digitalWrite(RELAY_PIN, LOW);
      fanOn = true;
    }
    else
    {
      digitalWrite(RELAY_PIN, HIGH);
      fanOn = false;
    }

    // LIGHT LOGIC

    if (ldr < 2000)
    {
      digitalWrite(LED_PIN, HIGH);
      lightOn = true;
    }
    else
    {
      digitalWrite(LED_PIN, LOW);
      lightOn = false;
    }

    // LCD

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temp);

    lcd.print(" H:");
    lcd.print(hum);

    lcd.setCursor(0, 1);
    lcd.print("L:");
    lcd.print(ldr);

    lcd.print(" G:");
    lcd.print(gas);

    // FIREBASE

    Firebase.RTDB.setFloat(
      &fbdo,
      "/SmartHome/temperature",
      temp);

    Firebase.RTDB.setFloat(
      &fbdo,
      "/SmartHome/humidity",
      hum);

    Firebase.RTDB.setInt(
      &fbdo,
      "/SmartHome/ldr",
      ldr);

    Firebase.RTDB.setInt(
      &fbdo,
      "/SmartHome/mq2",
      gas);

    Firebase.RTDB.setString(
      &fbdo,
      "/SmartHome/fanStatus",
      fanOn ? "ON" : "OFF");

    Firebase.RTDB.setString(
      &fbdo,
      "/SmartHome/lightStatus",
      lightOn ? "ON" : "OFF");

    delay(2000);
  }
}