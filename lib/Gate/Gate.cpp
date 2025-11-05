#include "Gate.h"

Gate::Gate(uint8_t trigPin, uint8_t echoPin, uint8_t servoPin)
    : m_trigPin(trigPin), m_echoPin(echoPin), m_servoPin(servoPin),
      m_lastMeasurementTime(0), m_lastStateChange(0),
      m_state(NONE), m_autoMode(AUTO) {}
      
void Gate::begin() {
  servoMotor.attach(m_servoPin);
  pinMode(m_trigPin, OUTPUT);
  pinMode(m_echoPin, INPUT);

  servoMotor.write(0);
}

uint16_t Gate::getDistance() {
  if(m_autoMode == MANUAL) {
    return 0; 
  }

  digitalWrite(m_trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(m_trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(m_trigPin, LOW);

  uint32_t duration = pulseIn(m_echoPin, HIGH);
  uint16_t distance = (duration * 0.0343) / 2;

  if (duration == 0) {
    Serial.println("Ultrasonic sensor error");
    return 0;   // error reading
  }

  return distance;
}

bool Gate::isObjectPassed(uint16_t threshold, uint16_t distance) {
  uint32_t currentTime = millis();
  if (currentTime - m_lastMeasurementTime < m_measurementInterval) return false;
  m_lastMeasurementTime = currentTime;

  switch (m_state) {
    case NONE:
      if (distance <= threshold) {
        m_state = PRESENT;
        m_lastStateChange = currentTime;
        Serial.println("Object Detected");
      }
      break;

    case PRESENT:
      if (distance > threshold) {
        m_state = PASSED;
        m_lastStateChange = currentTime;
        Serial.println("Object Passed");
        return true; // Object has passed
      }
      break;

    case PASSED:
      if (currentTime - m_lastStateChange > m_resetDelay) {
        m_state = NONE; // Reset state after delay
      }
      break;
  }

  return false;

}


GateState Gate::commandGate(GateState desiredState) {
  if (desiredState == OPEN && m_gateState == CLOSED) {
    servoMotor.write(90); // Open gate
    m_gateState = OPEN;
    Serial.println("Gate Opened");
  } else if (desiredState == CLOSED && m_gateState == OPEN) {
    servoMotor.write(0); // Close gate
    m_gateState = CLOSED;
    Serial.println("Gate Closed");
  }
  return m_gateState; // No state change
}


void Gate::disableUltrasonic() {
  if(m_autoMode == MANUAL) return;
  m_autoMode = MANUAL;
  digitalWrite(m_trigPin, LOW);
  pinMode(m_trigPin, INPUT);
  Serial.println("Ultrasonic Sensor Disabled (Manual Mode)");
}

void Gate::enableUltrasonic() {
  if(m_autoMode == AUTO) return;
  m_autoMode = AUTO;
  pinMode(m_trigPin, OUTPUT);
  digitalWrite(m_trigPin, LOW);
  Serial.println("Ultrasonic Sensor Enabled (Auto Mode)");
}

void Gate::setMode(AutoMode mode) {
  m_autoMode = mode;
  if (mode == AUTO) enableUltrasonic();
  else disableUltrasonic();
}
