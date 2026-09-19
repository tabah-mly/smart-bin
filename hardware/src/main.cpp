#include <Arduino.h>
#include <ESP32Servo.h>

#define TRIG_PIN 5
#define ECHO_PIN 18
#define SERVO_PIN 13

Servo servo;

bool isOpen = false;
unsigned long leaveTime = 0;

float getDistance() {
  // Send trigger pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Read echo duration
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) {
    return -1;
  }

  // Convert to centimeters
  return duration * 0.0343 / 2;
}

void setup() {
  Serial.begin(115200);

  servo.attach(SERVO_PIN);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  servo.write(0);

  Serial.println("Lid: CLOSED");
}

void loop() {
  float distance = getDistance();

  // Distance debugging
  if (distance < 0) {
    Serial.println("Distance: OUT OF RANGE");
  } else {
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");
  }

  // Person detected
  if (distance >= 0 && distance <= 10) {
    if (!isOpen) {
      servo.write(90);
      isOpen = true;
      Serial.println("Lid: OPEN");
    }
    // Reset closing timer
    leaveTime = 0;
  }

  // Person no longer detected
  else if (isOpen) {
    // Start closing timer
    if (leaveTime == 0) {
      leaveTime = millis();
      Serial.println("Person left - closing timer started");
    }

    // Close after 3 seconds
    if (millis() - leaveTime >= 3000) {
      servo.write(0);
      isOpen = false;
      leaveTime = 0;
      Serial.println("Lid: CLOSED");
    }
  }

  delay(200);
}