#include <HardwareSerial.h>
#include <TinyGPSPlus.h>

// ── SIM800L ───────────────────────────────────────────
HardwareSerial sim800(1);
#define SIM800_TX 12
#define SIM800_RX 13

// ── GPS (NEO-6M) ──────────────────────────────────────
HardwareSerial gpsSerial(2);
#define GPS_RX 16
#define GPS_TX 17

// ── ALCOHOL SENSOR (MQ-3) ─────────────────────────────
#define ALCOHOL_PIN       34
#define ALCOHOL_THRESHOLD 650   // triggers at 651+

// ── LEDs ──────────────────────────────────────────────
#define GREEN_LED 25
#define RED_LED   33

// ── 2-CHANNEL RELAY ───────────────────────────────────
#define RELAY1_PIN 26
#define RELAY2_PIN 27

TinyGPSPlus gps;
String phoneNumber = "+918056273107";

bool alcoholDetected = false;
unsigned long lastSMSTime = 0;
#define SMS_COOLDOWN 30000  // 30 seconds cooldown between SMS

// ─────────────────────────────────────────────────────
// Relay Control (Active LOW: LOW = ON, HIGH = OFF)
void setRelays(bool state) {
  if (state) {
    digitalWrite(RELAY1_PIN, LOW);
    digitalWrite(RELAY2_PIN, LOW);
    Serial.println("✅ Relays ON  → Vehicle ENABLED");
  } else {
    digitalWrite(RELAY1_PIN, HIGH);
    digitalWrite(RELAY2_PIN, HIGH);
    Serial.println("🔴 Relays OFF → Vehicle DISABLED");
  }
}

// ─────────────────────────────────────────────────────
bool waitForResponse(String expected, int timeoutMs = 5000) {
  long start = millis();
  String response = "";
  while (millis() - start < timeoutMs) {
    while (sim800.available()) {
      response += (char)sim800.read();
    }
    if (response.indexOf(expected) != -1) {
      Serial.println("✓ " + response);
      return true;
    }
  }
  Serial.println("✗ Timeout. Got: " + response);
  return false;
}

// ─────────────────────────────────────────────────────
void initSIM800L() {
  Serial.println("⚙ Initializing SIM800L...");
  sim800.println("AT");              delay(1000);
  sim800.println("ATE0");            delay(1000);
  sim800.println("AT+CSCS=\"GSM\""); delay(1000);
  Serial.println("⚙ SIM800L init done.");
}

// ─────────────────────────────────────────────────────
void sendSMS(String message) {
  Serial.println("\n--- Sending SMS ---");

  initSIM800L();

  bool cmgfOK = false;
  for (int i = 0; i < 5; i++) {
    sim800.println("AT+CMGF=1");
    if (waitForResponse("OK", 5000)) {
      cmgfOK = true;
      break;
    }
    Serial.println("⚠ CMGF retry " + String(i + 1));
    delay(3000);
  }
  if (!cmgfOK) {
    Serial.println("✗ CMGF failed after 5 retries"); return;
  }

  sim800.print("AT+CMGS=\"");
  sim800.print(phoneNumber);
  sim800.println("\"");
  if (!waitForResponse(">", 5000)) {
    Serial.println("✗ No > prompt"); return;
  }

  sim800.print(message);
  delay(500);
  sim800.write(26); // CTRL+Z

  if (waitForResponse("+CMGS", 15000)) {
    Serial.println("✅ SMS Sent!");
  } else {
    Serial.println("✗ SMS Failed");
  }
}

// ─────────────────────────────────────────────────────
String getGoogleMapsLink(double lat, double lng) {
  return "https://maps.google.com/?q=" + String(lat, 6) + "," + String(lng, 6);
}

// ─────────────────────────────────────────────────────
void setLED(bool alcoholState) {
  if (alcoholState) {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED,   HIGH);
  } else {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED,   LOW);
  }
}

