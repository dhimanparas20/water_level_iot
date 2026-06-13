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
int minDistance = 7; // Default minimum distance (water full level)
int maxDistance = 65; // Default maximum distance (water low level)

// Create Wi-Fi and MQTT clients
WiFiClient espClient;
PubSubClient client(espClient);

// Function prototypes
void connectToWiFi();
void connectToMQTT();
long measureDistance();
void blinkLED(int frequency);
void mqttCallback(char* topic, byte* payload, unsigned int length);
void showConnectingPattern();
void keepPowerBankActive();
void calculateAndPublishPercentage();
void controlMotor(bool state);

// Declare the static variable globally so it persists across loop iterations
static unsigned long lastActivityTime = 0;

// For non-blocking loop timing
unsigned long lastMeasurementTime = 0;

void setup() {
  // Initialize serial communication
  Serial.begin(115200);

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
  digitalWrite(motorRelayPin, LOW); // Turn off the motor initially (assuming LOW = OFF)

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

    // Measure the distance
    long duration = measureDistance();
    int distance = duration * 0.034 / 2; // Convert duration to distance in cm

    // Calculate the water level percentage
    int tankHeight = maxDistance - minDistance; // Calculate the tank height
    int waterLevel = tankHeight - (distance - minDistance); // Calculate water level
    int percentage = (waterLevel * 100) / tankHeight; // Calculate percentage

    // Ensure percentage is within 0-100 range
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;

    // Check if the distance is out of range (for buzzer)
    if (distance < minDistance || distance > maxDistance) {
      if (buzzerSound) { // Only buzz if enabled
        for (int i = 0; i < 3; i++) { // Beep 3 times
          digitalWrite(buzzerPin, LOW); // Turn on the buzzer
          delay(150);                   // Buzzer on for 150ms
          digitalWrite(buzzerPin, HIGH); // Turn off the buzzer
          delay(150);                   // Buzzer off for 150ms
        }
        // Reduce the delay to 800 ms while the buzzer is active
        del_ay = 800;
      }
    } 
    else {
      // Ensure the buzzer is off
      digitalWrite(buzzerPin, HIGH);
      // Restore the delay when the distance is within range
      del_ay = currentDelay;
    }

    // Only proceed if the distance has changed
    if (distance != previousDistance) {
      // Update the previous distance
      previousDistance = distance;

      // Print the distance and percentage to the Serial Monitor in a single line
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
        client.publish(publishTopic, message, true); // Retain the message with QoS 1
      }

      // Display the percentage on the TM1637 4-segment display (if enabled)
      if (displayEnabled) {
        if (percentage < 0 || percentage > 9999) {
          // If the percentage is out of range, display "----"
          display.showNumberDec(9999, false);
        } else {
          // Display the percentage normally
          display.showNumberDec(percentage, false);
        }
      } else {
        // Turn off the display
        display.clear();
      }
    }
  }
  // No delay here! Loop runs fast for MQTT responsiveness.
}

// Function to control motor
void controlMotor(bool state) {
  motorState = state;
  digitalWrite(motorRelayPin, state ? HIGH : LOW); // Assuming HIGH = ON, LOW = OFF
  Serial.print("Motor turned ");
  Serial.println(state ? "ON" : "OFF");
}

// Function to measure distance using the ultrasonic sensor
long measureDistance() {
  // Send a 10-microsecond pulse to the Trig pin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read the Echo pin and calculate the duration
  return pulseIn(echoPin, HIGH);
}

// Function to calculate and publish water level percentage
void calculateAndPublishPercentage() {
  int tankHeight = maxDistance - minDistance; // Calculate the tank height
  int waterLevel = tankHeight - (previousDistance - minDistance); // Calculate water level
  int percentage = (waterLevel * 100) / tankHeight; // Calculate percentage

  // Ensure percentage is within 0-100 range
  if (percentage < 0) percentage = 0;
  if (percentage > 100) percentage = 100;

  // Publish the percentage to the MQTT topic
  if (!offlineMode) {
    char message[10];
    snprintf(message, sizeof(message), "%d", percentage);
    client.publish(publishTopic, message, true); // Retain the message with QoS 1
  }

  // Print the percentage to the Serial Monitor
  Serial.print("Water Level Percentage: ");
  Serial.print(percentage);
  Serial.println("%");
}

