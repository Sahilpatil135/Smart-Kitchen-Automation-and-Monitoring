// ======================= Smart Kitchen Automation IoT Project =======================
// Upload this to your ESP8266/NodeMCU via Arduino IDE.
// Replace the placeholders with your own credentials and tokens before uploading.
//
// -----------------------------------------------------------------------------------
// Blynk + ThingSpeak + MQ135 + DHT11 + OLED + PIR + Relays Automation
// -----------------------------------------------------------------------------------

// -------------------- Blynk Template Configuration --------------------
#define BLYNK_TEMPLATE_ID "YOUR_BLYNK_TEMPLATE_ID"        // Replace with your Blynk Template ID
#define BLYNK_TEMPLATE_NAME "YOUR_BLYNK_TEMPLATE_NAME"    // Replace with your Blynk Template Name
#define BLYNK_PRINT Serial

// -------------------- Library Includes --------------------
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <SPI.h>
#include <Wire.h>
#include "MQ135.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// -------------------- Blynk Auth & Wi-Fi Credentials --------------------
BlynkTimer myTimer;  // Create a timer object

char auth[] = "YOUR_BLYNK_AUTH_TOKEN";   // Replace with your Blynk Auth Token
char ssid[] = "YOUR_WIFI_SSID";          // Replace with your WiFi SSID
char pass[] = "YOUR_WIFI_PASSWORD";      // Replace with your WiFi Password

// -------------------- OLED Display Configuration --------------------
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET -1     // Reset pin # (or -1 if sharing Arduino reset pin)

// -------------------- Sensors and Actuators --------------------
#define DHTTYPE DHT11  // DHT 11
#define DHTPIN D4

#define relay_fan D5
#define relay_light D6
#define relay_fridge D7
#define relay_oven D8

#define buzzer_alarm D0
#define pir_human D3

#define MQ135_PIN A0

// Calibrated value of MQ135 gas sensor (adjust after calibration)
float RZERO = 24.85;

int alarm_status;
int pir_status = 0;

DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -------------------- Blynk Virtual Pins --------------------
BLYNK_WRITE(V1) {  // Button for relay_light
  int pinValue = param.asInt();
  digitalWrite(relay_light, pinValue ? LOW : HIGH);
}

BLYNK_WRITE(V2) {  // Button for relay_fridge
  int pinValue = param.asInt();
  digitalWrite(relay_fridge, pinValue ? LOW : HIGH);
}

BLYNK_WRITE(V3) {                                   // Button for oven control
  int pinValue = param.asInt();                     // Get button state (0 or 1)
  digitalWrite(relay_oven, pinValue ? LOW : HIGH);  // LOW = ON, HIGH = OFF
}

BLYNK_WRITE(V0) {                // Button for relay_fan
  int pinValue = param.asInt();  // Get value from button (0 or 1)

  if (pinValue == 1) {
    // Button is ON, turn the fan ON (relay is LOW)
    digitalWrite(relay_fan, LOW);
    Serial.println("Fan ON");
  } else {
    // Button is OFF, turn the fan OFF (relay is HIGH)
    digitalWrite(relay_fan, HIGH);
    Serial.println("Fan OFF");
  }
}

// -------------------- Setup --------------------
void setup() {
  Serial.begin(115200);
  dht.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (128x64)
  Blynk.begin(auth, ssid, pass);

  pinMode(pir_human, INPUT);
  pinMode(buzzer_alarm, OUTPUT);

  pinMode(relay_fan, OUTPUT);
  pinMode(relay_light, OUTPUT);
  pinMode(relay_fridge, OUTPUT);
  pinMode(relay_oven, OUTPUT);

  digitalWrite(buzzer_alarm, LOW);

  digitalWrite(relay_fan, HIGH);
  digitalWrite(relay_light, HIGH);
  digitalWrite(relay_fridge, HIGH);
  digitalWrite(relay_oven, HIGH);
  Serial.println("Warming up MQ135 Sensor...");
  delay(60000);
}

// -------------------- AQI Calculation --------------------
float calculateAQI(float ppm) {
  // Define PPM and AQI breakpoints
  if (ppm <= 400) return map(ppm, 0, 400, 0, 50);
  else if (ppm <= 1000) return map(ppm, 401, 1000, 51, 100);
  else if (ppm <= 2000) return map(ppm, 1001, 2000, 101, 150);
  else if (ppm <= 5000) return map(ppm, 2001, 5000, 151, 200);
  else if (ppm <= 10000) return map(ppm, 5001, 10000, 201, 300);
  else return 500;  // Above 10000 PPM is Hazardous
}

