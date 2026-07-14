#define BLYNK_TEMPLATE_ID "TMPL6Mn9loapO"
#define BLYNK_TEMPLATE_NAME "IoT Project"
#define BLYNK_AUTH_TOKEN "OjLKkSiz3JguQ5kANWpdYweqdiyQyksf"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32Servo.h>
#include <HX711.h>

// ---------- WiFi ----------
const char* ssidList[] = {
  "Harvey's Galaxy A52s 5G",
  "Josh",
  "bb",
  "Reyb",
  "PLDTHOMEFIBRyMA4U",
  "Strange-1-EXT"
};
const char* passList[] = {
  "aifi1615",
  "babydylan12",
  "12121212",
  "wifirave",
  "PLDTWIFIN5f73",
  "Reign1$1"
};

const int WIFI_COUNT = 6;

// ---------- Blynk ----------
BlynkTimer timer;

// ---------- Pins ----------
const int SOIL_PIN    = 36;   // Soil moisture sensor
const int DS18B20_PIN = 13;   // DS18B20 data pin

// LED pins
const int RED_LED_PIN   = 14;
const int GREEN_LED_PIN = 23;

// ---------- Drill Motor (L298N) ----------
const int DRILL_IN1 = 4;
const int DRILL_IN2 = 5;

// Soil button timing
const unsigned long SOIL_BUTTON_INTERVAL = 2000;
bool soilButtonLocked = false;
unsigned long soilButtonPressTime = 0;

// ---------- Reconnection ----------
const unsigned long RECONNECT_INTERVAL = 10000; // 10 seconds
unsigned long lastReconnectAttempt = 0;

// ===== HX711 Weight Sensor Pins =====
const int HX711_DT  = 32;
const int HX711_SCK = 33;

// Servo Pins
// Seed Planting servo
const int SERVO1_PIN = 25;
// Soil Moisture Servo
const int SERVO2_PIN = 26;
// Drill Servo
const int SERVO3_PIN = 27;

// RIGHT motors -> OUT1 / OUT2
const int IN1 = 16;
const int IN2 = 17;

// LEFT motors -> OUT3 / OUT4. ,
const int IN3 = 18;
const int IN4 = 19;

// ---------- Joystick ----------
volatile int joyX = 127;
volatile int joyY = 127;

// Deadzone
const int CENTER_LOW  = 100;
const int CENTER_HIGH = 155;

// ---------- Soil Calibration ----------
const int SOIL_DRY = 3200;   // Dry soil value
const int SOIL_WET = 1400;   // Wet soil value

// ===== HX711 Calibration =====
float calibration_factor = -7050.0;

bool soilMonitoring = false;

// ---------- DS18B20 Setup ----------
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);

// ---------- Servo Objects ----------
Servo servo1;
Servo servo2;
Servo servo3;

// ===== HX711 Object =====
HX711 scale;

// ---------- Live Values ----------
int soilPercent = 0;
float tempC = NAN;

// ===== Weight Value =====
float weightG = 0.0;

// ---------- Functions ----------
int readSoilPercent() {
  int raw = analogRead(SOIL_PIN);

  raw = constrain(raw, SOIL_WET, SOIL_DRY);

  int pct = map(raw, SOIL_DRY, SOIL_WET, 0, 100);

  return constrain(pct, 0, 100);
}

float readTemperatureC() {
  ds18b20.requestTemperatures();

  float t = ds18b20.getTempCByIndex(0);

  if (t == DEVICE_DISCONNECTED_C) {
    return NAN;
  }

  return t;
}

// ===== Weight Reading Function =====
float readWeightGrams() {

  if (!scale.is_ready()) {
    return weightG;
  }

  float w = scale.get_units(10);

  return w;
}

// Servo Control Function
void setServoAngle(Servo &s, int state) {
  s.write(state ? 90 : 0);
}

