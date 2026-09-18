#include <Arduino.h>
#include <ESP32Servo.h>

constexpr int PIN_TRIG = 5;
constexpr int PIN_ECHO = 18;
constexpr int PIN_SERVO = 13;

constexpr float DISTANCE_THRESHOLD_CM = 20.0f;
constexpr unsigned long HOLD_OPEN_MS = 3000;
constexpr unsigned long SENSOR_INTERVAL_MS = 60;

constexpr int ANGLE_CLOSED = 0;
constexpr int ANGLE_OPEN = 90;
constexpr int STEP_DELAY_MS = 15;

enum class BinState {
  CLOSED,
  OPENING,
  OPEN,
  CLOSING
};

Servo binServo;
BinState currentState = BinState::CLOSED;

int currentAngle = ANGLE_CLOSED;
unsigned long lastSensorReadTime = 0;
unsigned long lastServoStepTime = 0;
unsigned long lastHandSeenTime = 0;
float currentDistanceCm = -1.0f;

float measureDistance() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  unsigned long duration = pulseIn(PIN_ECHO, HIGH, 30000);

  if (duration == 0) {
    return -1.0f;
  }

  return (duration * 0.0343f) / 2.0f;
}

void stepServoTowards(int targetAngle, BinState nextStateWhenDone) {
  unsigned long now = millis();
  if (now - lastServoStepTime >= STEP_DELAY_MS) {
    lastServoStepTime = now;
    if (currentAngle < targetAngle) {
      currentAngle++;
      binServo.write(currentAngle);
    } else if (currentAngle > targetAngle) {
      currentAngle--;
      binServo.write(currentAngle);
    }
    if (currentAngle == targetAngle) {
      currentState = nextStateWhenDone;
    }
  }
}

void updateStateMachine() {
  unsigned long now = millis();
  bool handDetected = (currentDistanceCm > 0.0f && currentDistanceCm <= DISTANCE_THRESHOLD_CM);

  switch (currentState) {
  case BinState::CLOSED:
    if (handDetected) {
      Serial.printf("[EVENT] Object detected at %.1f cm! Opening lid...\n", currentDistanceCm);
      currentState = BinState::OPENING;
    }
    break;

  case BinState::OPENING:
    stepServoTowards(ANGLE_OPEN, BinState::OPEN);
    if (currentState == BinState::OPEN) {
      lastHandSeenTime = now;
      Serial.println("[STATE] Lid fully OPEN.");
    }
    break;

  case BinState::OPEN:
    if (handDetected) {
      lastHandSeenTime = now;
    } else if (now - lastHandSeenTime >= HOLD_OPEN_MS) {
      Serial.println("[EVENT] Hold timer expired. Closing lid...");
      currentState = BinState::CLOSING;
    }
    break;

  case BinState::CLOSING:
    if (handDetected) {
      Serial.printf("[SAFETY] Hand detected at %.1f cm while closing! Reopening...\n", currentDistanceCm);
      currentState = BinState::OPENING;
      break;
    }
    stepServoTowards(ANGLE_CLOSED, BinState::CLOSED);
    if (currentState == BinState::CLOSED) {
      Serial.println("[STATE] Lid fully CLOSED.");
    }
    break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  binServo.setPeriodHertz(50);
  binServo.attach(PIN_SERVO, 500, 2400);

  currentAngle = ANGLE_CLOSED;
  binServo.write(currentAngle);

  Serial.printf("[SETUP] Servo attached to GPIO %d, set to %d deg\n", PIN_SERVO, ANGLE_CLOSED);
  Serial.printf("[SETUP] Ultrasonic TRIG: GPIO %d, ECHO: GPIO %d\n", PIN_TRIG, PIN_ECHO);
  Serial.printf("[SETUP] Threshold: %.1f cm, Hold Open: %lu ms\n", DISTANCE_THRESHOLD_CM, HOLD_OPEN_MS);
  Serial.println("[SETUP] Ready!\n");
}

void loop() {
  unsigned long now = millis();

  if (now - lastSensorReadTime >= SENSOR_INTERVAL_MS) {
    lastSensorReadTime = now;
    currentDistanceCm = measureDistance();
  }

  updateStateMachine();
}