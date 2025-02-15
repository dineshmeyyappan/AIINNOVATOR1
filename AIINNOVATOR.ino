//updates:
//camip webpage bug removed
//colour sensor bug removed,
//dynamic ip assignation with ssid entry
//touch sensor removed from command and webpage
//last updated 31/1/25.


#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <WiFi.h>
#include <WebServer.h>
#include <NewPing.h>
#include <FastLED.h>
#include <ESPmDNS.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <esp_sleep.h>

#define SLEEP_DURATION 600000000  // Sleep for 60 seconds
WebServer server(80);
String command;

const char *ssid = "AILITE-1919";  // Update this SSID as needed
const char *password = "123456789";

// Variables for IP configuration
IPAddress local_IP;
IPAddress gateway;
IPAddress subnet(255, 255, 255, 0);

#define motorpinLF1 14
#define motorpinLF2 16
#define motorpinLB2 33
#define motorpinLB1 32
#define motorpinRF1 19
#define motorpinRF2 17
#define motorpinRB1 25
#define motorpinRB2 26

#define encoderPinA1 36
#define encoderPinA2 35
#define encoderPinA3 34
#define encoderPinA4 27

int leftIR;
int rightIR;
int ultrasonic;

const int motorEnablePin = 13;
int receivedSpeed_left = 200;
int receivedSpeed_right = 200;

const int batteryPin = 39;
const float R1 = 150.0;
const float R2 = 560.0;

int rawValue;
float voltage;
float actualVoltage;
const float MIN_BATTERY_VOLTAGE = 3.4;
bool baterrystatus = true;

const int usirPin1 = 23;
const int usirPin2 = 2;

#define TRIGGER_PIN 5
#define ECHO_PIN 18
#define MAX_DISTANCE 200
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);
float distance;
#define touch_pin 12
#define LED_PIN_2 15
#define NUM_LEDS 1
CRGB leds1[NUM_LEDS];
CRGB leds2[NUM_LEDS];
int numClients = 0;

#define TCS3414CS_ADDRESS 0x29                                                                // ColorSensor address 0x29
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_60X);  // Initializing ColorSensor

int left_US_ir_Value;
int right_US_ir_Value;

// Declare variables for motor speeds
int lf_speed = 200;
int lb_speed = 200;
int rf_speed = 200;
int rb_speed = 200;

// Define PWM resolution and frequency
const int pwmResolution = 8;
const int pwmFrequency = 5000;

unsigned long lowBatteryStartTime = 0;
const unsigned long lowBatteryDuration = 500;  // 5 seconds of low voltage to trigger the condition

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  FastLED.addLeds<NEOPIXEL, LED_PIN_2>(leds2, NUM_LEDS);
  FastLED.clear();
  FastLED.show();

  // Calculate and configure the dynamic IP and gateway based on the SSID
  if (!setupNetworkConfig(ssid)) {
    Serial.println("Invalid SSID format! Expected format: AILITE-xxxx");
    while (1) {
      blinkRed();
    }
  }

  // Reading battery voltage
  rawValue = analogRead(batteryPin);
  voltage = (rawValue - 2800) * (4.1 - 3.07) / (4095 - 2800) + 3.07;
  if (voltage < MIN_BATTERY_VOLTAGE) {
    baterrystatus = false;
    blinkRed();
    // Put the ESP32 into deep sleep mode if the battery is low
    Serial.println("Battery low. Going to sleep to save power.");
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION);  // Set wakeup timer
    esp_deep_sleep_start();                         // Put ESP32 into deep sleep mode
  } else {
    baterrystatus = true;
  }

  Wire.begin(21, 22);  // SDA, SCL pins for ESP32

  if (tcs.begin()) {
    Serial.println("Found sensor");
  } else {
    Serial.println("No TCS34725 found ... check your connections");
  }

  Serial.println("Firmware  - AI-LITE");
  delay(100);

  // Continue with the rest of the setup if battery is okay
  if (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
    Serial.println("AP Config Failed");
    return;
  }

  Serial.println("Setting AP (Access Point)…");
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  if (!MDNS.begin(ssid)) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.println("mDNS responder started");
  }

  server.on("/", HTTP_handleRoot);
  server.onNotFound(HTTP_handleRoot);
  server.begin();

  pinMode(batteryPin, INPUT);
  pinMode(touch_pin, INPUT);

  pinMode(motorEnablePin, OUTPUT);
  digitalWrite(motorEnablePin, HIGH);

  pinMode(motorpinLF1, OUTPUT);
  pinMode(motorpinLF2, OUTPUT);
  pinMode(motorpinLB1, OUTPUT);
  pinMode(motorpinLB2, OUTPUT);
  pinMode(motorpinRF1, OUTPUT);
  pinMode(motorpinRF2, OUTPUT);
  pinMode(motorpinRB1, OUTPUT);
  pinMode(motorpinRB2, OUTPUT);

  ledcSetup(0, pwmFrequency, pwmResolution);
  ledcSetup(1, pwmFrequency, pwmResolution);
  ledcSetup(2, pwmFrequency, pwmResolution);
  ledcSetup(3, pwmFrequency, pwmResolution);
  ledcSetup(4, pwmFrequency, pwmResolution);
  ledcSetup(5, pwmFrequency, pwmResolution);
  ledcSetup(6, pwmFrequency, pwmResolution);
  ledcSetup(7, pwmFrequency, pwmResolution);

  ledcAttachPin(motorpinLF1, 0);
  ledcAttachPin(motorpinLF2, 1);
  ledcAttachPin(motorpinLB1, 2);
  ledcAttachPin(motorpinLB2, 3);
  ledcAttachPin(motorpinRF1, 4);
  ledcAttachPin(motorpinRF2, 5);
  ledcAttachPin(motorpinRB1, 6);
  ledcAttachPin(motorpinRB2, 7);
}