// ─────────────────────────────────────────────────────
// STARTUP CHECKLIST SMS
void sendStartupSMS() {
  Serial.println("\n📋 Running startup checklist...");

  // ── Check MQ-3 ──────────────────────────────────
  int alcoholLevel = analogRead(ALCOHOL_PIN);
  String mq3Status = (alcoholLevel >= 0 && alcoholLevel < 4095)
                     ? "✅ OK (Value: " + String(alcoholLevel) + ")"
                     : "❌ ERROR";
  Serial.println("MQ-3 : " + mq3Status);

  // ── Check Relay ──────────────────────────────────
  // Relays already ON at this point — confirm by reading pin state
  bool relay1OK = (digitalRead(RELAY1_PIN) == LOW);
  bool relay2OK = (digitalRead(RELAY2_PIN) == LOW);
  String relayStatus = (relay1OK && relay2OK)
                       ? "✅ OK (Both ON)"
                       : "❌ ERROR";
  Serial.println("Relay: " + relayStatus);

  // ── Check GPS ────────────────────────────────────
  Serial.println("📡 Waiting for GPS fix (max 15s)...");
  long gpsWait = millis();
  while (!gps.location.isValid() && millis() - gpsWait < 15000) {
    while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());
    delay(100);
  }
  String gpsStatus = "";
  String locationLine = "";
  if (gps.location.isValid()) {
    gpsStatus = "✅ OK (Sats: " + String(gps.satellites.value()) + ")";
    locationLine = "\nLocation: " + getGoogleMapsLink(gps.location.lat(), gps.location.lng());
  } else {
    gpsStatus = "⚠ No Fix Yet";
    locationLine = "\nLocation: Unavailable";
  }
  Serial.println("GPS  : " + gpsStatus);

  // ── Check SIM800L ────────────────────────────────
  sim800.println("AT+CREG?");
  String simStatus = waitForResponse("+CREG: 0,1", 3000)
                     ? "✅ OK (Network Registered)"
                     : "⚠ Check SIM/Network";
  Serial.println("SIM  : " + simStatus);

  // ── Build and send checklist SMS ─────────────────
  String sms  = "=== SMART HELMET ONLINE ===\n\n";
  sms        += "SENSOR CHECKLIST:\n";
  sms        += "MQ-3 Sensor : " + mq3Status + "\n";
  sms        += "GPS Module  : " + gpsStatus + "\n";
  sms        += "SIM800L GSM : " + simStatus + "\n";
  sms        += "Relay Module: " + relayStatus + "\n";
  sms        += locationLine;

  Serial.println("\n📤 Sending startup SMS...");
  sendSMS(sms);
  // lastSMSTime NOT set here — startup SMS does not block alcohol alert
}

// ─────────────────────────────────────────────────────
void triggerAlcoholAlert() {
  Serial.println("\n🚨 ALCOHOL DETECTED!");

  // 1. Red LED ON, Green OFF
  setLED(true);

  // 2. STOP both relays → kill vehicle immediately
  setRelays(false);

  // 3. Wait for GPS fix
  Serial.println("📡 Getting GPS location...");
  long gpsWait = millis();
  while (!gps.location.isValid() && millis() - gpsWait < 10000) {
    while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());
    delay(100);
  }

  // 4. Build SMS
  String sms = "";
  if (gps.location.isValid()) {
    double lat  = gps.location.lat();
    double lng  = gps.location.lng();
    String link = getGoogleMapsLink(lat, lng);

    sms  = "🚨 ALCOHOL ALERT!\n";
    sms += "Driver has consumed alcohol.\n";
    sms += "Vehicle has been stopped.\n\n";
    sms += "Last Location:\n";
    sms += "Lat: " + String(lat, 6) + "\n";
    sms += "Lng: " + String(lng, 6) + "\n";
    sms += link;

    Serial.println("📍 " + link);
  } else {
    sms  = "🚨 ALCOHOL ALERT!\n";
    sms += "Driver has consumed alcohol.\n";
    sms += "Vehicle has been stopped.\n\n";
    sms += "GPS location unavailable.";
    Serial.println("⚠ No GPS fix.");
  }

  // 5. Send SMS with cooldown
  if (millis() - lastSMSTime > SMS_COOLDOWN) {
    sendSMS(sms);
    lastSMSTime = millis();
  } else {
    Serial.println("⚠ SMS cooldown active, skipping duplicate alert...");
  }
}

// ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);

  // LED setup
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED,   OUTPUT);
  setLED(false);
  Serial.println("✅ GREEN LED ON → System Safe");

  // Relay setup — both ON at boot
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  setRelays(true);

  // SIM800L init
  sim800.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX);
  delay(8000);
  Serial.println("SIM800L Ready");
  initSIM800L();
  sim800.println("AT+CMGF=1"); delay(1000);

  // GPS init
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("GPS Started...");

  // Send startup checklist SMS
  sendStartupSMS();

  Serial.println("\n✅ System Ready. Monitoring alcohol...");
}

// ─────────────────────────────────────────────────────
void loop() {

  // ── Feed GPS ──────────────────────────────────────
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  // ── Read Alcohol Sensor ───────────────────────────
  int alcoholLevel = analogRead(ALCOHOL_PIN);

  // ── Alcohol Detected ──────────────────────────────
  if (alcoholLevel > ALCOHOL_THRESHOLD && !alcoholDetected) {
    alcoholDetected = true;
    triggerAlcoholAlert();
  }

  // ── Alcohol Cleared ───────────────────────────────
  if (alcoholLevel < ALCOHOL_THRESHOLD && alcoholDetected) {
    alcoholDetected = false;
    setLED(false);
    setRelays(true);
    Serial.println("✅ Alcohol cleared → GREEN LED ON, Vehicle RE-ENABLED");
  }

  // ── Status every 3s ───────────────────────────────
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 3000) {
    lastPrint = millis();
    Serial.print("🍺 Alcohol: "); Serial.print(alcoholLevel);
    Serial.print(" | Sats: ");    Serial.print(gps.satellites.value());
    Serial.print(" | Status: ");  Serial.println(alcoholDetected ? "🔴 ALERT" : "🟢 SAFE");
  }
}
