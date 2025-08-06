
#include <WiFi.h>
#include <Wire.h>
#include <ESPAsyncWebServer.h>
#include <ESP_Mail_Client.h>
#include <HTTPClient.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <time.h>

// Added for GPS parsing
HardwareSerial GPS(1); // Use UART1 for GPS

// WiFi credentials
#define WIFI_SSID       "abc"
#define WIFI_PASSWORD   "abc"

// Email and SMS
#define SMTP_HOST       "smtp.gmail.com"
#define SMTP_PORT       587
#define AUTHOR_EMAIL    "abc"
#define AUTHOR_PASSWORD "xyz"
#define RECIPIENT_EMAIL "xyz"

#define TWILIO_SID      "xyz"
#define TWILIO_TOKEN    "xyz"
#define TWILIO_FROM     "xyz"
#define ALERT_MOBILE    "xyz"

// Sensor
#define MPU_ADDR        0x68
#define BUZZER_PIN      25
#define LED_PIN         26

const char* http_username = "xyz";
const char* http_password = "xyz";

MAX30105 particleSensor;
AsyncWebServer server(80);

// GPS Data
String latitude = "--";
String longitude = "--";

float BPM = 0, SpO2 = 0;
float Ax, Ay, Az, totalAcc;
bool fallDetected = false;
unsigned long lastFallTime = 0;
String lastFallTimeStr = "Never";
uint32_t tsLastReport = 0;
SMTPSession smtp;
String fallHistory = "";

unsigned long lastBeat = 0;
float lastBPM = 0;

// --- Helper: Basic HTTP Authentication ---
bool isAuthenticated(AsyncWebServerRequest *request) {
  if (!request->authenticate(http_username, http_password)) {
    request->requestAuthentication();
    return false;
  }
  return true;
}

// --- Helper: Get Current Date/Time String (IST) ---
String getCurrentTimeString() {
  time_t now = time(nullptr);
  struct tm *t = localtime(&now);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
  return String(buf);
}

// --- Email Alert ---
void sendEmail(String lat, String lon, String fallTime) {
  ESP_Mail_Session session;
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "";

  SMTP_Message message;
  message.sender.name = "Smart Harness Alert";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "🚨 Fall Detected on Smart Harness";
  message.addRecipient("User", RECIPIENT_EMAIL);

  String htmlMsg = "<h2>🚨 Fall Detected! employee name: Mayank Pandey</h2>"
                   "<p><b>Date/Time:</b> " + fallTime + "</p>"
                   "<p><b>Location:</b><br>Latitude: " + lat + "<br>Longitude: " + lon + "</p>";
  message.html.content = htmlMsg.c_str();
  message.html.content_type = "text/html";
  message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  if (!smtp.connect(&session)) {
    Serial.println("SMTP connection failed: " + smtp.errorReason());
    return;
  }
  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Error sending Email: " + smtp.errorReason());
  } else {
    Serial.println("✅ Email sent successfully!");
  }
  smtp.closeSession();
}

// --- SMS Alert ---
void sendSMS(String messageText, String fallTime) {
  HTTPClient http;
  http.begin("https://api.twilio.com/2010-04-01/Accounts/" + String(TWILIO_SID) + "/Messages.json");
  http.setAuthorization(TWILIO_SID, TWILIO_TOKEN);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body = "From=" + String(TWILIO_FROM) + "&To=" + String(ALERT_MOBILE) + "&Body=" +
                "FALL DETECTED employee name: Mayank Pandey\nDate/Time: " + fallTime + "\n" + messageText;
  int code = http.POST(body);
  Serial.println("SMS sent, response code: " + String(code));
  http.end();
}

