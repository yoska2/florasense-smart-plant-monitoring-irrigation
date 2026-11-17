/*
 * FloraSense - Smart Plant Monitoring & Irrigation System
 *
 * Course: Robotics for Computer Science
 * Institute: HIT - Holon Institute of Technology
 *
 * Group Members:
 * Yosef Yosef 
 * Ziv Ashkenazi 
 */

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <Preferences.h>

#include <BH1750.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <LiquidCrystal.h>

// =====================================================
// WIFI
// =====================================================

const char* ssid = "YOSI";
const char* password = "211364740";

WebServer server(80);
Preferences preferences;

// =====================================================
// THRESHOLDS
// The website sends only two thresholds.
// One soil threshold is shared by all 4 plants.
// One minimum light threshold is used for the light sensor.
// =====================================================

int soilMinThreshold = 10;
int lightMinThreshold = 10;  // lux

// =====================================================
// LCD 20x4
// RS, E, D4, D5, D6, D7
// =====================================================

LiquidCrystal lcd(27, 26, 25, 14, 13, 4);

// =====================================================
// I2C SENSORS
// =====================================================

BH1750 lightMeter;
Adafruit_BME280 bme;

bool bmeOK = false;
bool bh1750OK = false;

// =====================================================
// SOIL MOISTURE
// =====================================================

const int soil1Pin = 32;

// Calibration values for the single soil sensor.
// Adjust these later after measuring the sensor dry and wet.
int soilDry = 3000;
int soilWet = 1300;

// Soil moisture in percent (0-100).
int soilPercent = 0;

// Automatic watering settings.
const unsigned long wateringTime = 3000;       // Pump runs for 3 seconds.
const unsigned long wateringCooldown = 30000;  // Wait 30 seconds before watering again.
unsigned long lastWateringTime = 0;

// =====================================================
// FLOAT SWITCH
// GPIO16 -> switch -> GND
// LOW  = יש מים
// HIGH = אין מים
// =====================================================

const int floatSwitchPin = 16;

// =====================================================
// RELAY - SINGLE NEW MODULE
// GPIO23 -> IN
// ACTIVE LOW (verified on the actual relay)
// LOW  = ON
// HIGH = OFF
// =====================================================

const int relay1 = 23;

// =====================================================
// RGB
// GPIO -> 1K resistor -> transistor base
// =====================================================

const int RED_PIN = 5;
const int GREEN_PIN = 2;
const int BLUE_PIN = 15;

// =====================================================
// LCD PAGE TIMER
// =====================================================

unsigned long lastPageChange = 0;
const unsigned long pageTime = 10000;

bool page = false;

// New settings LCD confirmation
bool showNewSettings = false;
unsigned long newSettingsDisplayUntil = 0;
const unsigned long newSettingsDisplayTime = 5000;

// =====================================================
// RED BLINK TIMER
// =====================================================

unsigned long lastBlink = 0;
const unsigned long blinkTime = 500;

bool redBlinkState = false;

// =====================================================
// PUMP STATUS
// =====================================================

bool pumpRunning = false;
unsigned long pumpStopTime = 0;
bool manualPumpMode = false;
unsigned long manualPumpStopTime = 0;

// =====================================================
// CURRENT SENSOR VALUES
// =====================================================

int soil1 = 0;

float temperature = 0.0;
float humidity = 0.0;
float lux = 0.0;

bool waterOK = false;

// =====================================================
// RGB FUNCTIONS
// =====================================================

void setColor(bool red, bool green, bool blue) {

  digitalWrite(RED_PIN, red ? HIGH : LOW);
  digitalWrite(GREEN_PIN, green ? HIGH : LOW);
  digitalWrite(BLUE_PIN, blue ? HIGH : LOW);
}

void greenLight() {

  setColor(false, true, false);
}

void blueLight() {

  setColor(false, false, true);
}

void redLight() {

  setColor(true, false, false);
}

void lightsOff() {

  setColor(false, false, false);
}

// =====================================================
// CONVERT RAW SOIL VALUE TO PERCENT
// =====================================================

int rawToSoilPercent(int rawValue) {

  if (soilDry == soilWet) {
    return 0;
  }

  // Most capacitive soil sensors give a lower value when the soil is wetter.
  int percent = map(rawValue, soilDry, soilWet, 0, 100);
  return constrain(percent, 0, 100);
}

// =====================================================
// STOP ALL PUMPS
// =====================================================

