# Water Level Monitoring System

ESP8266-based water tank level monitoring system with remote MQTT control, ultrasonic distance sensing, and a 4-digit TM1637 display.

## Hardware

### Components

| Component | Model / Type | Purpose |
|-----------|-------------|---------|
| Microcontroller | NodeMCU ESP8266 | Main controller with WiFi |
| Ultrasonic Sensor | HC-SR04 | Measures water distance |
| Display | TM1637 4-digit 7-segment | Shows water level % or settings |
| Buzzer | Active buzzer module | Alerts when out of range |
| Relay Module | 5V single-channel (active LOW) | Controls water pump motor |
| Power | USB or 5V adapter | Powers the NodeMCU |

### Wiring Diagram

```
NodeMCU          HC-SR04
-------          -------
D1 (GPIO5) ----> Trig
D2 (GPIO4) ----> Echo
3.3V ---------> VCC
GND ----------> GND

NodeMCU          TM1637
-------          ------
D5 (GPIO14) ---> CLK
D6 (GPIO12) ---> DIO
3.3V ---------> VCC
GND ----------> GND

NodeMCU          Relay Module
-------          -----------
D7 (GPIO13) ---> IN
5V -----------> VCC
GND ----------> GND

NodeMCU          Buzzer
-------          ------
D3 (GPIO0) ----> Signal
GND ----------> GND

Relay Module     Motor
-----------      -----
COM ----------> Live wire (cut)
NO  ----------> Motor live
Motor neutral ---> Mains neutral
```

> **Important:** The relay module is **active LOW** — `LOW` turns the relay ON (motor runs), `HIGH` turns it OFF.

### GPIO Reference

| Pin | GPIO | Function |
|-----|------|----------|
| D1 | GPIO5 | Ultrasonic Trig |
| D2 | GPIO4 | Ultrasonic Echo |
| D3 | GPIO0 | Buzzer |
| D5 | GPIO14 | TM1637 CLK |
| D6 | GPIO12 | TM1637 DIO |
| D7 | GPIO13 | Motor Relay |
| LED_BUILTIN | GPIO2 | Status LED |

## How It Works

### Measurement

1. The HC-SR04 sensor sends an ultrasonic pulse downward into the water tank.
2. The pulse bounces off the water surface and returns to the sensor.
3. The time-of-flight is converted to distance: `distance = duration * 0.034 / 2`
4. Three readings are averaged to filter noise (`measureDistanceAverage()`).
5. Distance is mapped to a percentage:
   ```
   percentage = ((tankHeight - (distance - minDistance)) / tankHeight) * 100
   ```
   - `minDistance` (default 7cm) = tank full
   - `maxDistance` (default 65cm) = tank empty
6. The percentage is published to MQTT and shown on the TM1637 display.

### Non-Blocking Design

The main loop uses `millis()`-based timing instead of `delay()` for:

- **Sensor readings** — every `currentDelay` (5s) in range, or 800ms when out of range
- **Buzzer beeps** — state-machine based, 3 beeps of 150ms on/off without blocking
- **Display restoration** — MQTT setting values show for 500ms, then percentage returns
- **WiFi reconnection** — checked every 30 seconds

### Offline Mode

If WiFi fails to connect within 60 seconds, the device enters **offline mode**:
- Sensor measurements continue normally
- Buzzer alerts still work
- Display shows water level percentage
- MQTT features are disabled
- LED stops blinking

## MQTT

### Broker Configuration

- **Server:** `mqtt.mstservices.me`
- **Port:** `1883`
- **Client ID:** `<TOKEN>/nodemcuwatertank`
- **LWT:** Publishes `"offline"` to `watertank/percentage` on disconnect (retained)

### Topics

#### Publish (Device → Broker)

| Topic | Payload | Retained | Description |
|-------|---------|----------|-------------|
| `watertank/percentage` | `0`-`100` | Yes | Current water level percentage |
| `watertank/motor/status` | `0` or `1` | Yes | Motor state after change |

#### Subscribe (Broker → Device)

| Topic | Payload | Description |
|-------|---------|-------------|
| `watertank/mindist` | `1`-`9999` | Set minimum distance (full tank) in cm |
| `watertank/maxdist` | `1`-`9999` | Set maximum distance (empty tank) in cm |
| `watertank/display` | `0` or `1` | Enable/disable the TM1637 display |
| `watertank/buzzer` | `0` or `1` | Enable/disable the buzzer |
| `watertank/motor` | `0` or `1` | Turn motor OFF/ON |