// ---------- Motor Functions ----------
void stopMotors() {
  Serial.println("STOP");

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// FORWARD
void forward() {
  Serial.println("FORWARD");

  // RIGHT motors forward
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  // LEFT motors forward
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

// BACKWARD
void backward() {
  Serial.println("BACKWARD");

  // RIGHT motors backward
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  // LEFT motors backward
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

// TURN LEFT
void turnLeft() {
  Serial.println("TURN LEFT");

  // RIGHT motors forward
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  // LEFT motors backward
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

// TURN RIGHT
void turnRight() {
  Serial.println("TURN RIGHT");

  // RIGHT motors backward
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  // LEFT motors forward
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

// FORWARD LEFT
void forwardLeft() {
  Serial.println("FORWARD LEFT");

  // RIGHT motors forward
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  // LEFT motors stop
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// FORWARD RIGHT
void forwardRight() {
  Serial.println("FORWARD RIGHT");

  // RIGHT motors stop
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  // LEFT motors forward
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

// BACKWARD LEFT
void backwardLeft() {
  Serial.println("BACKWARD LEFT");

  // RIGHT motors backward
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  // LEFT motors stop
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// BACKWARD RIGHT
void backwardRight() {
  Serial.println("BACKWARD RIGHT");

  // RIGHT motors stop
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  // LEFT motors backward
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

// ---------- Drive Logic ----------
void driveCar() {

  int x = joyX;
  int y = joyY;

  Serial.print("X: ");
  Serial.print(x);
  Serial.print(" | Y: ");
  Serial.println(y);

  // STOP zone
  if (x >= CENTER_LOW && x <= CENTER_HIGH &&
      y >= CENTER_LOW && y <= CENTER_HIGH) {

    stopMotors();
    return;
  }

  bool up    = (y > CENTER_HIGH);
  bool down  = (y < CENTER_LOW);
  bool left  = (x < CENTER_LOW);
  bool right = (x > CENTER_HIGH);

  if (up && left)         forwardLeft();
  else if (up && right)   forwardRight();
  else if (down && left)  backwardLeft();
  else if (down && right) backwardRight();
  else if (up)            forward();
  else if (down)          backward();
  else if (left)          turnLeft();
  else if (right)         turnRight();
  else                    stopMotors();
}

void sendToBlynk() {
  // Read Sensors
  // soilPercent = readSoilPercent();
  tempC = readTemperatureC();
  weightG = readWeightGrams();

  // Send Data to Blynk
  // Blynk.virtualWrite(V0, soilPercent); // Soil Moisture
  Blynk.virtualWrite(V1, tempC);       // Temperature
  Blynk.virtualWrite(V2, weightG);

  // Serial Monitor
  // Serial.print("Soil Moisture: ");
  // Serial.print(soilPercent);
  Serial.print("% | Temperature: ");

  if (isnan(tempC)) {
    Serial.println("Sensor Error");
  } else {
    Serial.print(tempC);
    Serial.print(" °C");
  }

  Serial.print(" | Weight: ");
  Serial.print(weightG);
  Serial.println(" g");
}  

void sendSoilToBlynk() {
  if (!soilMonitoring) return;

  soilPercent = readSoilPercent();
  Blynk.virtualWrite(V0, soilPercent);   // Soil Moisture on label/value widget

  Serial.print("Soil Moisture: ");
  Serial.print(soilPercent);
  Serial.println("%");
}

void unlockSoilButton() {
  soilButtonLocked = false;
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);

  Serial.println("Soil button ready again");
}

// ---------- Blynk Servo Controls ----------
BLYNK_WRITE(V3) {
  setServoAngle(servo1, param.asInt());
}

BLYNK_WRITE(V4) {
  int state = param.asInt();

  if (state == 1) {
    if (soilButtonLocked) {
      Serial.println("Soil button locked for 2 seconds");
      return;
    }

    setServoAngle(servo2, 1);
    soilMonitoring = true;

    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);

    soilButtonLocked = true;
    soilButtonPressTime = millis();

    timer.setTimeout(SOIL_BUTTON_INTERVAL, unlockSoilButton);
  } 
  else {
    setServoAngle(servo2, 0);
    soilMonitoring = false;

    // Keep green LED on when the button can be pressed again
    if (!soilButtonLocked) {
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, HIGH);
    }

    Blynk.virtualWrite(V0, 0);   // optional reset on dashboard when off
  }
}

BLYNK_WRITE(V5)
{
  int value = param.asInt();

  Serial.print("Drill Button: ");
  Serial.println(value);

  if (value == 1)
  {
    Serial.println("Starting Drill");

    // Lower drill
    servo3.write(90);

    // Start drill motor
    digitalWrite(DRILL_IN1, HIGH);
    digitalWrite(DRILL_IN2, LOW);
  }
  else
  {
    Serial.println("Stopping Drill");

    // Stop drill motor
    digitalWrite(DRILL_IN1, LOW);
    digitalWrite(DRILL_IN2, LOW);

    // Raise drill
    servo3.write(0);
  }
}

BLYNK_WRITE(V7) {
  joyX = param.asInt();

  Serial.print("Received X: ");
  Serial.println(joyX);

  driveCar();
}

BLYNK_WRITE(V8) {
  joyY = param.asInt();

  Serial.print("Received Y: ");
  Serial.println(joyY);

  driveCar();
}

void reconnectBlynk() {

  // Already connected
  if (WiFi.status() == WL_CONNECTED && Blynk.connected()) {
    return;
  }

  Serial.println("Reconnecting...");

  // ---------------------------
  // Reconnect WiFi if needed
  // ---------------------------
  if (WiFi.status() != WL_CONNECTED) {

    bool wifiConnected = false;

    for (int i = 0; i < WIFI_COUNT; i++) {

      Serial.print("Trying WiFi: ");
      Serial.println(ssidList[i]);

      WiFi.begin(ssidList[i], passList[i]);

      unsigned long startTime = millis();

      while (WiFi.status() != WL_CONNECTED &&
             millis() - startTime < 10000) {

        delay(500);
        Serial.print(".");
      }

      if (WiFi.status() == WL_CONNECTED) {

        Serial.println("\nWiFi Connected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());

        wifiConnected = true;
        break;
      }

      Serial.println("\nFailed.");
    }

    if (!wifiConnected) {
      Serial.println("No WiFi available.");
      return;
    }
  }

  // ---------------------------
  // Connect Blynk
  // ---------------------------
  if (!Blynk.connected()) {

    Serial.println("Connecting to Blynk...");

    Blynk.config(BLYNK_AUTH_TOKEN);

    if (Blynk.connect(5000)) {

      Serial.println("Blynk Connected!");

    } else {

      Serial.println("Blynk Connection Failed");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize Soil Sensor
  pinMode(SOIL_PIN, INPUT);

  // LED setup
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);

  // Start Temperature Sensor
  ds18b20.begin();

  // ===== HX711 Setup =====
  scale.begin(HX711_DT, HX711_SCK);
  scale.set_scale(calibration_factor);

  if (scale.is_ready()) {

    Serial.println("HX711 Found");

    scale.tare();
    Serial.println("HX711 Ready");

  } else {

    Serial.println("HX711 NOT Found");
  }

  // ---------- Servo Setup ----------
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo3.setPeriodHertz(50);

  servo1.attach(SERVO1_PIN, 500, 2400);
  servo2.attach(SERVO2_PIN, 500, 2400);
  servo3.attach(SERVO3_PIN, 500, 2400);

  // Initial Servo Position
  servo1.write(0);
  servo2.write(0);
  servo3.write(0);

  // Motor pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  stopMotors();

  // Drill Motor Pins
  pinMode(DRILL_IN1, OUTPUT);
  pinMode(DRILL_IN2, OUTPUT);

  // Drill motor OFF
  digitalWrite(DRILL_IN1, LOW);
  digitalWrite(DRILL_IN2, LOW);

  // Connect to Blynk
  // Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass); 

  bool wifiConnected = false;

  for (int i = 0; i < WIFI_COUNT; i++) {

    Serial.print("Trying WiFi: ");
    Serial.println(ssidList[i]);

    WiFi.begin(ssidList[i], passList[i]);

    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED &&
          millis() - startAttemptTime < 10000) {

      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {

      Serial.println("\nConnected!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());

      wifiConnected = true;
      break;
    }

    Serial.println("\nFailed. Trying next WiFi...");
  }

  if (wifiConnected) {

    Blynk.config(BLYNK_AUTH_TOKEN);

    if (Blynk.connect(5000)) {
      Serial.println("Blynk Connected");
    } else {
      Serial.println("Blynk Failed");
    }

  } else {

    Serial.println("No WiFi Available");
    Serial.println("Running Offline Mode");
  }

  // Send Sensor Data Every 2 Seconds
  timer.setInterval(10000L, sendToBlynk);

  // Send Soil Moisture Every 10 Seconds
  timer.setInterval(10000L, sendSoilToBlynk);

}

void loop() {
  // Blynk.run();
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
  }
  timer.run();

  // Attempt reconnection every 10 seconds
  if (millis() - lastReconnectAttempt >= RECONNECT_INTERVAL) {

    lastReconnectAttempt = millis();

    reconnectBlynk();
  }
}