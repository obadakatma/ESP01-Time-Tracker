#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* scriptURL = "https://script.google.com/macros/s/YOUR_SCRIPT_ID_HERE/exec";


#define BUTTON_PIN 0
#define LED_PIN    2

ESP8266WebServer server(80);

bool ledState = LOW;
bool lastButtonReading = HIGH;
bool debouncedButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

unsigned long recordStartMillis = 0;
bool isRecording = false;

void setupTime() {
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Syncing time");
  time_t now = time(nullptr);
  while (now < 100000) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println();
  Serial.println("Time synced");
}

String getTodayDateString() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  return String(t->tm_mon + 1) + "/" + String(t->tm_mday) + "/" + String((t->tm_year + 1900) % 100);
}

void sendHoursToSheet(double hours) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  String url = String(scriptURL) + "?date=" + getTodayDateString() + "&hours=" + String(hours, 4);

  Serial.print("Sending to Sheet: ");
  Serial.println(url);

  if (https.begin(client, url)) {
    int httpCode = https.GET();
    if (httpCode > 0) {
      Serial.print("Response: ");
      Serial.println(https.getString());
    } else {
      Serial.print("HTTP request failed: ");
      Serial.println(https.errorToString(httpCode));
    }
    https.end();
  }
}

void updateRecordingFromLedState() {
  if (ledState == HIGH) {
    recordStartMillis = millis();
    isRecording = true;
    Serial.println("Recording started");
  } else {
    if (isRecording) {
      unsigned long finishedDurationMs = millis() - recordStartMillis;
      double hours = finishedDurationMs / 3600000.0;
      Serial.print("Recording stopped, hours: ");
      Serial.println(hours, 4);
      sendHoursToSheet(hours);
    }
    isRecording = false;
    Serial.println("Recording stopped and reset");
  }
}

void handleRoot() {
  unsigned long displayDurationMs = isRecording ? (millis() - recordStartMillis) : 0;
  double hours = displayDurationMs / 3600000.0;
  String statusText = isRecording ? "RECORDING" : "STOPPED";
  String statusColor = isRecording ? "#2ecc71" : "#e74c3c";

  String html = "<!DOCTYPE html><html><head><title>ESP-01 Timer</title>";
  html += "<meta http-equiv='refresh' content='2'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;background:#1e1e2f;color:#fff;text-align:center;padding:30px;margin:0;}";
  html += "h1{font-size:24px;margin-bottom:10px;}";
  html += ".status{display:inline-block;padding:10px 25px;border-radius:8px;font-size:20px;font-weight:bold;background:" + statusColor + ";margin-bottom:20px;}";
  html += ".duration{font-size:36px;font-weight:bold;margin:15px 0;}";
  html += ".label{font-size:14px;color:#aaa;text-transform:uppercase;letter-spacing:1px;}";
  html += ".btn-row{margin-top:30px;display:flex;justify-content:center;gap:20px;}";
  html += ".btn{width:140px;height:70px;font-size:18px;font-weight:bold;color:#fff;border:none;border-radius:12px;cursor:pointer;box-shadow:0 4px 6px rgba(0,0,0,0.3);}";
  html += ".btn:active{transform:translateY(2px);}";
  html += ".start-btn{background:#2ecc71;}";
  html += ".stop-btn{background:#e74c3c;}";
  html += "</style></head><body>";

  html += "<h1>ESP-01 Recording Timer</h1>";
  html += "<div class='status'>" + statusText + "</div>";
  html += "<div class='label'>Duration</div>";
  html += "<div class='duration'>" + String(hours, 4) + " hrs</div>";
  

  html += "<div class='btn-row'>";
  html += "<form action='/start' method='GET' style='margin:0;'>";
  html += "<button class='btn start-btn' type='submit'>START</button></form>";
  html += "<form action='/stop' method='GET' style='margin:0;'>";
  html += "<button class='btn stop-btn' type='submit'>STOP</button></form>";
  html += "</div>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleStart() {
  ledState = HIGH;
  digitalWrite(LED_PIN, ledState);
  updateRecordingFromLedState();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleStop() {
  ledState = LOW;
  digitalWrite(LED_PIN, ledState);
  updateRecordingFromLedState();
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, ledState);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  setupTime();

  server.on("/", handleRoot);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.begin();
  Serial.println("HTTP server started");

  updateRecordingFromLedState();
}

void loop() {
  server.handleClient();

  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != debouncedButtonState) {
      debouncedButtonState = reading;
      if (debouncedButtonState == LOW) {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
        Serial.println(ledState ? "LED1 ON (GPIO2 HIGH)" : "LED2 ON (GPIO2 LOW)");
        updateRecordingFromLedState();
      }
    }
  }

  lastButtonReading = reading;
}