// --- Data Processor for AJAX ---
String getSensorJSON() {
  String fallData = "[";
  int idx = 0;
  while (idx < fallHistory.length()) {
    int next = fallHistory.indexOf(',', idx);
    String t = (next == -1) ? fallHistory.substring(idx) : fallHistory.substring(idx, next);
    if (t.length() > 0) fallData += "{x:" + t + ",y:1},";
    if (next == -1) break;
    idx = next + 1;
  }
  if (fallData.endsWith(",")) fallData.remove(fallData.length()-1);
  fallData += "]";

  String bpmStr = (BPM > 0) ? String(BPM, 1) : "\"--\"";
  String spo2Str = (SpO2 > 0) ? String(SpO2, 1) : "\"--\"";

  String json = "{";
  json += "\"BPM\":" + bpmStr + ",";
  json += "\"SPO2\":" + spo2Str + ",";
  json += "\"AX\":" + String(Ax, 2) + ",";
  json += "\"AY\":" + String(Ay, 2) + ",";
  json += "\"AZ\":" + String(Az, 2) + ",";
  json += "\"TOTALACC\":" + String(totalAcc, 2) + ",";
  json += "\"FALL\":" + String(fallDetected ? "true" : "false") + ",";
  json += "\"LASTFALL\":\"" + lastFallTimeStr + "\",";
  json += "\"LAT\":\"" + latitude + "\",";
  json += "\"LON\":\"" + longitude + "\",";
  json += "\"FALL_HISTORY\":" + fallData;
  json += "}";
  return json;
}

// --- GPS Decimal Conversion ---
double convertToDecimal(double raw, char dir) {
  int degrees = int(raw / 100);
  double minutes = raw - (degrees * 100);
  double decimal = degrees + (minutes / 60.0);
  if (dir == 'S' || dir == 'W') decimal *= -1;
  return decimal;
}

// --- GPS GPGGA Sentence Parser ---
void parseGPGGA(String sentence) {
  int commas[15], idx = 0;
  for (int i = 0; i < sentence.length(); i++) {
    if (sentence.charAt(i) == ',') commas[idx++] = i;
  }
  if (idx >= 5) {
    String latStr = sentence.substring(commas[1] + 1, commas[2]);
    String latDir = sentence.substring(commas[2] + 1, commas[3]);
    String lonStr = sentence.substring(commas[3] + 1, commas[4]);
    String lonDir = sentence.substring(commas[4] + 1, commas[5]);

    if (latStr.length() == 0 || lonStr.length() == 0) return; // avoid empty update

    double lat = latStr.toDouble();
    double lon = lonStr.toDouble();

    latitude = String(convertToDecimal(lat, latDir.charAt(0)), 6);
    longitude = String(convertToDecimal(lon, lonDir.charAt(0)), 6);
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  // Setup GPS on UART1: RX=16, TX=17 (GPS TX-> 16 ESP32 RX)
  GPS.begin(9600, SERIAL_8N1, 16, 17);

  configTime(19800, 0, "pool.ntp.org", "time.nist.gov");

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi Connected. IP: " + WiFi.localIP().toString());

  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("MAX30102 not found.");
    while (1);
  }
  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x2F);
  particleSensor.setPulseAmplitudeIR(0x2F);

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  // Original server routes remained intact
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!isAuthenticated(request)) return;
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Smart Harness</title>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<link rel='stylesheet' href='https://fonts.googleapis.com/css?family=Segoe+UI:400,700'>";
    html += "<style>";
    html += "body{font-family:'Segoe UI',Arial,sans-serif;background:#f6f8fa;margin:0;padding:0;}";
    html += ".dashboard{max-width:600px;margin:24px auto;background:#fff;border-radius:12px;box-shadow:0 2px 8px rgba(0,0,0,0.1);padding:16px 8px;}";
    html += "h2{color:#2d6cdf;margin-top:0;}";
    html += ".card{background:#f0f4fa;border-radius:8px;padding:12px;margin-bottom:12px;}";
    html += ".alert{color:#fff;background:#e74c3c;padding:8px 12px;border-radius:4px;}";
    html += ".ok{color:#27ae60;}";
    html += "iframe{border-radius:8px;width:100%;}";
    html += "@media (max-width:700px){.dashboard{max-width:98vw;padding:6vw 2vw;}.card{font-size:1em;}}";
    html += "</style>";
    html += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script></head><body>";
    html += "<div class='dashboard'>";
    html += "<h2>Smart Harness Dashboard</h2>";
    html += "<div class='card'><strong>BPM:</strong> <span id='bpm'>--</span> &nbsp; <strong>SpO2:</strong> <span id='spo2'>--</span></div>";
    html += "<div class='card'><span id='fallStatus'></span><span id='lastFall'></span></div>";
    html += "<div class='card'><strong>Location:</strong> Lat: <span id='lat'>--</span>, Lon: <span id='lon'>--</span></div>";
    html += "<iframe id='map' height='180'></iframe>";
    html += "</div>";
    html += "<script>";
    html += "function updateMap(lat,lon){";
    html += "if(lat!='--'&&lon!='--'){";
    html += "document.getElementById('map').src='https://maps.google.com/maps?q='+lat+','+lon+'&z=15&output=embed';";
    html += "}else{";
    html += "document.getElementById('map').src='';";
    html += "}";
    html += "}";
    html += "function fetchData(){";
    html += "fetch('/data').then(r=>r.json()).then(d=>{";
    html += "document.getElementById('bpm').textContent=d.BPM;";
    html += "document.getElementById('spo2').textContent=d.SPO2;";
    html += "document.getElementById('lat').textContent=d.LAT;";
    html += "document.getElementById('lon').textContent=d.LON;";
    html += "updateMap(d.LAT,d.LON);";
    html += "if(d.FALL){";
    html += "document.getElementById('fallStatus').innerHTML='<div class=\"alert\">Fall Detected!<br><small>Time: '+d.LASTFALL+'</small></div>';";
    html += "document.getElementById('lastFall').textContent = '';";
    html += "}else{";
    html += "document.getElementById('fallStatus').innerHTML='<div class=\"ok\">No Fall</div>';";
    html += "document.getElementById('lastFall').textContent = 'Last Fall: ' + d.LASTFALL;";
    html += "}";
    html += "}).catch(()=>{document.getElementById('fallStatus').innerHTML='<div class=\"alert\">FALL DETECTED!</div>';});";
    html += "}";
    html += "window.onload=function(){fetchData();setInterval(fetchData,5000);};";
    html += "</script>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!isAuthenticated(request)) return;
    request->send(200, "application/json", getSensorJSON());
  });

  server.begin();
}