void loop() {

  // Read battery voltage
  rawValue = analogRead(batteryPin);
  voltage = (rawValue - 2800) * (4.1 - 3.07) / (4095 - 2800) + 3.07;
  //Serial.println(voltage);

  // Check battery status
  if (voltage < 3.40) {
    if (lowBatteryStartTime == 0) {
      lowBatteryStartTime = millis();  // Start the timer for low battery
    } else if (millis() - lowBatteryStartTime > lowBatteryDuration) {
      baterrystatus = false;  // If voltage stays low for the duration, mark battery as low
    }
  } else {
    lowBatteryStartTime = 0;  // Reset timer if battery voltage is okay
    baterrystatus = true;
  }

  // If battery is low and baterrystatus is false, save power by entering deep sleep
  if (!baterrystatus) {
    // Stop motors and disconnect all clients
    digitalWrite(motorEnablePin, LOW);

    // Indicate low battery with a red blink before entering deep sleep
    Serial.println("Low battery, indicating with red LED and entering deep sleep.");
    blinkRed();  // Blink or show the red LED

    if (WiFi.softAPgetStationNum() > 0) {
      WiFi.softAPdisconnect(true);  // Disconnect all clients and stop AP
      server.stop();                // Stop the server to prevent further connections
      Serial.println("AP stopped due to low battery.");
    }

    // Enter deep sleep after showing red LED
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION);  // Set wakeup timer
    esp_deep_sleep_start();                         // Put ESP32 into deep sleep mode

  } else {
    // If the battery status is good, allow clients to connect
    numClients = WiFi.softAPgetStationNum();  // Get the number of connected clients

    // Allow the server to handle client requests
    server.handleClient();

    // LED behavior based on client connections
    if (numClients == 0) {
      blinkcyan();
    } else if (numClients == 1) {
      setConstantGreenLed2();
    } else if (numClients == 2) {
      setConstantmegentaLed2();
    }
  }
}

bool setupNetworkConfig(const char *ssid) {
  String ssidStr = String(ssid);

  // Ensure SSID matches the expected format "AILITE-xxxx"
  if (!ssidStr.startsWith("AILITE-") || ssidStr.length() != 11) {
    return false;
  }

  // Extract the serial number from the SSID (last 4 characters)
  String serialNumber = ssidStr.substring(7);  // Get "0207" from "AILITE-0207"

  // Third Octet: Reverse the first two digits of the serial number
  String firstTwoDigits = serialNumber.substring(0, 2);  // Get "02"
  String reversedThirdOctet = String(firstTwoDigits[1]) + String(firstTwoDigits[0]);  // Reverse "02" → "20"
  byte thirdOctet = reversedThirdOctet.toInt();          // Convert to integer (20)

  // Fourth Octet: Reverse the last two digits of the serial number
  String lastTwoDigits = serialNumber.substring(2);      // Get "07"
  String reversedFourthOctet = String(lastTwoDigits[1]) + String(lastTwoDigits[0]);  // Reverse "07" → "70"
  byte fourthOctet = reversedFourthOctet.toInt();        // Convert to integer (70)

  // Set the static IP and gateway
  local_IP = IPAddress(192, 168, fourthOctet, thirdOctet);  // e.g., 192.168.70.20
  gateway = IPAddress(192, 168, fourthOctet, 1);             // e.g., 192.168.70.1

  // Print the configuration
  Serial.print("AP IP address: ");
  Serial.println(local_IP);

  Serial.print("Gateway address: ");
  Serial.println(gateway);

  return true;
}




