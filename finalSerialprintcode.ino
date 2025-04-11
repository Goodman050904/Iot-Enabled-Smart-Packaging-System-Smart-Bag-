#define BLYNK_TEMPLATE_ID "TMPL3bKaPvTg2"
#define BLYNK_TEMPLATE_NAME "Smart Packaging"
#define BLYNK_AUTH_TOKEN "q0510MAFnnPfsRw0suk59KDdzwcXQZrP"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <SPI.h>
#include <MFRC522.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <TinyGPS++.h>

// WiFi credentials
char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Airtel_Ajay &Satya";
char pass[] = "77777777";

// Pins
#define MQ2_PIN 34
#define LDR_PIN 25
#define BUZZER_PIN 15
#define RST_PIN 22
#define SS_PIN 21
#define GPS_TX_PIN 17
#define GPS_RX_PIN 16

// RFID
MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;
int blockNum = 2;
byte bufferLen = 18;
byte readBlockData[18];
String card_holder_name;
const String sheet_url = "https://script.google.com/macros/s/AKfycbwzSt3PE1WWMfZi9kU3JgyTtEXKx9CA621t3ewH9J6wvPyNYywCcPPV81i4nwMBH1ZW/exec?name=";

// Sensors
Adafruit_MPU6050 mpu;
Adafruit_BMP280 bmp;
TinyGPSPlus gps;
float lat = 0, lng = 0;

// Buzzer control from Blynk
BLYNK_WRITE(V12) {
  int value = param.asInt();
  digitalWrite(BUZZER_PIN, value);  // ON or OFF
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  SPI.begin();
  Serial1.begin(9600, SERIAL_8N1, GPS_TX_PIN, GPS_RX_PIN);
  
  pinMode(LDR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Blynk.begin(auth, ssid, pass);

  if (!bmp.begin(0x76)) {
    Serial.println("BMP280 not found!");
    while (1);
  }

  if (!mpu.begin()) {
    Serial.println("MPU6050 not found!");
    while (1);
  }

  mfrc522.PCD_Init();
  Serial.println("Smart Packaging System Initialized.");
}

void loop() {
  Blynk.run();

  // Sensor Readings
  int mq2Value = analogRead(MQ2_PIN);
  int ldrStatus = digitalRead(LDR_PIN);
  String packageStatus = (ldrStatus == 0) ? "1" : "0";
  float temp = bmp.readTemperature();
  float pressure = bmp.readPressure() / 100.0;

  sensors_event_t a, g, temp_event;
  mpu.getEvent(&a, &g, &temp_event);

  // Read GPS
  unsigned long start = millis();
  while (millis() - start < 2000) {
    while (Serial1.available()) {
      gps.encode(Serial1.read());
    }
  }
  if (gps.location.isValid()) {
    lat = gps.location.lat();
    lng = gps.location.lng();
  }

  // Send to Blynk
  Blynk.virtualWrite(V2, temp);
  Blynk.virtualWrite(V3, pressure);
  Blynk.virtualWrite(V5, mq2Value);
  Blynk.virtualWrite(V4, a.acceleration.x);
  Blynk.virtualWrite(V10, a.acceleration.y);
  Blynk.virtualWrite(V11, a.acceleration.z);
  Blynk.virtualWrite(V7, g.gyro.x);
  Blynk.virtualWrite(V8, g.gyro.y);
  Blynk.virtualWrite(V9, g.gyro.z);
  Blynk.virtualWrite(V6, packageStatus);
  Blynk.virtualWrite(V0, lat);
  Blynk.virtualWrite(V1, lng);

  // Print to Serial Monitor
  Serial.println("------ Smart Package Sensor Readings ------");
  Serial.print("Temperature (°C): "); Serial.println(temp);
  Serial.print("Pressure (hPa): "); Serial.println(pressure);
  Serial.print("MQ2 Gas Value: "); Serial.println(mq2Value);
  Serial.print("LDR Status (0=open, 1=closed): "); Serial.println(ldrStatus);
  Serial.print("Accelerometer - X: "); Serial.print(a.acceleration.x);
  Serial.print(" | Y: "); Serial.print(a.acceleration.y);
  Serial.print(" | Z: "); Serial.println(a.acceleration.z);
  Serial.print("Gyroscope - X: "); Serial.print(g.gyro.x);
  Serial.print(" | Y: "); Serial.print(g.gyro.y);
  Serial.print(" | Z: "); Serial.println(g.gyro.z);
  Serial.print("Latitude: "); Serial.println(lat, 6);
  Serial.print("Longitude: "); Serial.println(lng, 6);
  Serial.println("--------------------------------------------\n");

  // RFID
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    ReadDataFromBlock(blockNum, readBlockData);
    Serial.print("RFID Data: ");
    for (int j = 0; j < 16; j++) {
      Serial.write(readBlockData[j]);
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      WiFiClientSecure client;
      client.setInsecure();
      card_holder_name = sheet_url + String((char*)readBlockData);
      card_holder_name.trim();

      HTTPClient https;
      if (https.begin(client, card_holder_name)) {
        int httpCode = https.GET();
        if (httpCode > 0) {
          Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
        } else {
          Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }
        https.end();
      } else {
        Serial.println("[HTTPS] Unable to connect");
      }
    }
  }

  delay(2000);
}

void ReadDataFromBlock(int blockNum, byte readBlockData[]) {
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNum, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Authentication failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  status = mfrc522.MIFARE_Read(blockNum, readBlockData, &bufferLen);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Reading failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  Serial.println("Block was read successfully");
}
