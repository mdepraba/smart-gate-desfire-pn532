#include "Connection.h"
#include "cert.h"

Connection* Connection::instancePtr = nullptr;

Connection::Connection(MqttConfig mqttConfig, Gate& gate)
  : mqttConfig(mqttConfig), gate(gate) {}

void Connection::begin() {
  instancePtr = this;
  WiFi.begin(mqttConfig.wifi_ssid, mqttConfig.wifi_password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\n Connected to WiFi");

  if (mqttConfig.port == 8883) {
    Serial.println("Configuring secure MQTT connection...");
    wifiClientTLS.setCACert(root_ca);
    client.setClient(wifiClientTLS);
  } else {
    Serial.println("Using plain MQTT connection...");
    client.setClient(wifiClient);
  }
  client.setServer(mqttConfig.server, mqttConfig.port);
  client.setCallback(Connection::mqttCallback);
  reconnect();
  publishStatus();
}

void Connection::reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqttConfig.username, mqttConfig.password)) {
      Serial.println("Connected to MQTT!");

      client.subscribe(mqttConfig.topics.control);
      Serial.println("Subscribed to /device/control");
      
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds");
      delay(5000);
    }
  }
}

void Connection::publishStatus() {
  JsonDocument root;
  JsonObject doc = root.to<JsonObject>();
  
  doc["online"] = true;
  doc["servo"] = gate.getGateState() == OPEN ? "open" : "closed";
  doc["auto_mode"] = gate.getMode() == AUTO ? "auto" : "manual";
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();

  long distance = gate.getDistance();
  doc["distance"] = distance;
  doc["threshold"] = gate.getThreshold();
  doc["timestamp"] = millis();

  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);

  client.publish(mqttConfig.topics.status, jsonBuffer);
  Serial.println("Status published to /device/status");
  Serial.println(jsonBuffer);
}

void Connection::loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

void Connection::mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("MQTT message received on topic: %c\n", topic);
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  // Logging
  Serial.printf("MQTT message received on topic: %s\n", topic);
  Serial.printf("Payload: %s\n", message.c_str());

  if (instancePtr) {
    instancePtr->onMessageReceived(String(topic), message);
  }
}

void Connection::setMessageHandler(void (*handler)(const String&, const String&)) {
  messageHandler = handler;
}

void Connection::onMessageReceived(const String& topic, const String& message) {
  Serial.printf("Topic: %s, Message: %s\n", topic.c_str(), message.c_str());
  if (messageHandler) {
    messageHandler(topic, message);
  }
}

void Connection::publishRFID(const String& pid) {
  JsonDocument root;
  JsonObject doc = root.to<JsonObject>();
  
  doc["pid"] = pid;
  doc["timestamp"] = millis();

  char jsonBuffer[100];
  serializeJson(doc, jsonBuffer);

  client.publish(mqttConfig.topics.rfid, jsonBuffer);
  Serial.println("PID published to /device/rfid");
  Serial.println(jsonBuffer);
}

