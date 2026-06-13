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
#define WS_DEVICE_CLIENTID "9POWuKe/nodemcuwatertank"

// MQTT topics
const char* publishTopic = "watertank/percentage";
const char* subscribeTopicMinDist = "watertank/mindist";
const char* subscribeTopicMaxDist = "watertank/maxdist";
const char* subscribeTopicDisplay = "watertank/display";
const char* subscribeTopicBuzzer = "watertank/buzzer";
const char* subscribeTopicMotor = "watertank/motor";        // Topic for motor control

// Default threshold distance (in cm)
const int defaultThresholdDistance = 10;

// Variable to track the delay duration
int currentDelay = 5000; // Start with 5000 ms delay

// Display control flag (default state)
const bool defaultDisplayEnabled = true;
// Buzzer control flag (default state)
const bool defaultBuzzerSound = true;

// Motor control defaults
const bool defaultMotorState = false;    // Motor off by default

// Define the relay pin for motor control
const int motorRelayPin = D7;  // GPIO13 - Change this to your actual relay pin

#endif // CONFIG_H