void stopAllPumps() {
  digitalWrite(relay1, HIGH);
  pumpRunning = false;
}

// =====================================================
// PUMP CONTROL - ONE SOIL SENSOR + ONE PUMP
// Pump can run only when the float switch says water is available.
// =====================================================

void updatePumps(bool waterAvailable) {

  // Manual activation from the website: exactly 3 seconds,
  // intentionally ignoring the float switch.
  if (manualPumpMode) {
    if ((long)(millis() - manualPumpStopTime) >= 0) {
      digitalWrite(relay1, HIGH);
      pumpRunning = false;
      manualPumpMode = false;
      lastWateringTime = millis();
      Serial.println("Manual pump OFF after 3 seconds");
    } else {
      digitalWrite(relay1, LOW);
      pumpRunning = true;
    }
    return;
  }

  // Automatic mode: float-switch safety remains active.
  if (!waterAvailable) {
    digitalWrite(relay1, HIGH);
    pumpRunning = false;
    return;
  }

  // Automatic watering: stop after wateringTime.
  if (pumpRunning) {
    if ((long)(millis() - pumpStopTime) >= 0) {
      digitalWrite(relay1, HIGH);
      pumpRunning = false;
      lastWateringTime = millis();
      Serial.println("Pump 1 OFF");
    }
    return;
  }

  bool soilTooDry = soilPercent < soilMinThreshold;
  bool neverWatered = lastWateringTime == 0;
  bool cooldownFinished =
    neverWatered || (millis() - lastWateringTime >= wateringCooldown);

  if (soilTooDry && cooldownFinished) {
    digitalWrite(relay1, LOW);
    pumpRunning = true;
    pumpStopTime = millis() + wateringTime;
    Serial.println("Automatic watering: Pump 1 ON");
  }
}

// =====================================================
// MANUAL PUMP API
// POST /pump?action=on
// POST /pump?action=off
// =====================================================

void handlePumpControl() {
  addCorsHeaders();

  if (!server.hasArg("action")) {
    server.send(400, "application/json", "{\"error\":\"Missing action\"}");
    return;
  }

  String action = server.arg("action");
  action.toLowerCase();

  if (action == "on") {
    // Manual test ignores water level and stops automatically after 3 seconds.
    manualPumpMode = true;
    manualPumpStopTime = millis() + wateringTime;
    digitalWrite(relay1, LOW);
    pumpRunning = true;

    server.send(200, "application/json",
                "{\"success\":true,\"mode\":\"manual\",\"durationMs\":3000,\"pumpRunning\":true}");
    return;
  }

  if (action == "off") {
    manualPumpMode = false;
    digitalWrite(relay1, HIGH);
    pumpRunning = false;
    lastWateringTime = millis();

    server.send(200, "application/json",
                "{\"success\":true,\"mode\":\"auto\",\"pumpRunning\":false}");
    return;
  }

  server.send(400, "application/json", "{\"error\":\"action must be on or off\"}");
}

// =====================================================
// RGB STATUS
// =====================================================

void updateBacklight(bool waterAvailable) {

  // ---------------------------------------------------
  // NO WATER -> RED FLASHING
  // ---------------------------------------------------

  if (!waterAvailable) {

    if (millis() - lastBlink >= blinkTime) {

      lastBlink = millis();

      redBlinkState = !redBlinkState;
    }

    if (redBlinkState) {
      redLight();
    }

    else {
      lightsOff();
    }

    return;
  }

  // ---------------------------------------------------
  // PUMP RUNNING -> BLUE
  // ---------------------------------------------------

  if (pumpRunning) {

    blueLight();

    return;
  }

  // ---------------------------------------------------
  // NORMAL -> GREEN
  // ---------------------------------------------------

  greenLight();
}

// =====================================================
// CORS
// מאפשר לאתר לפנות ל-ESP32
// =====================================================

void addCorsHeaders() {

  server.sendHeader(
    "Access-Control-Allow-Origin",
    "*"
  );

  server.sendHeader(
    "Access-Control-Allow-Methods",
    "GET,POST,OPTIONS"
  );

  server.sendHeader(
    "Access-Control-Allow-Headers",
    "Content-Type"
  );
}

// =====================================================
// API - STATUS
//
// GET /status
// =====================================================

void handleStatus() {

  addCorsHeaders();

  String json = "{";

  json += "\"status\":\"ok\",";
  json += "\"ip\":\"";
  json += WiFi.localIP().toString();
  json += "\",";
  json += "\"soilMin\":";
  json += String(soilMinThreshold);
  json += ",";
  json += "\"lightMin\":";
  json += String(lightMinThreshold);

  json += "}";

  server.send(
    200,
    "application/json",
    json
  );
}

