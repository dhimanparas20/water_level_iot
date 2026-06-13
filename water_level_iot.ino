#include <TM1637Display.h> // Include the TM1637 library
#include <ESP8266WiFi.h>   // Include the ESP8266 Wi-Fi library
#include <PubSubClient.h>  // Include the PubSubClient library for MQTT
#include "config.h"        // Include the configuration file

// Define pins for the ultrasonic sensor
const int trigPin = D1;  // Trig pin of ultrasonic sensor (GPIO5)
const int echoPin = D2;  // Echo pin of ultrasonic sensor (GPIO4)

// Define pins for the TM1637 display
const int CLK = D5;      // Clock pin of TM1637 (GPIO14)
const int DIO = D6;      // Data pin of TM1637 (GPIO12)

// Define the buzzer pin
const int buzzerPin = D3;   // Buzzer pin (GPIO0)

// Define the built-in LED pin
const int ledPin = LED_BUILTIN; // Built-in NodeMCU LED pin (usually GPIO2 or GPIO0)

// Create an instance of the TM1637Display class
TM1637Display display(CLK, DIO);

// Variable to store the previous distance value
int previousDistance = -1; // Initialize with an invalid value

// Offline mode flag
bool offlineMode = false; // Start in online mode by default

// Display control flag
bool displayEnabled = defaultDisplayEnabled; // Use default value from config.h
// Buzzer Control flag
bool buzzerSound = defaultBuzzerSound; // Use default value from config.h
int del_ay = 0; // declare the delay value

// Motor control variable
bool motorState = defaultMotorState;     // Current motor state

// Threshold distances
int minDistance = defaultMinDistance;
int maxDistance = defaultMaxDistance;

// Create Wi-Fi and MQTT clients
WiFiClient espClient;
PubSubClient client(espClient);

// Function prototypes
void connectToWiFi();
void connectToMQTT();
long measureDistance();
int measureDistanceAverage();
void blinkLED(int frequency);
void mqttCallback(char* topic, byte* payload, unsigned int length);
void showConnectingPattern();
void controlMotor(bool state);

// For non-blocking loop timing
unsigned long lastMeasurementTime = 0;

// WiFi reconnection timer
unsigned long lastWifiCheckTime = 0;

// Non-blocking buzzer variables
bool buzzerActive = false;
unsigned long buzzerStartTime = 0;
int buzzerBeepCount = 0;
bool buzzerOn = false;

// Non-blocking display variables
unsigned long displayShowTime = 0;
bool displayTemporary = false;
int lastPercentage = 0;

void setup() {
  // Initialize serial communication
  Serial.begin(9600);

  // Set pin modes for ultrasonic sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Set pin mode for the buzzer
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, HIGH); // Turn off the buzzer initially

  // Set pin mode for the built-in LED
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH); // Turn off the LED initially

  // Set pin mode for the motor relay
  pinMode(motorRelayPin, OUTPUT);
  digitalWrite(motorRelayPin, HIGH); // Turn off the motor initially (active LOW)

  // Initialize the TM1637 display
  display.setBrightness(1); // Set brightness (0-7, 7 is max)
  displayEnabled = defaultDisplayEnabled; // Use default value from config.h

  // Attempt to connect to Wi-Fi
  connectToWiFi();

  // If not in offline mode, configure MQTT client
  if (!offlineMode) {
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(mqttCallback); // Set the callback function for incoming messages

    // Connect to MQTT broker
    connectToMQTT();
  }
}

