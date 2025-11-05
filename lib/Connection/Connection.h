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


class Connection {
public:
  Connection(char* ssid, char* password, 
              char* mqtt_server, uint8_t mqtt_port, char* mqtt_user, char* mqtt_pass, 
              MqttTopics topic,
              Gate& gate);
  void begin();
  void reconnect();
  void loop();
  void publishStatus(uint16_t detectionThreshold, Gate& gate);

  void setMessageHandler(void (*handler)(const String&, const String&));

private:
    static Connection* instance;
    static void mqttCallback(char* topic, byte* payload, unsigned int length);

    WiFiClient wifiClient;
    PubSubClient client;
    MqttTopics topic;
    AutoMode autoMode;

    char* ssid;
    char* password;
    char* mqtt_server;
    uint8_t mqtt_port;
    char* mqtt_user;
    char* mqtt_pass;

    void onMessageReceived(const String& topic, const String& message);
    void (*messageHandler)(const String&, const String&) = nullptr;

};

#endif