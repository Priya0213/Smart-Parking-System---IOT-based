#define BLYNK_TEMPLATE_ID "TMPL357YO4TTi"
#define BLYNK_TEMPLATE_NAME "smart parking system"
#define BLYNK_AUTH_TOKEN "edpxQ54sDabn03xxf6pQSXoXmhQK30xj"
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>

#define WIFI_SSID "Devs"
#define WIFI_PASS "devanshu123"

// Parking configuration
const int TOTAL_SPACES = 5;
bool parkingSlots[TOTAL_SPACES] = {0}; // 0 = free, 1 = occupied

// Hardware setup
LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C address 0x27, 16x2 display
Servo entryServo;
Servo exitServo;

// Pin assignments
const int ENTRY_TRIG = 12;
const int ENTRY_ECHO = 13;
const int EXIT_TRIG = 14;
const int EXIT_ECHO = 27;
const int ENTRY_SERVO_PIN = 25;
const int EXIT_SERVO_PIN = 26;

// Servo control
const int SERVO_OPEN_ANGLE = 110;
const int SERVO_CLOSE_ANGLE = 0;
const int SERVO_SPEED = 15; // ms per degree
unsigned long lastServoUpdate = 0;
int entryCurrentAngle = 0;
int exitCurrentAngle = 0;
int entryTargetAngle = SERVO_CLOSE_ANGLE;
int exitTargetAngle = SERVO_CLOSE_ANGLE;

// Track when gates were fully opened
unsigned long entryGateOpenedTime = 0;
unsigned long exitGateOpenedTime = 0;

// Car detection
const int DETECTION_DISTANCE = 40; // cm
const int DETECTION_THRESHOLD = 3; // consecutive detections needed
int entryDetectionCount = 0;
int exitDetectionCount = 0;

BlynkTimer timer;

void setupServos() {
  ESP32PWM::allocateTimer(0);
  entryServo.setPeriodHertz(50); // Standard 50Hz servo
  exitServo.setPeriodHertz(50);
  entryServo.attach(ENTRY_SERVO_PIN);
  exitServo.attach(EXIT_SERVO_PIN);
}

void updateBlynk() {
  for (int i = 0; i < TOTAL_SPACES; i++) {
    Blynk.virtualWrite(i, parkingSlots[i]); // V0-V4
  }
  Blynk.virtualWrite(V5, TOTAL_SPACES - countOccupied()); // Free spaces
}

void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Free Space: " + String(TOTAL_SPACES - countOccupied()));
  lcd.setCursor(0, 1);
  lcd.print("Used Space: " + String(countOccupied()));
}

int countOccupied() {
  int count = 0;
  for (int i = 0; i < TOTAL_SPACES; i++) {
    if (parkingSlots[i]) count++;
  }
  return count;
}

int findFreeSlot() {
  for (int i = 0; i < TOTAL_SPACES; i++) {
    if (!parkingSlots[i]) return i;
  }
  return -1;
}

long getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  return pulseIn(echoPin, HIGH) * 0.034 / 2;
}

void controlServos() {
  if (millis() - lastServoUpdate >= SERVO_SPEED) {
    // Entry servo
    if (entryCurrentAngle != entryTargetAngle) {
      entryCurrentAngle += (entryTargetAngle > entryCurrentAngle) ? 1 : -1;
      entryServo.write(entryCurrentAngle);
    } else {
      // Check if gate just reached open position
      if (entryTargetAngle == SERVO_OPEN_ANGLE && entryGateOpenedTime == 0) {
        entryGateOpenedTime = millis();
      }
    }
   
    // Exit servo
    if (exitCurrentAngle != exitTargetAngle) {
      exitCurrentAngle += (exitTargetAngle > exitCurrentAngle) ? 1 : -1;
      exitServo.write(exitCurrentAngle);
    } else {
      if (exitTargetAngle == SERVO_OPEN_ANGLE && exitGateOpenedTime == 0) {
        exitGateOpenedTime = millis();
      }
    }
   
    lastServoUpdate = millis();
  }
}

void checkEntry() {
  static unsigned long entryTimer = 0;
  long distance = getDistance(ENTRY_TRIG, ENTRY_ECHO);

  if (distance < DETECTION_DISTANCE) {
    if (entryDetectionCount < DETECTION_THRESHOLD) {
      entryDetectionCount++;
    }
    entryTimer = millis();
  } else if (millis() - entryTimer > 1000) {
    entryDetectionCount = 0;
  }

  if (entryDetectionCount >= DETECTION_THRESHOLD &&
      entryTargetAngle == SERVO_CLOSE_ANGLE &&
      countOccupied() < TOTAL_SPACES) {
       
    int freeSlot = findFreeSlot();
    if (freeSlot != -1) {
      parkingSlots[freeSlot] = true;
      entryTargetAngle = SERVO_OPEN_ANGLE;
      updateBlynk();
      updateLCD();
      entryDetectionCount = 0;
    }
  }
}

void checkExit() {
  static unsigned long exitTimer = 0;
  long distance = getDistance(EXIT_TRIG, EXIT_ECHO);

  if (distance < DETECTION_DISTANCE) {
    if (exitDetectionCount < DETECTION_THRESHOLD) {
      exitDetectionCount++;
    }
    exitTimer = millis();
  } else if (millis() - exitTimer > 1000) {
    exitDetectionCount = 0;
  }

  if (exitDetectionCount >= DETECTION_THRESHOLD &&
      exitTargetAngle == SERVO_CLOSE_ANGLE &&
      countOccupied() > 0) {
       
    for (int i = TOTAL_SPACES-1; i >= 0; i--) {
      if (parkingSlots[i]) {
        parkingSlots[i] = false;
        exitTargetAngle = SERVO_OPEN_ANGLE;
        updateBlynk();
        updateLCD();
        exitDetectionCount = 0;
        break;
      }
    }
  }
}

void autoCloseGates() {
  // Close entry gate after 8 seconds
  if (entryGateOpenedTime != 0 && millis() - entryGateOpenedTime >= 5000) {
    entryTargetAngle = SERVO_CLOSE_ANGLE;
    entryGateOpenedTime = 0; // Reset timer
  }
 
  // Close exit gate after 8 seconds
  if (exitGateOpenedTime != 0 && millis() - exitGateOpenedTime >= 5000) {
    exitTargetAngle = SERVO_CLOSE_ANGLE;
    exitGateOpenedTime = 0; // Reset timer
  }
}

void setup() {
  // Hardware setup
  setupServos();
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Initializing");
  pinMode(ENTRY_TRIG, OUTPUT);
  pinMode(ENTRY_ECHO, INPUT);
  pinMode(EXIT_TRIG, OUTPUT);
  pinMode(EXIT_ECHO, INPUT);

  // Blynk connection
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);
 
  // Timer setup
  timer.setInterval(20L, controlServos);    // Smooth servo control
  timer.setInterval(100L, checkEntry);     // Entry sensor check
  timer.setInterval(100L, checkExit);      // Exit sensor check
  timer.setInterval(1000L, autoCloseGates);// Auto-close gates after 8 seconds
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Smart Parking  ");
  lcd.setCursor(0, 1);
  lcd.print("    System    ");
  entryServo.write(0);
  exitServo.write(0);
}

void loop() {
  Blynk.run();
  timer.run();
}