void loop() {
  // Non-blocking buzzer handling
  if (buzzerActive) {
    unsigned long elapsed = millis() - buzzerStartTime;
    if (!buzzerOn && elapsed >= 150) {
      digitalWrite(buzzerPin, LOW);
      buzzerOn = true;
      buzzerStartTime = millis();
    } else if (buzzerOn && elapsed >= 150) {
      digitalWrite(buzzerPin, HIGH);
      buzzerOn = false;
      buzzerStartTime = millis();
      buzzerBeepCount++;
      if (buzzerBeepCount >= 3) {
        buzzerActive = false;
      }
    }
  }

  // Non-blocking display restoration
  if (displayTemporary && millis() - displayShowTime >= 500) {
    displayTemporary = false;
    if (displayEnabled) {
      display.showNumberDec(lastPercentage, false);
    }
  }

  // Check WiFi connection every 30 seconds
  if (!offlineMode && millis() - lastWifiCheckTime >= 30000) {
    lastWifiCheckTime = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, attempting to reconnect...");
      WiFi.reconnect();
    }
  }

  // If not in offline mode, ensure the MQTT client stays connected
  if (!offlineMode && !client.connected()) {
    connectToMQTT();
  }
  if (!offlineMode) {
    client.loop();
  }

  unsigned long now = millis();
  if (now - lastMeasurementTime >= del_ay) {
    lastMeasurementTime = now;

    // Measure the distance (averaged from 3 readings)
    int distance = measureDistanceAverage();

    // Guard against division by zero
    int tankHeight = maxDistance - minDistance;
    if (tankHeight <= 0) {
      Serial.println("Error: maxDistance must be greater than minDistance");
      del_ay = currentDelay;
      return;
    }

    // Calculate the water level percentage
    int waterLevel = tankHeight - (distance - minDistance);
    int percentage = (waterLevel * 100) / tankHeight;

    // Ensure percentage is within 0-100 range
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;

    // Check if the distance is out of range (for buzzer)
    if (distance < minDistance || distance > maxDistance) {
      if (buzzerSound && !buzzerActive) {
        buzzerActive = true;
        buzzerStartTime = millis();
        buzzerBeepCount = 0;
        buzzerOn = false;
        del_ay = 800;
      }
    } else {
      // Ensure the buzzer is off
      digitalWrite(buzzerPin, HIGH);
      buzzerActive = false;
      // Restore the delay when the distance is within range
      del_ay = currentDelay;
    }

    // Only proceed if the distance has changed by more than 2cm (filter noise)
    if (abs(distance - previousDistance) > 2) {
      previousDistance = distance;
      lastPercentage = percentage;

      // Print the distance and percentage to the Serial Monitor
      Serial.print("Distance: ");
      Serial.print(distance);
      Serial.print(" cm, Water Level: ");
      Serial.print(percentage);
      Serial.print("%, Motor: ");
      Serial.println(motorState ? "ON" : "OFF");

      // Publish the percentage to the MQTT topic
      if (!offlineMode) {
        char message[10];
        snprintf(message, sizeof(message), "%d", percentage);
        client.publish(publishTopic, message, true);
      }

      // Display the percentage on the TM1637 display (if enabled and not temporarily showing a value)
      if (displayEnabled && !displayTemporary) {
        display.showNumberDec(percentage, false);
      }
    }
  }
}

// Function to control motor
void controlMotor(bool state) {
  motorState = state;
  digitalWrite(motorRelayPin, state ? LOW : HIGH); // Active LOW relay
  Serial.print("Motor turned ");
  Serial.println(state ? "ON" : "OFF");

  // Publish motor state back
  if (!offlineMode) {
    char message[10];
    snprintf(message, sizeof(message), "%d", state ? 1 : 0);
    client.publish(publishTopicMotor, message, true);
  }
}

// Function to measure distance using the ultrasonic sensor
long measureDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // 30ms timeout — max range ~400cm = ~23ms pulse
  long duration = pulseIn(echoPin, HIGH, 30000);
  return duration * 0.034 / 2;
}

int measureDistanceAverage() {
  long total = 0;
  int validReadings = 0;
  for (int i = 0; i < 3; i++) {
    long dist = measureDistance();
    if (dist > 0) {
      total += dist;
      validReadings++;
    }
    delay(10);
  }
  if (validReadings == 0) return 0;
  return total / validReadings;
}

// Function to connect to Wi-Fi
void connectToWiFi() {
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    blinkLED(500);
    showConnectingPattern();
    Serial.print(".");

    if (millis() - startTime > 60000) {
      Serial.println("\nFailed to connect to Wi-Fi within 60 seconds. Entering offline mode.");
      offlineMode = true;
      display.clear();
      return;
    }
  }

  Serial.println("\nWi-Fi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(ledPin, HIGH); // LED OFF after connection
}

