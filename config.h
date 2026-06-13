#include "pins_arduino.h"
#ifndef CONFIG_H
#define CONFIG_H

// Wi-Fi credentials
const char* ssid = "";
const char* password = "";

// MQTT server configuration
#define MQTT_SERVER ""
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_PORT 1883
#define WS_DEVICE_CLIENTID ""

// MQTT topics
const char* publishTopic = "watertank/percentage";
const char* publishTopicMotor = "watertank/motor/status";
const char* subscribeTopicMinDist = "watertank/mindist";
const char* subscribeTopicMaxDist = "watertank/maxdist";
const char* subscribeTopicDisplay = "watertank/display";
const char* subscribeTopicBuzzer = "watertank/buzzer";
const char* subscribeTopicMotor = "watertank/motor";        // Topic for motor control

// Default threshold distances (in cm)
const int defaultMinDistance = 7;   // Water full level
const int defaultMaxDistance = 65;  // Water low level

// Variable to track the delay duration
const int currentDelay = 5000; // Start with 5000 ms delay

// Display control flag (default state)
const bool defaultDisplayEnabled = true;
// Buzzer control flag (default state)
const bool defaultBuzzerSound = true;

// Motor control defaults
const bool defaultMotorState = false;    // Motor off by default

// Define the relay pin for motor control
const int motorRelayPin = D7;  // GPIO15 — Change this to your actual relay pin

#endif // CONFIG_H