// -------------------- ThingSpeak Integration --------------------
void sendToThingSpeak(float temperature, float humidity, float air_quality, int pir_status) {
  WiFiClient client;
  HTTPClient http;

  // Replace YOUR_THINGSPEAK_API_KEY with your own key
  String url = "http://api.thingspeak.com/update?api_key=YOUR_THINGSPEAK_API_KEY";
  url += "&field1=" + String(temperature);
  url += "&field2=" + String(humidity);
  url += "&field3=" + String(air_quality);
  url += "&field4=" + String(pir_status);

  http.begin(client, url);
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    Serial.print("ThingSpeak Response: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("Error sending data: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

unsigned long previousMillis = 0;
const long thingspeakInterval = 20000;  // 20 seconds

// Global variables to store sensor readings
float air_quality, AQI, t, h;

// -------------------- Main Loop --------------------
void loop() {
  Blynk.run();
  // myTimer.run();

  MQ135 gasSensor = MQ135(MQ135_PIN, RZERO);
  float air_quality = gasSensor.getPPM();
  float AQI = calculateAQI(air_quality);

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  pir_status = digitalRead(pir_human);
  alarm_status = digitalRead(buzzer_alarm);

  if (pir_status == 1) {
    Serial.println("Person Detected");
    digitalWrite(relay_light, LOW);  // Turn ON the light (LOW = ON)
    Blynk.virtualWrite(V1, 1);       // Update Blynk Button state if needed
  } else {
    Serial.println("No One in Room");
    digitalWrite(relay_light, HIGH);  // Turn OFF the light (HIGH = OFF)
    Blynk.virtualWrite(V1, 0);        // Update Blynk Button state if needed
  }

  if (air_quality > 1000) {
    digitalWrite(buzzer_alarm, HIGH);
    digitalWrite(relay_fan, LOW);               // if fan button is not switched on in Blynk, add Blynk.virtualWrite(V0, 1);
    Serial.println("Buzzer Status: ON");
    Serial.println("Exhaust Fan: ON");
  } else {
    digitalWrite(buzzer_alarm, LOW);
    digitalWrite(relay_fan, HIGH);          
    Serial.println("Buzzer Status: OFF");
    Serial.println("Exhaust Fan: OFF");
  }

  Serial.print("Air Quality: ");
  Serial.print(air_quality);
  Serial.println(" PPM");

  Serial.print("AQI: ");
  Serial.println(AQI);

  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.println(" *C");

  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.println(" %");

  Serial.println();
  Serial.println("****************************");
  Serial.println();

  // Update OLED Display
  updateOLED(t, h, air_quality);

  Blynk.virtualWrite(V4, t);             // For Temperature
  Blynk.virtualWrite(V5, h);             // For Humidity
  Blynk.virtualWrite(V6, air_quality);   // For Gas
  Blynk.virtualWrite(V8, alarm_status);  // For Alarm & Exhaust Fan
  Blynk.virtualWrite(V7, pir_status);    // For Human Detection
  Blynk.virtualWrite(V9, AQI);           // For AQI

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= thingspeakInterval) {
    previousMillis = currentMillis;  // Update time
    sendToThingSpeak(t, h, air_quality, pir_status);
  }

  delay(3000);
}

// -------------------- OLED Update Function --------------------
unsigned long previousOLEDUpdate = 0;
const long oledUpdateInterval = 3000;  // Update every 3 seconds

// Function to update the OLED display without delays
void updateOLED(float t, float h, float air_quality) {
  unsigned long currentMillis = millis();

  if (currentMillis - previousOLEDUpdate >= oledUpdateInterval) {
    previousOLEDUpdate = currentMillis;  // Update timestamp

    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.println("Air Quality (PPM)");

    display.setCursor(0, 20);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.print(air_quality);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.println(" PPM");
    display.display();

    delay(1500);  // Small delay for readability

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Temperature: ");
    display.setTextSize(2);
    display.setCursor(0, 10);
    display.print(t);
    display.print(" ");
    display.setTextSize(1);
    display.cp437(true);
    display.write(167);
    display.setTextSize(2);
    display.print("C");

    display.setTextSize(1);
    display.setCursor(0, 35);
    display.print("Humidity: ");
    display.setTextSize(2);
    display.setCursor(0, 45);
    display.print(h);
    display.print(" %");

    display.display();
  }
}

// -------------------- Calibration of MQ135 gas sensor --------------------
// Run this once to calibrate the gas sensor to your surroundings before using it

// #include <MQ135.h>

// #define MQ135_PIN A0  // MQ135 connected to A0 pin
// MQ135 gasSensor(MQ135_PIN);

// void setup() {
//     Serial.begin(115200);
//     Serial.println("Calibrating... Please wait.");
//     delay(20000); // Let the sensor heat up for 20 seconds

//     float R0 = gasSensor.getRZero();  // Get baseline resistance (R0)

//     Serial.print("Calibration Done! R0 Value: ");
//     Serial.println(R0);
// }

// void loop() {
//     // Nothing here, just run once
// } 
