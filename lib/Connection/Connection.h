#ifndef CONNECTION_H
#define CONNECTION_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Gate.h>

struct MqttTopics {
    const char* status;
    const char* control;
    const char* rfid;
};

struct MqttConfig {
  const char* wifi_ssid;
  const char* wifi_password;
  const char* server;
  uint16_t port;
  const char* username;
  const char* password;
  MqttTopics topics;
};

class Connection {
public:
  Connection(MqttConfig mqttConfig, Gate& gate);
  void begin();
  void reconnect();
  void loop();
  void publishStatus();
  void publishRFID(const String& uid);

  void setMessageHandler(void (*handler)(const String&, const String&));

private:
    static Connection* instancePtr;
    static void mqttCallback(char* topic, byte* payload, unsigned int length);

    WiFiClient wifiClient;
    PubSubClient client;
    MqttConfig mqttConfig;
    Gate& gate;

    void onMessageReceived(const String& topic, const String& message);
    void (*messageHandler)(const String&, const String&) = nullptr;

};

#endif