void Forward() {

  ledcWrite(0, lf_speed);
  ledcWrite(1, 0);
  ledcWrite(2, lb_speed);
  ledcWrite(3, 0);
  ledcWrite(4, rf_speed);
  ledcWrite(5, 0);
  ledcWrite(6, rb_speed);
  ledcWrite(7, 0);
  server.send(200, "text/plain", "Forward command executed");
}

void Backward() {

  ledcWrite(0, 0);
  ledcWrite(1, lf_speed);
  ledcWrite(2, 0);
  ledcWrite(3, lb_speed);
  ledcWrite(4, 0);
  ledcWrite(5, rf_speed);
  ledcWrite(6, 0);
  ledcWrite(7, rb_speed);
  server.send(200, "text/plain", "Backward command executed");
}

void Left() {

  ledcWrite(0, 0);
  ledcWrite(1, lf_speed);
  ledcWrite(2, 0);
  ledcWrite(3, lb_speed);
  ledcWrite(4, rf_speed);
  ledcWrite(5, 0);
  ledcWrite(6, rb_speed);
  ledcWrite(7, 0);
  server.send(200, "text/plain", "Left command executed");
}

void Right() {

  ledcWrite(0, lf_speed);
  ledcWrite(1, 0);
  ledcWrite(2, lb_speed);
  ledcWrite(3, 0);
  ledcWrite(4, 0);
  ledcWrite(5, rf_speed);
  ledcWrite(6, 0);
  ledcWrite(7, rb_speed);
  server.send(200, "text/plain", "Right command executed");
}

void Stop() {
  ledcWrite(0, 0);
  ledcWrite(1, 0);
  ledcWrite(2, 0);
  ledcWrite(3, 0);
  ledcWrite(4, 0);
  ledcWrite(5, 0);
  ledcWrite(6, 0);
  ledcWrite(7, 0);
}

void blinkRed() {
  for (int i = 0; i < 10; i++) {  // Blink red 10 times

    setRedLed2();
    delay(300);  // 500ms on

    turnOffLed2();
    delay(300);  // 500ms off
  }

  // After blinking, set LEDs to constant red
  setRedLed2();
}


// void blinkBlue() {
//   for (int i = 0; i < 1; i++) {  // Blink blue 5 times

//     setBlueLed2();
//     delay(500);  // 500ms on

//     turnOffLed2();
//     delay(500);  // 500ms off
//   }
// }


void blinkcyan() {
  for (int i = 0; i < 1; i++) {  // Blink blue 5 times
    // setmagentaLed1();
    setcyanLed2();
    delay(500);  // 500ms on
    // turnOffLed1();
    turnOffLed2();
    delay(500);  // 500ms off
  }
}


void setConstantGreenLed2() {
  leds2[0] = CRGB::Green;
  FastLED.show();
}


void setConstantmegentaLed2() {
  leds2[0] = CRGB(255, 0, 255);
  FastLED.show();
}


void setRedLed2() {
  leds2[0] = CRGB::Red;
  FastLED.show();
}

void setcyanLed2() {
  leds2[0] = CRGB(0, 255, 255);
  FastLED.show();
}

void turnOffLed2() {
  leds2[0] = CRGB::Black;
  FastLED.show();
}

float readVoltage() {
  rawValue = analogRead(batteryPin);
  voltage = (float)rawValue / 4095.0 * 3.4;
  actualVoltage = voltage * (R1 + R2) / R2;
  Serial.print("Voltage: ");
  Serial.println(actualVoltage);
  server.send(200, "text/plain", String("Battery Voltage ") + String(actualVoltage));
  return actualVoltage;
}

