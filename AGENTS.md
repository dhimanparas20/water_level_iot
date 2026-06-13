# AGENTS.md — AI Agent Contribution Guidelines

This file provides context for AI agents (opencode, Copilot, Cursor, etc.) working on this codebase.

## Project Overview

ESP8266 (NodeMCU) water tank level monitor. Reads distance via HC-SR04 ultrasonic sensor, calculates water level percentage, displays on TM1637 4-digit display, publishes to MQTT, and accepts remote commands for motor relay, buzzer, display, and threshold configuration.

## Architecture

```
watter_level.ino   — Single-file Arduino sketch: setup(), loop(), all functions
config.h           — Credentials, MQTT topics, pin assignments, defaults
```

Everything runs in a single `.ino` file. There are no external libraries beyond the two listed below. No classes, no modules — it is a flat, procedural Arduino sketch.

## Key Libraries

| Library | Version | Purpose |
|---------|---------|---------|
| `TM1637Display` | Avishay Orpaz | Drives the 4-digit 7-segment display |
| `PubSubClient` | Nick O'Leary | MQTT client for ESP8266 |
| `ESP8266WiFi` | Built-in | WiFi connectivity |

## Critical Design Decisions

### 1. Non-Blocking Loop

The main `loop()` uses `millis()`-based timing exclusively. **Never add `delay()` calls in the main loop or in functions called from the loop.** The only acceptable `delay()` calls are:

- `delayMicroseconds()` in `measureDistance()` (sensor timing requirement)
- `delay(10)` between sensor readings in `measureDistanceAverage()`
- `delay(5000)` in `connectToMQTT()` retry (intentional backoff during connection only)

### 2. Relay Logic: Active LOW

The motor relay is **active LOW**:
- `LOW` = relay ON (motor runs)
- `HIGH` = relay OFF (motor stops)

In `controlMotor()`: `digitalWrite(motorRelayPin, state ? LOW : HIGH);`
In `setup()`: `digitalWrite(motorRelayPin, HIGH);` (motor off at boot)

**Do not change this logic without confirming the relay module type.**

### 3. MQTT Callback Order

In `mqttCallback()`, the display block runs **before** the motor control block. This is intentional — calling `controlMotor()` triggers `client.publish()` which can briefly interfere with display updates. Always keep display updates before any publish operations in the callback.

### 4. Sensor Averaging

`measureDistanceAverage()` takes 3 readings with 10ms gaps and averages valid ones. This filters noise. Do not reduce the reading count below 3.

### 5. Distance Noise Filter

Display and MQTT publish only trigger when distance changes by more than **2cm** (`abs(distance - previousDistance) > 2`). This prevents display flicker from sensor noise.

## Code Style

- **No comments** unless explicitly requested by the user
- **Single-file architecture** — all functions in `watter_level.ino`, no separate `.cpp` files
- **Pin definitions** use NodeMCU `D*` notation (e.g., `D1`, `D7`), not raw GPIO numbers
- **String comparisons** use `strcmp()`, never `String()` class (prevents heap fragmentation on ESP8266)
- **MQTT messages** are null-terminated char arrays, not String objects
- **Serial output** at 9600 baud for debugging

## Topics Reference

### Publish (device → broker)
| Topic | Payload | Retained |
|-------|---------|----------|
| `watertank/percentage` | `0`-`100` | Yes |
| `watertank/motor/status` | `0` or `1` | Yes |

### Subscribe (broker → device)
| Topic | Payload | Description |
|-------|---------|-------------|
| `watertank/motor` | `0`/`1` | Motor control |
| `watertank/buzzer` | `0`/`1` | Buzzer enable/disable |
| `watertank/display` | `0`/`1` | Display enable/disable |
| `watertank/mindist` | cm value | Min distance (full tank) |
| `watertank/maxdist` | cm value | Max distance (empty tank) |

## Variables Reference