// Function to connect to the MQTT broker
void connectToMQTT() {
  unsigned long startTime = millis();
  while (!client.connected()) {
    // Timeout after 15 seconds — will retry next loop
    if (millis() - startTime > 15000) {
      Serial.println("MQTT connection timeout, will retry later.");
      return;
    }

    Serial.print("Connecting to MQTT...");

    if (client.connect(WS_DEVICE_CLIENTID, MQTT_USER, MQTT_PASSWORD, publishTopic, 1, true, "offline")) {
      Serial.println("connected!");
      client.subscribe(subscribeTopicMinDist);
      client.subscribe(subscribeTopicMaxDist);
      client.subscribe(subscribeTopicDisplay);
      client.subscribe(subscribeTopicBuzzer);
      client.subscribe(subscribeTopicMotor);
      Serial.println("Subscribed to all topics including motor control");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds...");
      delay(5000);
    }
  }
  digitalWrite(ledPin, HIGH); // LED OFF after connection
}

// Function to blink the LED at a specified frequency
void blinkLED(int frequency) {
  digitalWrite(ledPin, LOW); // Turn on the LED
  delay(frequency / 2);      // Wait for half the frequency duration
  digitalWrite(ledPin, HIGH); // Turn off the LED
  delay(frequency / 2);      // Wait for the other half
}

// Function to show connecting pattern on the TM1637 display
void showConnectingPattern() {
  static int patternIndex = 0;
  int patterns[] = {0b00000000, 0b01010101, 0b10101010, 0b11111111}; // Example patterns
  display.setSegments((uint8_t*)&patterns[patternIndex], 4, 0);
  patternIndex = (patternIndex + 1) % 4; // Cycle through patterns
}

// Callback function to handle incoming MQTT messages
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);

  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';

  // Show value on display FIRST for all control topics
  if (
    strcmp(topic, subscribeTopicMinDist) == 0 ||
    strcmp(topic, subscribeTopicMaxDist) == 0 ||
    strcmp(topic, subscribeTopicDisplay) == 0 ||
    strcmp(topic, subscribeTopicBuzzer) == 0 ||
    strcmp(topic, subscribeTopicMotor) == 0
  ) {
    int displayValue = atoi(message);
    if (displayValue >= 0 && displayValue <= 9999) {
      display.showNumberDec(displayValue, false);
    } else {
      display.showNumberDecEx(0, 0b01000000, false);
    }
    displayShowTime = millis();
    displayTemporary = true;
    Serial.print("Display showing: ");
    Serial.println(displayValue);
  }

  if (strcmp(topic, subscribeTopicMotor) == 0) {
    int newMotorState = atoi(message);
    if (newMotorState == 1) {
      controlMotor(true);
      Serial.println("Motor turned ON via MQTT");
    } else if (newMotorState == 0) {
      controlMotor(false);
      Serial.println("Motor turned OFF via MQTT");
    } else {
      Serial.println("Invalid motor state received.");
    }
  }

  if (strcmp(topic, subscribeTopicMinDist) == 0) {
    int newMinDistance = atoi(message);
    if (newMinDistance > 0) {
      minDistance = newMinDistance;
      Serial.print("Updated minDistance to: ");
      Serial.println(minDistance);
    } else {
      Serial.println("Invalid minimum distance value received.");
    }
  }

  if (strcmp(topic, subscribeTopicBuzzer) == 0) {
    int newBuzzerState = atoi(message);
    if (newBuzzerState == 1) {
      buzzerSound = true;
      Serial.println("Buzzer enabled.");
    } else if (newBuzzerState == 0) {
      buzzerSound = false;
      Serial.println("Buzzer disabled.");
    } else {
      Serial.println("Invalid buzzer state received.");
    }
  }

  if (strcmp(topic, subscribeTopicMaxDist) == 0) {
    int newMaxDistance = atoi(message);
    if (newMaxDistance > minDistance) {
      maxDistance = newMaxDistance;
      Serial.print("Updated maxDistance to: ");
      Serial.println(maxDistance);
    } else {
      Serial.println("Invalid maximum distance value received.");
    }
  }

  if (strcmp(topic, subscribeTopicDisplay) == 0) {
    int newDisplayState = atoi(message);
    if (newDisplayState == 1) {
      displayEnabled = true;
      Serial.println("Display enabled.");
    } else if (newDisplayState == 0) {
      displayEnabled = false;
      display.clear();
      Serial.println("Display disabled.");
    } else {
      Serial.println("Invalid display state received.");
    }
  }
}
