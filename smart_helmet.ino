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
#define ALCOHOL_THRESHOLD 500

// ── LEDs ──────────────────────────────────────────────
#define GREEN_LED 25
#define RED_LED   26

// ── 2 CHANNEL RELAY (ACTIVE LOW) ─────────────────────
#define RELAY1_PIN 32
#define RELAY2_PIN 33
#define RELAY_ON   LOW
#define RELAY_OFF  HIGH

// ── BUZZER ────────────────────────────────────────────
#define BUZZER_PIN 27

TinyGPSPlus gps;
String phoneNumber = "+916381618970";
bool alcoholDetected = false;

// ─────────────────────────────────────────────────────
bool waitForResponse(String expected, int timeoutMs = 5000) {
  long start = millis();
  String response = "";
  while (millis() - start < timeoutMs) {
    while (sim800.available()) {
      response += (char)sim800.read();
    }
    if (response.indexOf(expected) != -1) {
      Serial.println("OK: " + response);
      return true;
    }
  }
  Serial.println("TIMEOUT. Got: " + response);
  return false;
}

// ─────────────────────────────────────────────────────
void sendSMS(String message) {
  Serial.println("\n--- Sending SMS ---");

  sim800.println("AT+CMGF=1");
  if (!waitForResponse("OK", 3000)) {
    Serial.println("CMGF failed"); return;
  }

  sim800.print("AT+CMGS=\"");
  sim800.print(phoneNumber);
  sim800.println("\"");
  if (!waitForResponse(">", 5000)) {
    Serial.println("No > prompt"); return;
  }

  sim800.print(message);
  delay(500);
  sim800.write(26); // CTRL+Z

  if (waitForResponse("+CMGS", 10000)) {
    Serial.println("SMS Sent!");
  } else {
    Serial.println("SMS Failed");
  }
}

// ─────────────────────────────────────────────────────
String getGoogleMapsLink(double lat, double lng) {
  return "https://maps.google.com/?q=" + String(lat, 6) + "," + String(lng, 6);
}

// ─────────────────────────────────────────────────────
void setLED(bool alert) {
  digitalWrite(GREEN_LED, alert ? LOW  : HIGH);
  digitalWrite(RED_LED,   alert ? HIGH : LOW);
}

// ─────────────────────────────────────────────────────
void setRelay(bool alert) {
  if (alert) {
    digitalWrite(RELAY1_PIN, RELAY_ON);
    digitalWrite(RELAY2_PIN, RELAY_ON);
    Serial.println("RELAY1 ON - Ignition cut");
    Serial.println("RELAY2 ON - Fuel pump cut");
  } else {
    digitalWrite(RELAY1_PIN, RELAY_OFF);
    digitalWrite(RELAY2_PIN, RELAY_OFF);
    Serial.println("RELAY1 OFF - Ignition restored");
    Serial.println("RELAY2 OFF - Fuel pump restored");
  }
}

// ─────────────────────────────────────────────────────
// Beeps N times with given on/off duration (ms)
void buzz(int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(onMs);
    digitalWrite(BUZZER_PIN, LOW);
    delay(offMs);
  }
}

// ─────────────────────────────────────────────────────
// 3 second warning beeps before relay cuts power
void buzzerWarning() {
  Serial.println("BUZZER WARNING - 3 beeps before cutoff...");
  buzz(3, 500, 500); // 3 beeps, 0.5s on, 0.5s off = 3 seconds total
}

// Continuous fast beep while alcohol is detected
void buzzerAlert() {
  buzz(1, 100, 100); // single short beep — called repeatedly in loop
}

// ─────────────────────────────────────────────────────
void triggerAlcoholAlert() {
  Serial.println("\nALCOHOL DETECTED!");

  // 1. Red ON, Green OFF
  setLED(true);

  // 2. Warning beeps BEFORE relay cuts power
  buzzerWarning();

  // 3. Trigger both relays AFTER warning
  setRelay(true);

  // 4. Wait up to 10s for GPS fix
  Serial.println("Getting GPS location...");
  long gpsWait = millis();
  while (!gps.location.isValid() && millis() - gpsWait < 10000) {
    while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());
    delay(100);
  }

  // 5. Build SMS
  String sms = "";
  if (gps.location.isValid()) {
    double lat  = gps.location.lat();
    double lng  = gps.location.lng();
    String link = getGoogleMapsLink(lat, lng);

    sms  = "ALCOHOL ALERT!\n";
    sms += "Driver has consumed alcohol.\n";
    sms += "Vehicle has been stopped.\n\n";
    sms += "Last Location:\n";
    sms += "Lat: " + String(lat, 6) + "\n";
    sms += "Lng: " + String(lng, 6) + "\n";
    sms += link;

    Serial.println("Location: " + link);

  } else {
    sms  = "ALCOHOL ALERT!\n";
    sms += "Driver has consumed alcohol.\n";
    sms += "Vehicle has been stopped.\n\n";
    sms += "GPS location unavailable.";
    Serial.println("No GPS fix. Sending without location.");
  }

  // 6. Send SMS
  sendSMS(sms);
}

// ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);

  // ── LED Init ──────────────────────────────────────
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED,   OUTPUT);
  setLED(false);
  Serial.println("GREEN ON - System Safe");

  // ── Relay Init ────────────────────────────────────
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  setRelay(false);
  Serial.println("RELAYS OFF - Vehicle Running");

  // ── Buzzer Init ───────────────────────────────────
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  buzz(2, 100, 100); // 2 quick beeps = system ready
  Serial.println("BUZZER Ready");

  // ── SIM800L Init ──────────────────────────────────
  sim800.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX);
  delay(5000);
  Serial.println("SIM800L Ready");
  sim800.println("AT");        delay(500);
  sim800.println("ATE0");      delay(500);
  sim800.println("AT+CMGF=1"); delay(500);

  // ── GPS Init ──────────────────────────────────────
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("GPS Started...");

  Serial.println("\nSystem Ready. Monitoring alcohol...\n");
}

// ─────────────────────────────────────────────────────
void loop() {

  // ── Feed GPS ──────────────────────────────────────
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  // ── Read Alcohol Sensor ────────────────────────────
  int alcoholLevel = analogRead(ALCOHOL_PIN);

  // ── Alcohol Detected ──────────────────────────────
  if (alcoholLevel > ALCOHOL_THRESHOLD && !alcoholDetected) {
    alcoholDetected = true;
    triggerAlcoholAlert();
  }

  // ── Keep buzzing while alcohol still present ──────
  if (alcoholDetected) {
    buzzerAlert();
  }

  // ── Alcohol Cleared ────────────────────────────────
  if (alcoholLevel < ALCOHOL_THRESHOLD && alcoholDetected) {
    alcoholDetected = false;
    setLED(false);
    setRelay(false);
    digitalWrite(BUZZER_PIN, LOW);  // Buzzer OFF
    buzz(2, 100, 100);              // 2 beeps = system cleared
    Serial.println("Alcohol cleared - System Safe - Vehicle Restored");
  }

  // ── Serial Status every 3s ────────────────────────
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 3000) {
    lastPrint = millis();
    Serial.print("Alcohol: ");  Serial.print(alcoholLevel);
    Serial.print(" | Sats: ");  Serial.print(gps.satellites.value());
    Serial.print(" | GPS: ");   Serial.print(gps.location.isValid() ? "Fix OK" : "Waiting...");
    Serial.print(" | Relay: "); Serial.print(alcoholDetected ? "ON" : "OFF");
    Serial.print(" | Status: ");Serial.println(alcoholDetected ? "ALERT" : "SAFE");
  }
}
