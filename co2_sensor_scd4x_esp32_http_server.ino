#include <SensirionI2cScd4x.h>

#include "secrets.h"

#define LED_PIN 13
#define SDA_PIN 3
#define SCL_PIN 4

// Task scheduler
#include <TaskScheduler.h>
void readSensorCallback();
Task readSensorTask(5000, -1, &readSensorCallback);  // Read sensor every 5 seconds
Scheduler runner;

#include <Arduino.h>
#include <Wire.h>

uint16_t error;
char errorMessage[256];

SensirionI2cScd4x sensor;

// SCD4X
uint16_t co2;
float temperature;
float humidity;
float voltage;

// Task callback
void readSensorCallback() {
    // Read the voltage (ESP32-C3 plugged into laptop 4.2V reads 3342)
    // int sensorValue = analogRead(A2);
    // printToSerial((String)"Analog read: " + sensorValue);
    // voltage = sensorValue * (4.2 / 3342.0);

    // Read the SCD4X CO2 sensor
    error = sensor.readMeasurement(co2, temperature, humidity);
    if (error) {
        printToSerial("Error trying to execute readMeasurement(): ");
        errorToString(error, errorMessage, 256);
        printToSerial(errorMessage);
    } else if (co2 == 0) {
        printToSerial("Invalid sample detected, skipping.");
    } else {
        printToSerial((String)"Co2: " + co2);
        printToSerial((String)"Temperature: " + temperature);
        printToSerial((String)"Humidity: " + humidity);
        printToSerial((String)"Voltage: " + voltage);
        printToSerial("");
    }

    // Pulse blue LED
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(LED_PIN, LOW);
}

void printToSerial(String message) {
  // Check for Serial to avoid the ESP32-C3 hanging attempting to Serial.print
  if (Serial) {
    Serial.println(message);
  }
}

// WiFi init
#include <WiFi.h>
char* ssid     = SECRET_SSID;
char* password = SECRET_PASSWORD;
WiFiServer server(80);

void setup() {

    Serial.begin(115200);
    delay(100);
    printToSerial("Hello");

    pinMode(LED_PIN, OUTPUT);

    // Sensor setup
    Wire.begin(SCL_PIN, SDA_PIN);

    uint16_t error;
    char errorMessage[256];

    sensor.begin(Wire, 0x62);

    // stop potentially previously started measurement
    error = sensor.stopPeriodicMeasurement();
    if (error) {
      printToSerial("Error trying to execute stopPeriodicMeasurement(): ");
      errorToString(error, errorMessage, 256);
      printToSerial(errorMessage);
    }

    // SCD4X
    error = sensor.startPeriodicMeasurement();

    if (error) {
      printToSerial("Error trying to execute startPeriodicMeasurement(): ");
      errorToString(error, errorMessage, 256);
      printToSerial(errorMessage);
    }

    printToSerial("Waiting for first measurement... (5 sec)");

    // Setup read sensor task
    readSensorTask.enable();
    runner.addTask(readSensorTask);
    runner.enableAll();

    // WiFi setup
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);

    delay(10);

    // We start by connecting to a WiFi network
    printToSerial((String)"Connecting to " + ssid);

    WiFi.begin(ssid, password);

    // Wait for a WiFi connection for up to 10 seconds
    for (int i = 0; i < 10; i++) {
      if (WiFi.status() != WL_CONNECTED) {
        digitalWrite(LED_PIN, HIGH);
        delay(500);
        digitalWrite(LED_PIN, LOW);
        printToSerial(".");
        delay(500);
      } else {
        printToSerial("WiFi connected.");
        printToSerial("IP address: ");
        printToSerial((String)WiFi.localIP());

        digitalWrite(LED_PIN, HIGH);
        delay(1000);
        digitalWrite(LED_PIN, LOW);
        break;
      }
    }
    server.begin();
}

void loop() {
  runner.execute();

  // WiFi server
  WiFiClient client = server.available();   // listen for incoming clients

  if (client) {                             // if you get a client,
    printToSerial("New Client.");           // print a message out the serial port
    printToSerial("");
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:application/json; charset=UTF-8");
            client.println();

            // Pulse the LED to show a connection has been made
            digitalWrite(LED_PIN, HIGH);

            // Send JSON data
            client.print((String)"{\"temperature\": "+temperature+",\"humidity\":"+humidity+",\"co2\": "+co2+"}\n");

            digitalWrite(LED_PIN, LOW);

            // The HTTP response ends with another blank line:
            client.print("\n");
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // close the connection:
    client.stop();
    printToSerial("Client Disconnected.");
  }
}
