#include "Connection.h"

Connection::Connection(char* ssid, char* password, char* mqtt_server, uint8_t mqtt_port, char* mqtt_user, char* mqtt_pass, MqttTopics topic, Gate& gate)
  : ssid(ssid), password(password), mqtt_server(mqtt_server), mqtt_port(mqtt_port), mqtt_user(mqtt_user), mqtt_pass(mqtt_pass), client(wifiClient) {}

void Connection::begin() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  client.setServer(mqtt_server, mqtt_port);
  reconnect();
}

void Connection::reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("Connected to MQTT!");
      
      client.subscribe(topic.control);
      Serial.println("Subscribed to /device/control");
      
      // publishStatus();
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds");
      delay(5000);
    }
  }
}

void Connection::publishStatus(uint16_t detectionThreshold, Gate& gate) {
  JsonDocument root;
  JsonObject doc = root.to<JsonObject>();
  
  doc["online"] = true;
  doc["servo"] = gate.getGateState() == OPEN ? "open" : "closed";
  doc["auto_mode"] = autoMode;
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();

  long distance = gate.getDistance();
  doc["distance"] = distance;
  doc["threshold"] = detectionThreshold;
  doc["timestamp"] = millis();

  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);

  client.publish(topic.status, jsonBuffer);
  Serial.println("Status published to /device/status");
  Serial.println(jsonBuffer);
}

void Connection::loop() {
  client.loop();
}

void Connection::mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("MQTT message received on topic: %c\n", topic);
  if (instance) {
    String message;
    for (unsigned int i = 0; i < length; i++) {
      message += (char)payload[i];
    }
    instance->onMessageReceived(String(topic), message);
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