// =====================================================
// API - SENSOR DATA
//
// GET /sensors
// =====================================================

void handleSensors() {

  addCorsHeaders();

  String json = "{";

  json += "\"soil1\":";
  json += String(soilPercent);
  json += ",";

  json += "\"temperature\":";
  json += String(temperature, 1);
  json += ",";

  json += "\"humidity\":";
  json += String(humidity, 1);
  json += ",";

  json += "\"light\":";
  json += String(lux, 0);
  json += ",";

  json += "\"lightStatus\":\"";
  if (!bh1750OK) {
    json += "SENSOR_ERROR";
  } else if (lux < lightMinThreshold) {
    json += "LOW";
  } else {
    json += "OK";
  }
  json += "\",";

  json += "\"waterOK\":";
  json += waterOK ? "true" : "false";
  json += ",";

  json += "\"pumpRunning\":";
  json += pumpRunning ? "true" : "false";
  json += ",";

  json += "\"pumpMode\":\"";
  json += manualPumpMode ? "manual" : "auto";
  json += "\"";

  json += "}";

  server.send(
    200,
    "application/json",
    json
  );
}

// =====================================================
// API - GET CURRENT SETTINGS
//
// GET /settings
// =====================================================

void handleGetSettings() {

  addCorsHeaders();

  String json = "{";

  json += "\"soilMin\":";
  json += String(soilMinThreshold);
  json += ",";

  json += "\"lightMin\":";
  json += String(lightMinThreshold);

  json += "}";

  server.send(
    200,
    "application/json",
    json
  );
}

// =====================================================
// API - UPDATE SETTINGS
//
// POST /settings?soilMin=45&lightMin=60
// =====================================================

void handleUpdateSettings() {

  addCorsHeaders();

  if (
    !server.hasArg("soilMin") ||
    !server.hasArg("lightMin")
  ) {

    server.send(
      400,
      "application/json",
      "{\"error\":\"Missing soilMin or lightMin\"}"
    );

    return;
  }

  int newSoilMin =
    server.arg("soilMin").toInt();

  int newLightMin =
    server.arg("lightMin").toInt();

  if (
    newSoilMin < 0 ||
    newSoilMin > 100 ||
    newLightMin < 0 ||
    newLightMin > 100000
  ) {

    server.send(
      400,
      "application/json",
      "{\"error\":\"soilMin must be 0-100 and lightMin must be 0-100000 lux\"}"
    );

    return;
  }

  // ---------------------------------------------------
  // UPDATE THRESHOLDS
  // ---------------------------------------------------

  soilMinThreshold = newSoilMin;
  lightMinThreshold = newLightMin;

  // ---------------------------------------------------
  // SAVE IN ESP32 MEMORY
  // ---------------------------------------------------

  preferences.putInt(
    "soilMin",
    soilMinThreshold
  );

  preferences.putInt(
    "lightMin",
    lightMinThreshold
  );

  // ---------------------------------------------------
  // SERIAL
  // ---------------------------------------------------

  Serial.println();
  Serial.println("NEW SETTINGS RECEIVED");

  Serial.print("Soil minimum: ");
  Serial.print(soilMinThreshold);
  Serial.println("%");

  Serial.print("Light minimum: ");
  Serial.print(lightMinThreshold);
  Serial.println(" lux");

  // ---------------------------------------------------
  // RESPONSE
  // ---------------------------------------------------

  String json = "{";

  json += "\"success\":true,";
  json += "\"soilMin\":";
  json += String(soilMinThreshold);
  json += ",";
  json += "\"lightMin\":";
  json += String(lightMinThreshold);

  json += "}";

  server.send(
    200,
    "application/json",
    json
  );

  // Show the two new thresholds on the LCD for 5 seconds
  // without blocking the web server or sensor loop.
  showNewSettings = true;
  newSettingsDisplayUntil = millis() + newSettingsDisplayTime;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("NEW SETTINGS SAVED");
  lcd.setCursor(0, 1);
  lcd.print("Soil Min: " );
  lcd.print(soilMinThreshold);
  lcd.print("%");
  lcd.setCursor(0, 2);
  lcd.print("Light Min: " );
  lcd.print(lightMinThreshold);
  lcd.setCursor(0, 3);
  lcd.print("lux");
}