void loop() {
  // GPS reading: read lines and parse $GPGGA sentences
  while (GPS.available()) {
    String line = GPS.readStringUntil('\n');
    line.trim();
    if (line.startsWith("$GPGGA")) {
      parseGPGGA(line);
      Serial.println("GPS NMEA $GPGGA: " + line);
      Serial.printf("Parsed Lat: %s, Lon: %s\n", latitude.c_str(), longitude.c_str());
    }
  }

  long ir = particleSensor.getIR();
  long red = particleSensor.getRed();

  // BPM detection
  if (checkForBeat(ir)) {
    uint32_t now = millis();
    float delta = (now - lastBeat) / 1000.0;
    lastBeat = now;
    float rawBPM = 60.0 / delta;
    if (rawBPM >= 50 && rawBPM <= 180) {
      BPM = rawBPM;
      lastBPM = BPM;
    }
  }

  // SpO2 calculation
  if (red > 0 && ir > 0) {
    float ratio = (float)red / ir;
    float rawSpO2 = 110.0 - 25.0 * ratio;
    if (rawSpO2 >= 70 && rawSpO2 <= 100) SpO2 = rawSpO2;
  } else {
    BPM = 0;
    SpO2 = 0;
  }

  if (millis() - lastBeat > 5000) {
    BPM = 0;
  } else if (BPM == 0 && lastBPM > 0) {
    BPM = lastBPM;
  }

  // MPU6050 accelerometer & gyro readings
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);

  int16_t accX = Wire.read() << 8 | Wire.read();
  int16_t accY = Wire.read() << 8 | Wire.read();
  int16_t accZ = Wire.read() << 8 | Wire.read();
  Wire.read(); Wire.read(); // skip temperature
  int16_t gyroX = Wire.read() << 8 | Wire.read();
  int16_t gyroY = Wire.read() << 8 | Wire.read();
  int16_t gyroZ = Wire.read() << 8 | Wire.read();

  Ax = accX / 16384.0;
  Ay = accY / 16384.0;
  Az = accZ / 16384.0;
  totalAcc = sqrt(Ax*Ax + Ay*Ay + Az*Az);

  static unsigned long lastFallTriggered = 0;
  bool newFall = (totalAcc > 2.5 || totalAcc < 0.3);
  if (newFall && (millis() - lastFallTriggered > 10000)) {
    fallDetected = true;
    lastFallTime = millis();
    lastFallTriggered = millis();
    lastFallTimeStr = getCurrentTimeString();

    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);

    sendEmail(latitude, longitude, lastFallTimeStr);
    sendSMS("Lat: " + latitude + "\nLon: " + longitude, lastFallTimeStr);
    Serial.println("Fall detected!");
  } else if (!newFall && (millis() - lastFallTriggered > 2000)) {
    fallDetected = false;
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
  }

  delay(20); // 50Hz loop timing
}