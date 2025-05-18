#include <WiFiManager.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

#define DHTPIN 4
#define DHTTYPE DHT22
#define MQ2_PIN 34
#define MQ5_PIN 35
#define LED_PIN 2
#define BUZZER_PIN 15
#define RELAY1 13
#define RELAY2 18
#define RELAY3 19
#define RELAY4 21

DHT dht(DHTPIN, DHTTYPE);
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
WebServer server(80);

const int relayPins[4] = { RELAY1, RELAY2, RELAY3, RELAY4 };
bool relays[4] = { false, false, false, false };

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, 17, 16);  // GPS: TX = 16, RX = 17

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  for (int i = 0; i < 4; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);  // Default to OFF
  }

  dht.begin();

  WiFiManager wm;
  // Static IP config
IPAddress staticIP(192, 168, 1, 27);     // ESP32 IP
IPAddress gateway(192, 168, 1, 1);       // Your router IP
IPAddress subnet(255, 255, 255, 0);      // Subnet mask

wm.setSTAStaticIPConfig(staticIP, gateway, subnet);  // Without DNS
  wm.autoConnect("PCB3_Config");

  server.on("/data", HTTP_GET, handleData);
  server.on("/control", HTTP_POST, handleControl);
  server.on("/control", HTTP_OPTIONS, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(204);
  });
  server.begin();
  Serial.println("Server started");
}

void loop() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }
  server.handleClient();
}

void handleData() {
  StaticJsonDocument<512> doc;

  doc["temp"] = dht.readTemperature();
  doc["hum"] = dht.readHumidity();
  doc["mq2"] = analogRead(MQ2_PIN);
  doc["mq5"] = analogRead(MQ5_PIN);

  if (gps.location.isValid()) {
    doc["lat"] = gps.location.lat();
    doc["lng"] = gps.location.lng();
  } else {
    doc["lat"] = 0;
    doc["lng"] = 0;
  }

  JsonArray relayArr = doc.createNestedArray("relays");
  for (int i = 0; i < 4; i++) {
    relayArr.add(digitalRead(relayPins[i]) == LOW ? 1 : 0);
  }

  String output;
  serializeJson(doc, output);
  Serial.println(output);  // Debug print
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", output);
}

void handleControl() {
  Serial.println(">>> handleControl triggered");
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  if (doc.containsKey("relay")) {
    Serial.println("Relay key detected");
    int r = doc["relay"];
    bool s = doc["state"];
    if (r >= 1 && r <= 4) {
      Serial.printf("Relay %d requested -> %s", r, s ? "ON" : "OFF");
      relays[r - 1] = s;
      digitalWrite(relayPins[r - 1], s ? LOW : HIGH);
    }
  }

  if (doc.containsKey("led") && doc["led"] == "pcb3") {
    Serial.printf("LED PCB3 -> %s", doc["state"] ? "ON" : "OFF");
    digitalWrite(LED_PIN, doc["state"] ? HIGH : LOW);
  }
  if (doc.containsKey("buzzer") && doc["buzzer"] == "pcb3") {
    Serial.printf("BUZZER PCB3 -> %s", doc["state"] ? "ON" : "OFF");
    digitalWrite(BUZZER_PIN, doc["state"] ? HIGH : LOW);
  }

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}
