# Smart Helmet System

![Smart Helmet Banner](https://image.pollinations.ai/prompt/Smart%20helmet%20ESP32%20IoT%20device%20with%20GPS%20GSM%20sensors%20alcohol%20detection%20circuit%20dark%20blueprint%20style?width=1280&height=640&nologo=true)

A comprehensive ESP32-based smart helmet system that detects alcohol consumption, tracks GPS location, and sends SMS alerts to emergency contacts.

## Features

- **Alcohol Detection** - MQ-3 sensor detects driver alcohol levels
- **GPS Tracking** - NEO-6M GPS module for real-time location tracking
- **SMS Alerts** - SIM800L GSM module sends emergency SMS with location
- **Vehicle Control** - 2-channel relay controls vehicle ignition
- **LED Indicators** - Visual feedback for system status

## Hardware Components

| Component | Model | Purpose |
|-----------|-------|---------|
| Microcontroller | ESP32 | Main processing unit |
| Alcohol Sensor | MQ-3 | Breath alcohol detection |
| GPS Module | NEO-6M | Location tracking |
| GSM Module | SIM800L | SMS communication |
| Relay Module | 2-Channel | Vehicle control |

## Hardware Setup

```
ESP32 Pinout:
├── GPIO 12 (TX) ←→ SIM800L RX
├── GPIO 13 (RX) ←→ SIM800L TX
├── GPIO 16 (TX) ←→ NEO-6M RX
├── GPIO 17 (RX) ←→ NEO-6M TX
├── GPIO 25      → Green LED (+)
├── GPIO 26      → Relay 1 Signal
├── GPIO 27      → Relay 2 Signal
├── GPIO 33      → Red LED (+)
└── GPIO 34      → MQ-3 Analog Output
```

## Installation

1. Open `smart_helmet.ino` in Arduino IDE
2. Install required libraries:
   - TinyGPSPlus (by Mikal Hart)
3. Select ESP32 Dev Module as board
4. Upload to your ESP32

## Configuration

Edit these variables in the code:
```cpp
String phoneNumber = "+918056273107";  // Emergency contact
#define ALCOHOL_THRESHOLD 650;          // Sensitivity adjustment
```

## How It Works

1. **Startup**: System sends a checklist SMS with all sensor statuses
2. **Monitoring**: Continuously reads MQ-3 alcohol sensor
3. **Detection**: If alcohol detected above threshold:
   - Turns ON red LED
   - Disables vehicle (relays OFF)
   - Sends SMS with GPS location to emergency contact
4. **Reset**: When alcohol clears, re-enables vehicle

## SMS Messages

### Startup Checklist
```
=== SMART HELMET ONLINE ===

SENSOR CHECKLIST:
MQ-3 Sensor : ✅ OK
GPS Module  : ✅ OK
SIM800L GSM : ✅ OK
Relay Module: ✅ OK
Location: [Google Maps Link]
```

### Alcohol Alert
```
🚨 ALCOHOL ALERT!
Driver has consumed alcohol.
Vehicle has been stopped.

Last Location:
Lat: xx.xxxxxx
Lng: xx.xxxxxx
[Google Maps Link]
```

## License

MIT License