### Global State Variables
| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| `motorState` | `bool` | `false` | Current motor on/off state |
| `buzzerSound` | `bool` | `true` | Buzzer enabled flag |
| `displayEnabled` | `bool` | `true` | Display enabled flag |
| `minDistance` | `int` | `7` | Full tank distance (cm) |
| `maxDistance` | `int` | `65` | Empty tank distance (cm) |
| `offlineMode` | `bool` | `false` | WiFi/MQTT offline flag |
| `previousDistance` | `int` | `-1` | Last measured distance (noise filter) |
| `lastPercentage` | `int` | `0` | Last water level % (display restore) |

### Timing Variables
| Variable | Purpose |
|----------|---------|
| `lastMeasurementTime` | Last sensor reading timestamp |
| `lastWifiCheckTime` | Last WiFi health check timestamp |
| `displayShowTime` | When MQTT value was shown on display |
| `buzzerStartTime` | When current beep started |
| `del_ay` | Current measurement interval (0, 800, or 5000ms) |

### Non-Blocking Buzzer State
| Variable | Purpose |
|----------|---------|
| `buzzerActive` | Whether buzzer is currently beeping |
| `buzzerOn` | Whether current beep is in ON phase |
| `buzzerBeepCount` | Number of beeps completed (0-3) |

## Functions Reference

| Function | Description |
|----------|-------------|
| `setup()` | Pin init, display init, WiFi connect, MQTT connect |
| `loop()` | Non-blocking buzzer, display restore, WiFi check, MQTT loop, sensor measurement |
| `controlMotor(bool)` | Sets relay pin, updates `motorState`, publishes status |
| `measureDistance()` | Single ultrasonic reading with 30ms timeout |
| `measureDistanceAverage()` | 3 readings averaged |
| `connectToWiFi()` | Blocking WiFi connect with 60s timeout, LED blink |
| `connectToMQTT()` | Blocking MQTT connect with 15s timeout |
| `mqttCallback()` | Handles all incoming MQTT messages |
| `blinkLED(int)` | Blocking LED blink (used only during connection) |
| `showConnectingPattern()` | Animated pattern on TM1637 during connection |

## Pitfalls to Avoid

1. **Never use `String()` for topic comparison** — always `strcmp()`. `String()` causes heap fragmentation on ESP8266.
2. **Never add blocking delays in `loop()`** — use `millis()` state machines.
3. **Never change relay logic** without verifying the relay module is active HIGH vs active LOW.
4. **Never publish to `subscribeTopic`** from inside `mqttCallback()` without understanding re-entrancy.
5. **Never remove the `tankHeight <= 0` guard** — it prevents division by zero crash.
6. **Never reduce `pulseIn` timeout** below 20000 (20ms) — HC-SR04 needs up to 23ms for max range.
7. **Never remove the 2cm distance deadband** — sensor noise causes display flicker without it.

## Common Modifications

### Change measurement interval
Edit `currentDelay` in `config.h` (default 5000ms).

### Change tank dimensions
Edit `defaultMinDistance` and `defaultMaxDistance` in `config.h`.

### Change relay pin
Edit `motorRelayPin` in `config.h`. Verify the GPIO matches your wiring.

### Add a new MQTT topic
1. Add topic string in `config.h`
2. Subscribe in `connectToMQTT()`
3. Handle in `mqttCallback()` using `strcmp()`
4. Add to display block condition if value should show on TM1637

### Change buzzer pattern
Modify the buzzer state machine in `loop()` — change the `150`ms values or the `3` beep count.

## Testing

There is no automated test suite. Test by:
1. Flash to NodeMCU
2. Open Serial Monitor at 9600 baud
3. Send MQTT commands via `mosquitto_pub`
4. Verify display, buzzer, and relay respond correctly
5. Verify water level percentage updates when water level changes

## Dependencies

No `platformio.ini` or `library.json` is committed. Dependencies are managed manually in Arduino IDE. If migrating to PlatformIO, add:

```ini
lib_deps =
    avishorp/TM1637@^1.2.0
    knolleary/PubSubClient@^2.8
```