// =====================================================
// WIFI CONNECTION
// =====================================================

void connectToWiFi() {

  Serial.println();
  Serial.println("Connecting to WiFi...");

  // הצגה על ה-LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int dots = 0;

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");

    // אנימציה קטנה על המסך
    lcd.setCursor(dots, 1);
    lcd.print(".");

    dots++;

    if (dots >= 20) {
      lcd.setCursor(0, 1);
      lcd.print("                    ");
      dots = 0;
    }
  }

  // הצליח להתחבר
  Serial.println();
  Serial.println("WiFi connected!");

  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // הצגת הצלחה על ה-LCD
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected!");

  lcd.setCursor(0, 1);
  lcd.print("IP Address:");

  lcd.setCursor(0, 2);
  lcd.print(WiFi.localIP());

  lcd.setCursor(0, 3);
  lcd.print("FloraSense Ready");

  // משאיר את ה-IP על המסך 5 שניות
  delay(5000);

  lcd.clear();
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  // ---------------------------------------------------
  // LOAD SAVED SETTINGS
  // ---------------------------------------------------

  preferences.begin(
    "flora1",
    false
  );

  soilMinThreshold =
    preferences.getInt(
      "soilMin",
      10
    );

  lightMinThreshold =
    preferences.getInt(
      "lightMin",
      10
    );

  // ---------------------------------------------------
  // FLOAT SWITCH
  // ---------------------------------------------------

  pinMode(
    floatSwitchPin,
    INPUT_PULLUP
  );

  // ---------------------------------------------------
  // RELAYS
  // ---------------------------------------------------

  pinMode(relay1, OUTPUT);

  stopAllPumps();

  // ---------------------------------------------------
  // RGB
  // ---------------------------------------------------

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  lightsOff();

  // ---------------------------------------------------
  // I2C
  // SDA = GPIO21
  // SCL = GPIO22
  // ---------------------------------------------------

  Wire.begin(21, 22);

  // ---------------------------------------------------
  // LCD
  // ---------------------------------------------------

  lcd.begin(20, 4);

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Smart Irrigation");

  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  // ---------------------------------------------------
  // BH1750
  // ---------------------------------------------------

  bh1750OK = lightMeter.begin();

  if (bh1750OK) {

    Serial.println("BH1750 FOUND");
  }

  else {

    Serial.println("BH1750 NOT FOUND");
  }

  // ---------------------------------------------------
  // BME280
  // Address = 0x76
  // ---------------------------------------------------

  bmeOK = bme.begin(0x76);

  if (bmeOK) {

    Serial.println("BME280 FOUND");
  }

  else {

    Serial.println("BME280 NOT FOUND");

    lcd.setCursor(0, 2);
    lcd.print("BME280 ERROR");
  }

  // ---------------------------------------------------
  // WIFI
  // ---------------------------------------------------

  connectToWiFi();

  // ---------------------------------------------------
  // mDNS - fixed local hostname
  // Website can use: http://florasense.local
  // ---------------------------------------------------
  if (MDNS.begin("florasense")) {
    Serial.println("mDNS started: http://florasense.local");
  } else {
    Serial.println("mDNS failed - use the IP shown on the LCD");
  }

  // ---------------------------------------------------
  // HTTP API
  // ---------------------------------------------------

  server.on(
    "/status",
    HTTP_GET,
    handleStatus
  );

  server.on(
    "/sensors",
    HTTP_GET,
    handleSensors
  );

  server.on(
    "/settings",
    HTTP_GET,
    handleGetSettings
  );

  server.on(
    "/settings",
    HTTP_POST,
    handleUpdateSettings
  );

  server.on(
    "/pump",
    HTTP_POST,
    handlePumpControl
  );

  server.begin();

  Serial.println();
  Serial.println("HTTP server started");

  Serial.print("ESP32 URL: http://");
  Serial.println(WiFi.localIP());

  Serial.print("Soil minimum threshold: ");
  Serial.println(soilMinThreshold);

  Serial.print("Light minimum threshold: ");
  Serial.println(lightMinThreshold);

  delay(2000);

  lcd.clear();

  lastPageChange = millis();
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  // ===================================================
  // HTTP SERVER
  // ===================================================

  server.handleClient();

  // ===================================================
  // FLOAT SWITCH
  // ===================================================

  waterOK =
    (digitalRead(floatSwitchPin) == LOW);

  // ===================================================
  // READ SOIL SENSOR
  // ===================================================

  soil1 = analogRead(soil1Pin);
  soilPercent = rawToSoilPercent(soil1);

  // ===================================================
  // PUMP CONTROL
  // ===================================================

  updatePumps(waterOK);

  // ===================================================
  // RGB STATUS
  // ===================================================

  updateBacklight(waterOK);

  // ===================================================
  // BME280
  // ===================================================

  if (bmeOK) {

    temperature = bme.readTemperature();
    humidity = bme.readHumidity();
  }

  // ===================================================
  // BH1750
  // ===================================================

  if (bh1750OK) {

    lux = lightMeter.readLightLevel();
  }

  // ===================================================
  // LCD DISPLAY
  // ===================================================

  if (showNewSettings) {
    if ((long)(millis() - newSettingsDisplayUntil) >= 0) {
      showNewSettings = false;
      lcd.clear();
      lastPageChange = millis();
    }
  } else {
    // ===================================================
    // LCD PAGE CHANGE
    // ===================================================

    if (
      millis() - lastPageChange >= pageTime
    ) {

      page = !page;

      lastPageChange = millis();

      lcd.clear();
    }

    // ===================================================
    // PAGE 1
    // SOIL + SYSTEM STATUS
    // ===================================================

    if (!page) {

      lcd.setCursor(0, 0);
      lcd.print("SOIL MOISTURE       ");

      lcd.setCursor(0, 1);
      lcd.print("Soil: ");
      lcd.print(soilPercent);
      lcd.print("%           ");

      lcd.setCursor(0, 2);
      lcd.print("Min: ");
      lcd.print(soilMinThreshold);
      lcd.print("%            ");

      lcd.setCursor(0, 3);

      if (pumpRunning && manualPumpMode) {

        lcd.print("MANUAL PUMP 3 SEC   ");
      }

      else if (!waterOK) {

        lcd.print("NO WATER - STOPPED! ");
      }

      else if (pumpRunning) {

        lcd.print("WATERING PUMP 1     ");
      }

      else {

        lcd.print("SYSTEM NORMAL       ");
      }
    }

    // ===================================================
    // PAGE 2
    // TEMPERATURE / HUMIDITY / LIGHT
    // ===================================================

    else {

      lcd.setCursor(0, 0);
      lcd.print("ENVIRONMENT         ");

      // Temperature

      lcd.setCursor(0, 1);

      if (bmeOK) {

        lcd.print("Temp: ");
        lcd.print(temperature, 1);
        lcd.print(" C       ");
      }

      else {

        lcd.print("Temp: ERROR         ");
      }

      // Humidity

      lcd.setCursor(0, 2);

      if (bmeOK) {

        lcd.print("Humidity: ");
        lcd.print(humidity, 1);
        lcd.print("%   ");
      }

      else {

        lcd.print("Humidity: ERROR     ");
      }

      // Light

      lcd.setCursor(0, 3);

      if (bh1750OK) {

        lcd.print("Light: ");
        lcd.print(lux, 0);
        lcd.print(" lux      ");
      }

      else {

        lcd.print("Light: ERROR        ");
      }
    }

  }

  // ===================================================
  // SERIAL MONITOR
  // ===================================================

  Serial.print("SoilRaw=");
  Serial.print(soil1);
  Serial.print(" (");
  Serial.print(soilPercent);
  Serial.print("%)");

  Serial.print(" | Temp=");

  if (bmeOK) {

    Serial.print(temperature, 1);
    Serial.print("C");

    Serial.print(" | Hum=");
    Serial.print(humidity, 1);
    Serial.print("%");
  }

  else {

    Serial.print("ERROR");
  }

  Serial.print(" | Light=");

  if (bh1750OK) {

    Serial.print(lux);
    Serial.print(" lux");

    if (lux < lightMinThreshold) {
      Serial.print(" (LOW)");
    } else {
      Serial.print(" (OK)");
    }
  }

  else {

    Serial.print("ERROR");
  }

  Serial.print(" | Water=");

  if (waterOK) {

    Serial.print("OK");
  }

  else {

    Serial.print("EMPTY");
  }

  Serial.print(" | Pump=");

  if (pumpRunning) {

    Serial.print("1 ON");
  }

  else {

    Serial.print("OFF");
  }

  // Settings received from the website.

  Serial.print(" | SoilMin=");
  Serial.print(soilMinThreshold);
  Serial.print("%");

  Serial.print(" | LightMin=");
  Serial.print(lightMinThreshold);
  Serial.println(" lux");

  delay(50);
}


