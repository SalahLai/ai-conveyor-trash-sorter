#include <Servo.h>

// Pin definitions for stepper motor (Nema 17 with A4988)
const int stepPin = 9;    // STEP pin for pulsing
const int dirPin = 10;    // DIR pin for direction
const int enablePin = 11; // ENABLE pin to enable/disable driver

// Pin definitions for servos
const int servoPin1 = 5;  // Servo 1 (metal vs. others)
const int servoPin2 = 6;  // Servo 2 (paper vs. plastic)

// Pin definitions for ultrasonic sensor
const int trigPin = 7;    // Trigger pin
const int echoPin = 8;    // Echo pin

// Servo objects
Servo servo1;
Servo servo2;


// Global variables to track current servo positions
int currentServo1Pos = 90; // Initial neutral position
int currentServo2Pos = 90; // Initial neutral position

// Stepper motor parameters
const int stepsPerRevolution = 200; // Nema 17: 1.8°/step (full-step)
const int stepDelay = 500; // Delay between steps (us), ~300 RPM
// Step delay range: 500us (~600 RPM) to 5000us (~30 RPM)

// Ultrasonic sensor parameters
const float maxDistance = 10.0; // Max distance to detect trash (cm)

// Classification averaging parameters
const unsigned long sampleDuration = 1500; // Time to collect predictions (ms)

// Clearance delay to allow trash to move past sensor
const unsigned long clearanceDelay = 1500; // Delay after servo actions (ms)
const unsigned long 3clearanceDelay = 5000;
void setup() {
  // Initialize serial communication for Python AI input
  Serial.begin(115200);
  
  // Set stepper motor control pins as outputs
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(enablePin, OUTPUT);
  
  // Set ultrasonic sensor pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  // Attach servos to their pins
  servo1.attach(servoPin1);
  servo2.attach(servoPin2);
  
  // Set servos to initial neutral position
  servo1.write(currentServo1Pos);
  servo2.write(currentServo2Pos);
  
  // Enable stepper driver and start conveyor (forward)
  digitalWrite(enablePin, LOW); // Enable driver (LOW active)
  stepMotor(1); // Continuous forward
}

// Function to control stepper motor
// direction: 0 for stop, 1 for forward, -1 for backward
void stepMotor(int direction) {
  if (direction == 0) {
    // Stop: Disable driver
    digitalWrite(enablePin, HIGH);
    return;
  }
  
  // Enable driver
  digitalWrite(enablePin, LOW);
  
  // Set direction
  digitalWrite(dirPin, (direction == 1) ? HIGH : LOW);
  
  // Pulse STEP pin to move one step
  digitalWrite(stepPin, HIGH);
  delayMicroseconds(10); // Short pulse
  digitalWrite(stepPin, LOW);
  delayMicroseconds(stepDelay); // Control speed
}

// Function to set servo position without reset
// degree: 0 to 180 (target angle)
// Updates global position if changed
void setServo(Servo &servo, int degree, int &currentPos) {
  degree = constrain(degree, 0, 180);
  if (degree != currentPos) {
    servo.write(degree);
    currentPos = degree;
    delay(500); // Brief delay to ensure servo reaches position
  }
}

// Function to measure distance using ultrasonic sensor (in cm)
float getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH, 30000); // Timeout after 30ms
  if (duration == 0) return maxDistance + 1; // No echo, assume far
  float distance = duration * 0.0343 / 2; // Speed of sound = 0.0343 cm/us
  return distance;
}

// Function to read and average classifications from Python AI via serial
// Returns most frequent class_index (0, 1, 2) or -1 for Unknown
int getAveragedClassification() {
  int counts[3] = {0, 0, 0}; // Counts for metal (0), paper (1), plastic (2)
  unsigned long startTime = millis();
  
  // Clear serial buffer to use only fresh predictions
  while (Serial.available() > 0) {
    Serial.read();
  }
  
  // Collect data for sampleDuration
  while (millis() - startTime < sampleDuration) {
    if (Serial.available() > 0) {
      char incomingByte = Serial.read(); // Read single byte from Python AI
      int classIndex = incomingByte - '0'; // Convert ASCII to integer
      if (classIndex >= 0 && classIndex <= 2) {
        counts[classIndex]++;
      }
    }
    delay(10); // Small delay to avoid overwhelming serial buffer
  }
  
  // Find the most frequent class
  int maxCount = 0;
  int bestClass = -1;
  for (int i = 0; i < 3; i++) {
    if (counts[i] > maxCount) {
      maxCount = counts[i];
      bestClass = i;
    }
  }
  
  // Return -1 if no valid classifications were received (Unknown)
  return (maxCount > 0) ? bestClass : -1;
}

void loop() {
  // Clear serial buffer to ignore AI predictions until detection
  while (Serial.available() > 0) {
    Serial.read();
  }
  
  // Check distance with ultrasonic sensor
  float distance = getDistance();
  
  // Default state: Run conveyor
  if (distance >= maxDistance || distance <= 0) {
    stepMotor(1); // Keep conveyor running forward
    return;
  }
  
  // Trash detected: Stop conveyor immediately
  stepMotor(0); // Stop motor
  
  // Get averaged classification from Python AI
  int classIndex = getAveragedClassification();
  
  // Servo states based on classification
  switch (classIndex) {
    case 0: // Metal
      setServo(servo1, 90, currentServo1Pos); // Servo 2 neutralAllTopic:All
      setServo(servo2, 90, currentServo2Pos); // Servo 1 to 45°
      break;
    case 1: // Paper
      setServo(servo1, 0, currentServo1Pos); // Servo 1 to 135°
      setServo(servo2, 45, currentServo2Pos); // Servo 2 to 45°
      break;
    case 2: // Plastic
      setServo(servo1,130, currentServo1Pos); // Servo 1 to 135°
      setServo(servo2,180, currentServo2Pos); // Servo 2 to 135°
      break;
    case -1: // Unknown
      setServo(servo1, 90, currentServo1Pos); // Servo 1 neutral
      setServo(servo2, 90, currentServo2Pos); // Servo 2 neutral
      break;
  }
  
  // Pause detection and keep motor stopped to allow trash to clear
  stepMotor(0); // Ensure motor remains stopped
  delay(clearanceDelay); // Wait for clearance (2 seconds)
  
  // Resume conveyor after clearance
  stepMotor(1); // Restart forward
  delay(clearanceDelay);
}