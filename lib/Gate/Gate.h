#ifndef GATE_H
#define GATE_H

#include <Arduino.h>
#include <ESP32Servo.h>

enum ObjectState {
    NONE,
    PRESENT,
    PASSED
};

enum GateState {
    CLOSED,
    OPEN
};

enum AutoMode {
    AUTO,
    MANUAL
};

class Gate {
public:
    Gate(uint8_t trigPin, uint8_t echoPin, uint8_t servoPin);
    void begin();
    uint16_t getDistance();
    bool isObjectPassed(uint16_t threshold, uint16_t distance);
    GateState commandGate(GateState state);
    void disableUltrasonic();
    void enableUltrasonic();
    void setMode(AutoMode mode);

    GateState getGateState() const { return m_gateState; }
    ObjectState getObjectState() const { return m_state; }
    AutoMode getMode() const { return m_autoMode; }

private:
    uint8_t m_trigPin;
    uint8_t m_echoPin;
    uint8_t m_servoPin;

    unsigned long m_lastMeasurementTime;
    unsigned long m_lastStateChange;
    const unsigned long m_measurementInterval = 100;
    const unsigned long m_resetDelay = 1000;

    ObjectState m_state;
    GateState m_gateState;
    AutoMode m_autoMode;
    Servo servoMotor;
};

#endif