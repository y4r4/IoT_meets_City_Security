#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

#define PIR_PIN     13  // GPIO13
#define LED_PIN     12  // GPIO12
#define BUZZER_PIN  14  // GPIO14

ESP8266WebServer server(80);
bool motionDetected = false;
bool motionTriggered = false;

void IRAM_ATTR handlePirInterrupt() {
  motionDetected = true;
}

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  attachInterrupt(digitalPinToInterrupt(PIR_PIN), handlePirInterrupt, RISING);

  // Static IP config
  IPAddress staticIP(192, 168, 1, 100);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFiManager wm;
  wm.setSTAStaticIPConfig(staticIP, gateway, subnet);
  wm.autoConnect("PCB1_Config");

  // API routes
  server.on("/motion", HTTP_GET, []() {
    StaticJsonDocument<100> doc;
    doc["motion"] = motionDetected;
    String output;
    serializeJson(doc, output);

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", output);

    motionDetected = false; // Reset after read
  });

  server.on("/control", HTTP_POST, []() {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    if (doc.containsKey("led") && doc["led"] == "pcb1") {
      digitalWrite(LED_PIN, doc["state"] ? HIGH : LOW);
    }

    if (doc.containsKey("buzzer") && doc["buzzer"] == "pcb1") {
      digitalWrite(BUZZER_PIN, doc["state"] ? HIGH : LOW);
    }

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  });

  server.on("/control", HTTP_OPTIONS, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(204);
  });

  server.begin();
  Serial.println("PCB1 ready at: http://" + WiFi.localIP().toString());
}

void loop() {
  server.handleClient();

  // Motion detected logic
  if (motionDetected && !motionTriggered) {
    motionTriggered = true;
    Serial.println("🚨 Motion Detected - Beep & LED ON");

    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);  // 2 seconds
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    delay(500);  // 2 seconds
     digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);  // 2 seconds
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }

  // Reset after motion stops
  if (digitalRead(PIR_PIN) == LOW && motionTriggered) {
    motionTriggered = false;
    motionDetected = false;
  }
}