// Function to connect to Wi-Fi
void connectToWiFi() {
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  WiFi.setSleepMode(WIFI_LIGHT_SLEEP);

  unsigned long startTime = millis(); // Record the start time
  while (WiFi.status() != WL_CONNECTED) {
    blinkLED(500); // Blink LED every 500ms while connecting to Wi-Fi
    showConnectingPattern(); // Show connecting pattern on the display
    Serial.print(".");

    // Check if 60 seconds have passed
    if (millis() - startTime > 60000) {
      Serial.println("\nFailed to connect to Wi-Fi within 60 seconds. Entering offline mode.");
      offlineMode = true; // Enable offline mode
      display.clear();    // Clear the display
      return;             // Exit the function
    }
  }

  Serial.println("\nWi-Fi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(ledPin, HIGH); // Turn off the LED after connection
}

// Function to connect to the MQTT broker
void connectToMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    blinkLED(200); // Blink LED every 200ms while connecting to MQTT
    showConnectingPattern(); // Show connecting pattern on the display

    // Set Last Will and Testament (LWT) during the connect call
    if (client.connect(WS_DEVICE_CLIENTID, MQTT_USER, MQTT_PASSWORD, publishTopic, 1, true, "offline")) {
      Serial.println("connected!");
      client.subscribe(subscribeTopicMinDist); // Subscribe to minimum distance updates
      client.subscribe(subscribeTopicMaxDist); // Subscribe to maximum distance updates
      client.subscribe(subscribeTopicDisplay); // Subscribe to display control
      client.subscribe(subscribeTopicBuzzer);  // Subscribe to buzzer control
      client.subscribe(subscribeTopicMotor);   // Subscribe to motor control
      Serial.println("Subscribed to all topics including motor control");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds...");
      delay(5000);
    }
  }
  digitalWrite(ledPin, HIGH); // Turn off the LED after connection
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

  // Convert the payload to a null-terminated string
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0'; // Null-terminate the string

  // Handle motor control
  if (String(topic) == subscribeTopicMotor) {
    int newMotorState = atoi(message); // Convert the message to an integer
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

  // Display on TM1637 for all control topics
  if (
    String(topic) == subscribeTopicMinDist ||
    String(topic) == subscribeTopicMaxDist ||
    String(topic) == subscribeTopicDisplay ||
    String(topic) == subscribeTopicBuzzer ||
    String(topic) == subscribeTopicMotor
  ) {
    int displayValue = atoi(message); // Convert the message to an integer
    if (displayValue >= 0 && displayValue <= 9999) {
      display.showNumberDec(displayValue, false); // Show the value on the display
      delay(500); // Keep the value displayed for 0.5 seconds
    } else {
      // If the value is invalid, show "----" temporarily
      display.showNumberDecEx(0, 0b01000000, false); // Show "----"
      delay(500);
    }
  }

  // Handle minimum distance updates
  if (String(topic) == subscribeTopicMinDist) {
    int newMinDistance = atoi(message); // Convert the message to an integer
    if (newMinDistance > 0) { // Ensure the new minimum distance is valid
      minDistance = newMinDistance;
      Serial.print("Updated minDistance to: ");
      Serial.println(minDistance);
    } else {
      Serial.println("Invalid minimum distance value received.");
    }
  }

  // Handle buzzer control
  if (String(topic) == subscribeTopicBuzzer) {
    int newBuzzerState = atoi(message); // Convert the message to an integer
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

  // Handle maximum distance updates
  if (String(topic) == subscribeTopicMaxDist) {
    int newMaxDistance = atoi(message); // Convert the message to an integer
    if (newMaxDistance > minDistance) { // Ensure the new maximum distance is valid
      maxDistance = newMaxDistance;
      Serial.print("Updated maxDistance to: ");
      Serial.println(maxDistance);
    } else {
      Serial.println("Invalid maximum distance value received.");
    }
  }

  // Handle display control
  if (String(topic) == subscribeTopicDisplay) {
    int newDisplayState = atoi(message); // Convert the message to an integer
    if (newDisplayState == 1) {
      displayEnabled = true;
      Serial.println("Display enabled.");
    } else if (newDisplayState == 0) {
      displayEnabled = false;
      display.clear(); // Turn off the display
      Serial.println("Display disabled.");
    } else {
      Serial.println("Invalid display state received.");
    }
  }
}

void keepPowerBankActive() {
  // Toggle a GPIO pin to simulate activity
  digitalWrite(ledPin, LOW); // Turn on the built-in LED
  delay(100);
  digitalWrite(ledPin, HIGH); // Turn off the LED
}
