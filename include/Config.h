#pragma once

// PN532 Pins
#define PN532_SS   5
#define PN532_RST  21
#define LED_PIN    2
#define USE_AES    true     // true = AES, false = 3DES

// Servo Pin
#define SERVO_PIN  32

// Ultrasonic Sensor Pins
#define TRIG_PIN   16
#define ECHO_PIN   17

// Wi-Fi Credentials
// const char* ssid = "frazerion";
// const char* password = "youshallnotpass";
const char* ssid = "sastra wifi";
const char* password = "password";

// MQTT Credentials
const char* mqtt_server = "48361d6ebd2a41fc9ea321dc602a27d2.s1.eu.hivemq.cloud";
const uint16_t mqtt_port = 8883;
const char* mqtt_user = "frazerion";
const char* mqtt_pass = "Frazmqtt00";

const char* topic_control = "/device/control";
const char* topic_status = "/device/status";
const char* topic_rfid = "/device/rfid";