void get_ir_bat() {
  right_US_ir_Value = digitalRead(usirPin2);
  left_US_ir_Value = digitalRead(usirPin1);
  rawValue = analogRead(batteryPin);
  voltage = (float)rawValue / 4095.0 * 3.4;
  actualVoltage = voltage * (R1 + R2) / R2;
  server.send(200, "text/plain", String(right_US_ir_Value) + " " + String(left_US_ir_Value) + " " + String(actualVoltage));
}
void get_ir_us() {
  right_US_ir_Value = digitalRead(usirPin2);
  left_US_ir_Value = digitalRead(usirPin1);
  distance = sonar.ping_cm();
  server.send(200, "text/plain", String(right_US_ir_Value) + " " + String(left_US_ir_Value) + " " + String(distance));
}
void get_ir() {
  right_US_ir_Value = digitalRead(usirPin2);
  left_US_ir_Value = digitalRead(usirPin1);
  server.send(200, "text/plain", String(right_US_ir_Value) + " " + String(left_US_ir_Value));
}
void print_right_us_ir() {
  right_US_ir_Value = digitalRead(usirPin2);
  Serial.print("Right US IR Value: ");
  Serial.println(right_US_ir_Value);
  //delay(1000);  // Add a delay for stability during debugging
  server.send(200, "text/plain", String("Right US IR Value: ") + String(right_US_ir_Value));
}
void print_left_us_ir() {
  left_US_ir_Value = digitalRead(usirPin1);
  Serial.print("Left US IR Value: ");
  Serial.println(left_US_ir_Value);
  // delay(1000);  // Add a delay for stability during debugging
  server.send(200, "text/plain", String("Left US IR Value: ") + String(left_US_ir_Value));
}
void print_ultrasonic() {
  delay(50);
  distance = sonar.ping_cm();
  int int_distance = int(distance);  // Convert float to int
  Serial.print("Ping: ");
  Serial.print(int_distance);
  Serial.println("cm");
  server.send(200, "text/plain", String(int_distance));
}
void touch() {
  int touchValue = touchRead(touch_pin);
  if (touchValue > 0) {
    server.send(200, "text/plain", String("Not Detected"));
  } else {
    server.send(200, "text/plain", String("Detected"));
  }
}
String ColorSensor() {
    if (tcs.begin()) {
    Serial.println("Found sensor");
  } else {
    Serial.println("No TCS34725 found ... check your connections");
  }
  uint16_t r, g, b, c, colorTemp;
  tcs.getRawData(&r, &g, &b, &c);
  colorTemp = tcs.calculateColorTemperature_dn40(r, g, b, c);

  // Determine the color based on RGB values
  String detectedColor;
  if (r > g && r > b) {
    detectedColor = "Red";
  } else if (g > r && g > b && c < 5000) {
    detectedColor = "Green";
  } else if (c > 5300) {
    detectedColor = "Blue";
  } else {
    detectedColor = "Unknown";
  }

  // Create a string with the color data
  String colorData = "Detected Color: " + detectedColor + " - R: " + String(r) + ", G: " + String(g) + ", B: " + String(b) + ", C: " + String(c) + ", Color Temp: " + String(colorTemp);

  // Debug output to the serial monitor
  Serial.println(colorData);

  return colorData;
}
void HTTP_handleRoot(void) {
  if (server.hasArg("cmd")) {
    String command = server.arg("cmd");
    Serial.print("Received command: ");
    Serial.println(command);

    if (command.startsWith("f(") || command.startsWith("b(") || command.startsWith("l(") || command.startsWith("r(")) {
      char direction = command.charAt(0);
      int openParenIndex = command.indexOf('(');
      int closeParenIndex = command.indexOf(')');
      if (openParenIndex != -1 && closeParenIndex != -1 && openParenIndex < closeParenIndex - 1) {
        String delayString = command.substring(openParenIndex + 1, closeParenIndex);
        int delayValue = delayString.toInt();
        if (delayValue > 0) {
          if (direction == 'f') {
            Forward();
          } else if (direction == 'b') {
            Backward();
          } else if (direction == 'l') {
            Left();
          } else if (direction == 'r') {
            Right();
          }
          delay(delayValue);
          Stop();
          Serial.print("Command executed: ");
          Serial.println(command);
          server.send(200, "text/plain", "Command executed successfully");
        } else {
          Serial.println("Invalid delay value");
          server.send(400, "text/plain", "Invalid delay value");
        }
      } else {
        Serial.println("Invalid command format");
        server.send(400, "text/plain", "Invalid command format");
      }
    } else if (command == "s") {
      Stop();
      Serial.println("Stop command executed");
      server.send(200, "text/plain", "Stop command executed");
    } else if (command == "f") {
      Forward();
    } else if (command == "b") {
      Backward();
    } else if (command == "l") {
      Left();
    } else if (command == "r") {
      Right();
    } else if (command == "USRIR") {
      print_right_us_ir();
    } else if (command == "USLIR") {
      print_left_us_ir();
    } else if (command == "US") {
      print_ultrasonic();
    } else if (command == "V") {
      readVoltage();
    } else if (command == "getirbat") {
      get_ir_bat();
    } else if (command == "getirus") {
      get_ir_us();
    } else if (command == "t") {
      touch();
    } else if (command == "getir") {
      get_ir();
    } else if (command == "cs") {
      String frontColor = ColorSensor();
      server.send(200, "text/plain", frontColor);
    } else if (command.startsWith("lf_speed=")) {
      String speedValue = command.substring(9);
      lf_speed = speedValue.toInt();
      // set LF motor speed here
      server.send(200, "text/plain", "LF motor speed set successfully");
    } else if (command.startsWith("lb_speed=")) {
      String speedValue = command.substring(9);
      lb_speed = speedValue.toInt();
      // set LB motor speed here
      server.send(200, "text/plain", "LB motor speed set successfully");
    } else if (command.startsWith("rf_speed=")) {
      String speedValue = command.substring(9);
      rf_speed = speedValue.toInt();
      // set RF motor speed here
      server.send(200, "text/plain", "RF motor speed set successfully");
    } else if (command.startsWith("rb_speed=")) {
      String speedValue = command.substring(9);
      rb_speed = speedValue.toInt();
      // set RB motor speed here
      server.send(200, "text/plain", "RB motor speed set successfully");
    } else {
      Serial.println("Invalid command");
      server.send(400, "text/plain", "Invalid command");
    }
  } else {


String page = "<html><head><title>AILITE DASHBOARD</title>";
    page += "<style>";
    page += "body { background-color: #5d4e77; font-family: 'Roboto', sans-serif; color: #000000; margin: 0; padding: 0; }";
    page += ".top-band { background-color: #249ea0; color: #FFFFFF; padding: 20px; font-size: 24px; font-weight: bold; display: flex; justify-content: space-between; align-items: center; }";
    page += ".container { display: flex; flex-wrap: wrap; justify-content: space-between; padding: 20px; }";
    page += ".left-half, .right-half { box-sizing: border-box; padding: 20px; width: calc(50% - 40px); margin: 10px; background-color: #fff; border-radius: 10px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); }";
    page += ".control-panel { background-color: #9fc5e8; border-radius: 5px; box-shadow: 0 0 1px rgba(0, 0, 0, 0.1); padding: 10px; color: #0000FF; }";
    page += ".heading { text-align: center; margin-bottom: 20px; color: #100000; font-size: 24px; font-weight: bold; }";
    page += ".button-container { display: flex; flex-wrap: wrap; justify-content: center; }";
    page += ".button-container button { padding: 20px; margin: 10px; border-radius: 5px; border: 1px solid #ccc; cursor: pointer; font-size: 20px; background-color: #5fb067; color: #fff; min-width: 150px; font-weight: bold; }";
    page += ".button-container button:hover { background-color: #4c8c52; }";
    page += ".camera { width: 100%; height: 80vh; border: none; }";
    page += ".ip-info { margin-top: 20px; padding: 10px; background-color: #FFFFFF; color: #000000; border-radius: 5px; font-size: 16px; font-weight: bold; }";
    page += ".bottom-left-block { margin-top: 1px; }";
    page += ".gamepad-container { display: grid; grid-template-columns: 2fr 2fr 2fr; gap: 10px; justify-items: center; align-items: center; }";
    page += ".gamepad-container button { font-size: 13px; font-weight: bold; color: #fff; padding: 20px; margin: 10px; border-radius: 8px; border: 2px solid #ccc; cursor: pointer; }";
    page += ".gamepad-container button:nth-child(1) { grid-column: 2; background-color: #027cc3; width: 100%; height: 60px; }";
    page += ".gamepad-container button:nth-child(2) { grid-row: 2; background-color: #027cc3; width: 100%; height: 60px; }";
    page += ".gamepad-container button:nth-child(3) { grid-column: 2; grid-row: 2; background-color: #f54242; width: 100%; height: 60px; }";
    page += ".gamepad-container button:nth-child(4) { grid-column: 3; grid-row: 2; background-color: #027cc3; width: 100%; height: 60px; }";
    page += ".gamepad-container button:nth-child(5) { grid-column: 2; grid-row: 3; background-color: #027cc3; width: 100%; height: 60px; }";
    page += ".button-container button:nth-child(1) { background-color: #c90076; }";
    page += ".button-container button:nth-child(2) { background-color: #019136; }";
    page += ".button-container button:nth-child(3) { background-color: #f5a142; }";
    page += ".button-container button:nth-child(4) { background-color: #0c343d; }";
    page += ".response-box { margin-top: 18px; padding: 13px; background-color: #C7D2FE; border-radius: 8px; color: #000000; width: 94%; font-size: 16px; }";
    page += ".custom-command { display: flex; flex-direction: column; align-items: center; margin-top: 20px; }";
    page += ".custom-command input { padding: 10px; font-size: 16px; border-radius: 5px; border: 1px solid #ccc; margin-right: 10px; }";
    page += ".custom-command button { padding: 10px 20px; font-size: 16px; border-radius: 5px; border: 1px solid #ccc; background-color: #5fb067; color: #fff; cursor: pointer; }";
    page += ".speed-input-container { display: flex; justify-content: space-between; flex-wrap: wrap; margin-top: 20px; }";
    page += ".speed-input-container div { display: flex; flex-direction: column; align-items: center; margin: 0 10px; }";
    page += ".speed-input-container label { margin-bottom: 5px; font-weight: bold; }";
    page += ".speed-input-container input { padding: 10px; font-size: 16px; border-radius: 5px; border: 1px solid #ccc; margin-bottom: 10px; width: 80px; text-align: center; }";
    page += ".speed-input-container input[type=range] { width: 200px; }";
    page += ".speed-input-container button { padding: 10px 20px; font-size: 16px; border-radius: 5px; border: 1px solid #ccc; background-color: #5fb067; color: #fff; cursor: pointer; margin-top: 10px; }";
    page += ".control-panel-inner { display: flex; flex-wrap: wrap; justify-content: space-between; }";
    page += ".control-panel-inner > div { width: calc(50% - 10px); }";
    page += "</style></head><body>";
    page += "<div class='top-band'>AILITE - DASHBOARD";
    page += "<div class='ip-info'>Robot IP: http:// " + String(local_IP[0]) + "." + String(local_IP[1]) + "." + String(local_IP[2]) + "." + String(local_IP[3] + 4) + "</div>";
    page += "<div class='ip-info'>camera IP: http://" + String(local_IP[0]) + "." + String(local_IP[1]) + "." + String(local_IP[2]) + "." + String(local_IP[3]) + ":81/stream</div>";
    page += "</div>";
    page += "<div class='container'>";
    page += "<div class='left-half'>";
    page += "<div class='top-left-block'>";
    page += "<div class='control-panel'>";
    page += "<h1 class='heading'>Motor Controls</h1>";
    page += "<div class='control-panel-inner'>";
    page += "<div>";
    page += "<div class='gamepad-container'>";
    page += "<button id='forward' onclick='sendCommand(\"f\")'>Forward</button>";
    page += "<button id='left' onclick='sendCommand(\"l\")'>Left</button>";
    page += "<button id='stop' onclick='sendCommand(\"s\")'>Stop</button>";
    page += "<button id='right' onclick='sendCommand(\"r\")'>Right</button>";
    page += "<button id='backward' onclick='sendCommand(\"b\")'>Backward</button>";
    page += "</div>";
    page += "</div>";
    page += "</div>";
    page += "</div>";
    page += "</div>";
    page += "<div class='bottom-left-block'>";
    page += "<div class='control-panel'>";
    page += "<h1 class='heading'>Sensors</h1>";
    page += "<div class='button-container'>";
    page += "<button onclick='sendCommand(\"USLIR\")'>Left IR</button>";
    page += "<button onclick='sendCommand(\"US\")'>Ultrasonic</button>";
    page += "<button onclick='sendCommand(\"USRIR\")'>Right IR</button>";
    page += "<button onclick='sendCommand(\"cs\")'>Color Sensor</button>";
    // page += "<button onclick='sendCommand(\"t\")'>Touch</button>";
    page += "</div>";
    page += "<div class='response-box' id='response-box'></div>";
    page += "</div>";
    page += "</div>";
    page += "</div>";
    page += "<div class='right-half'>";
    page += "<div class='control-panel'>";
    page += "<h1 class='heading'>Camera Feed</h1>";
    page += "<iframe src='http://" + String(local_IP[0]) + "." + String(local_IP[1]) + "." + String(local_IP[2]) + "." + String(local_IP[3] + 4) + ":81/stream' class='camera'></iframe>";
    page += "</div>";
    page += "</div>";
    page += "<div class='left-half'>";
    page += "<div class='control-panel'>";
    page += "<h1 class='heading'>Speed Settings</h1>";
    page += "<div class='speed-input-container'>";
    page += "<div><label for='lf-speed-input'>LF Speed</label>";
    page += "<input type='text' id='lf-speed-input' value='0'></div>";
    page += "<div><label for='lb-speed-input'>LB Speed</label>";
    page += "<input type='text' id='lb-speed-input' value='0'></div>";
    page += "<div><label for='rf-speed-input'>RF Speed</label>";
    page += "<input type='text' id='rf-speed-input' value='0'></div>";
    page += "<div><label for='rb-speed-input'>RB Speed</label>";
    page += "<input type='text' id='rb-speed-input' value='0'></div>";
    page += "</div>";
    page += "<button onclick='sendSpeeds()'>Set</button>";
    page += "</div>";
    page += "</div>";
    page += "</div>";
    page += "<script>";
    page += "function sendCommand(cmd) {";
    page += "var xhttp = new XMLHttpRequest();";
    page += "xhttp.onreadystatechange = function() {";
    page += "if (this.readyState == 4) {";
    page += "if (this.status == 200) {";
    page += "document.getElementById('response-box').innerHTML = this.responseText;";
    page += "console.log('Command sent: ' + cmd);";
    page += "} else {";
    page += "console.log('Error: ' + this.status);";
    page += "}";
    page += "}";
    page += "};";
    page += "var param = 'cmd=' + cmd;";
    page += "xhttp.open('GET', '/?' + param, true);";
    page += "xhttp.send();";
    page += "}";
    page += "function fetchColor() {";
    page += "var xhttp = new XMLHttpRequest();";
    page += "xhttp.onreadystatechange = function() {";
    page += "if (this.readyState == 4) {";
    page += "if (this.status == 200) {";
    page += "document.getElementById('response-box').innerHTML = 'Color: ' + this.responseText;";
    page += "} else {";
    page += "console.log('Error: ' + this.status);";
    page += "}";
    page += "}";
    page += "};";
    page += "xhttp.open('GET', '/color', true);";
    page += "xhttp.send();";
    page += "}";
    page += "document.addEventListener('keydown', function(event) {";
    page += "var key = event.key.toLowerCase();";
    page += "if (key === 'w') { document.getElementById('forward').click(); }";
    page += "else if (key === 's') { document.getElementById('backward').click(); }";
    page += "else if (key === 'a') { document.getElementById('left').click(); }";
    page += "else if (key === 'd') { document.getElementById('right').click(); }";
    page += "});";
    page += "document.addEventListener('keyup', function(event) {";
    page += "var key = event.key.toLowerCase();";
    page += "if (key === 'w' || key === 's' || key === 'a' || key === 'd') { document.getElementById('stop').click(); }";
    page += "});";
    page += "function sendCustomCommand() {";
    page += "var cmd = document.getElementById('custom-command-input').value;";
    page += "sendCommand(cmd);";
    page += "}";
    page += "function sendSpeeds() {";
    page += "var lf = document.getElementById('lf-speed-input').value;";
    page += "var lb = document.getElementById('lb-speed-input').value;";
    page += "var rf = document.getElementById('rf-speed-input').value;";
    page += "var rb = document.getElementById('rb-speed-input').value;";
    page += "sendCommand('lf_speed=' + lf);";
    page += "sendCommand('lb_speed=' + lb);";
    page += "sendCommand('rf_speed=' + rf);";
    page += "sendCommand('rb_speed=' + rb);";
    page += "}";
    page += "</script></body></html>";

    server.send(200, "text/html", page);
  }
}