#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <EthernetClient.h>
#include <Dns.h>
#include <smtp.h>
#include <SoftwareSerial.h>

// Email settings
const char* smtpServer = "your.smtp.server";
const int smtpPort = 587;
const char* emailUser = "your.email@example.com";
const char* emailPassword = "your_password";
const char* recipients[] = {"recipient1@example.com", "recipient2@example.com"};
const int recipientCount = 2;

// Turbo pump temperature monitoring
SoftwareSerial turboPumpSerial(10, 11); // RX, TX
String turboPumpCommand = "?V859";
float pumpMotorTemp, pumpControllerTemp;

// Define pins
#define TOXIC_GAS_PIN A0
#define AMBIENT_TEMP_PIN A1
#define WATER_TEMP_PIN A2

// Thresholds
const float toxicGasThreshold = 1.5; // Example threshold for analog value
const float ambientTempThreshold = 35.0;
const float waterTempThresholdLow = 10.0;
const float waterTempThresholdHigh = 20.0;

EthernetClient client;
BME280 bme; // Create an instance of the sensor

void setup() {
  Serial.begin(9600);
  turboPumpSerial.begin(9600);
  Wire.begin();

  if (!bme.begin()) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  // Initialize Ethernet
  byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    while (1);
  }
  delay(1000);
  Serial.println("Ethernet initialized");

  // Send initial command to turbo pump to get temperature readings
  sendTurboPumpCommand(turboPumpCommand);
}

void loop() {
  // Poll for conditions
  checkToxicGas();
  checkAmbientTemp();
  checkWaterTemp();
  checkTurboPumpTemp();
  
  delay(10000); // Adjust the delay as necessary
}

void sendTurboPumpCommand(String command) {
  turboPumpSerial.print(command + "\r");
}

void checkTurboPumpTemp() {
  while (turboPumpSerial.available()) {
    String response = turboPumpSerial.readStringUntil('\n');
    parseTurboPumpTemp(response);
  }

  if (pumpMotorTemp > waterTempThresholdHigh || pumpControllerTemp > waterTempThresholdHigh) {
    sendEmail("Warning! ambient temperature is above 35°C you must water-cool the pump");
  }
}

void parseTurboPumpTemp(String response) {
  int separator = response.indexOf(';');
  if (separator != -1) {
    pumpMotorTemp = response.substring(0, separator).toFloat();
    pumpControllerTemp = response.substring(separator + 1).toFloat();
  }
}

void checkToxicGas() {
  float gasLevel = analogRead(TOXIC_GAS_PIN);
  if (gasLevel > toxicGasThreshold) {
    sendEmail("Warning! there is toxic gas leak in the lab");
  }
}

void checkAmbientTemp() {
  float ambientTemp = bme.readTemperature();
  if (ambientTemp > ambientTempThreshold) {
    sendEmail("Warning! ambient temperature is above 35°C you must water-cool the pump");
  }
}

void checkWaterTemp() {
  float waterTemp = analogRead(WATER_TEMP_PIN);
  if (waterTemp < waterTempThresholdLow || waterTemp > waterTempThresholdHigh) {
    sendEmail("Warning! ambient temperature is above 35°C you must water-cool the pump");
  }
}

void sendEmail(String message) {
  for (int i = 0; i < recipientCount; i++) {
    SMTP email;
    email.setServer(smtpServer, smtpPort);
    email.setLogin(emailUser, emailPassword);
    email.setSender(emailUser);
    email.setRecipient(recipients[i]);
    email.setSubject("Lab Alert");
    email.setMessage(message);

    if (email.send()) {
      Serial.println("Email sent successfully");
    } else {
      Serial.println("Error sending email");
    }
  }
}
