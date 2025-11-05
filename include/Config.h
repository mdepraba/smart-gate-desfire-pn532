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
const char* ssid = "LIT-2.4G-CSSU";
const char* password = "yakaligaarka2.4G";

// MQTT Credentials
const char* mqtt_server = "192.168.18.5";
const uint16_t mqtt_port = 1883;
const char* mqtt_user = "arkalit";
const char* mqtt_pass = "arkamqtt";

const char* topic_control = "/device/control";
const char* topic_status = "/device/status";
const char* topic_rfid = "/device/rfid";