### Example MQTT Commands (CLI)

```bash
# Turn motor ON
mosquitto_pub -h mqtt.mstservices.me -t "watertank/motor" -m "1"

# Turn motor OFF
mosquitto_pub -h mqtt.mstservices.me -t "watertank/motor" -m "0"

# Set max distance to 70cm
mosquitto_pub -h mqtt.mstservices.me -t "watertank/maxdist" -m "70"

# Disable buzzer
mosquitto_pub -h mqtt.mstservices.me -t "watertank/buzzer" -m "0"

# Enable display
mosquitto_pub -h mqtt.mstservices.me -t "watertank/display" -m "1"

# Subscribe to water level
mosquitto_sub -h mqtt.mstservices.me -t "watertank/percentage"

# Subscribe to motor status feedback
mosquitto_sub -h mqtt.mstservices.me -t "watertank/motor/status"
```

### Display Behavior

When any MQTT command is received, the TM1637 display shows the received value (0, 1, or the distance) for **500ms**, then automatically restores the water level percentage.

## Configuration

All configurable values are in `config.h`:

| Variable | Default | Description |
|----------|---------|-------------|
| `ssid` | `"<WIFI SSID>"` | WiFi SSID |
| `password` | `"<WIFI PASSWORD>"` | WiFi password |
| `MQTT_SERVER` | `"<MQTT SERVER>"` | MQTT broker hostname |
| `MQTT_PORT` | `1883` | MQTT broker port |
| `MQTT_USER` | `"<MQTT USER>"` | MQTT username |
| `MQTT_PASSWORD` | `"<MQTT PASSWORD>"` | MQTT password |
| `defaultMinDistance` | `7` | Full tank distance (cm) |
| `defaultMaxDistance` | `65` | Empty tank distance (cm) |
| `currentDelay` | `5000` | Measurement interval (ms) |
| `defaultDisplayEnabled` | `true` | Display on at boot |
| `defaultBuzzerSound` | `true` | Buzzer on at boot |
| `defaultMotorState` | `false` | Motor off at boot |
| `motorRelayPin` | `D7` | Relay control pin |

## Building & Flashing

### Requirements

- [Arduino IDE](https://www.arduino.cc/en/software) with ESP8266 board support, **or**
- [PlatformIO](https://platformio.org/) (recommended)

### Arduino IDE Setup

1. Install Arduino IDE
2. Add ESP8266 board URL: `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
3. Install **esp8266** board package via Board Manager
4. Install libraries via Library Manager:
   - `TM1637Display` by Avishay Orpaz
   - `PubSubClient` by Nick O'Leary
5. Select board: **NodeMCU 1.0 (ESP-12E Module)**
6. Select correct COM port
7. Upload `watter_level.ino`

### PlatformIO Setup

```ini
[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino
lib_deps =
    avishorp/TM1637@^1.2.0
    knolleary/PubSubClient@^2.8
monitor_speed = 9600
```

## Serial Monitor

Baud rate: **9600**

Sample output:
```
Connecting to Wi-Fi.....
Wi-Fi connected!
IP Address: 192.168.1.42
Connecting to MQTT...connected!
Subscribed to all topics including motor control
Distance: 32 cm, Water Level: 53%, Motor: OFF
Message received on topic: watertank/motor
Display showing: 1
Motor turned ON
Motor turned ON via MQTT
```

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| Display shows `----` | Distance out of range or invalid | Check sensor alignment and wiring |
| Motor not triggering | Relay logic mismatch | Verify active LOW relay; check `motorRelayPin` |
| Buzzer not sounding | `buzzerSound` disabled | Send `1` to `watertank/buzzer` |
| No MQTT data | WiFi or broker down | Check serial for connection errors |
| Erratic readings | Sensor noise or water surface ripples | Readings are averaged (3x); increase `currentDelay` |
| Display flickers | Distance hovering at ±2cm threshold | Normal — 2cm deadband filters noise |
| LED blinks forever | WiFi not connecting | Check SSID/password; device enters offline mode after 60s |

## File Structure

```
watter_level/
├── watter_level.ino   # Main firmware (setup, loop, all functions)
├── config.h           # WiFi, MQTT, pin, and threshold configuration
├── README.md          # This file
└── AGENTS.md          # AI agent contribution guidelines
```

## License

Personal project — contact the author for usage rights.
