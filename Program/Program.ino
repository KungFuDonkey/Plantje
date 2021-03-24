#include <ESP8266WiFi.h>
#include <Servo.h>

#define ONBOARD_LED 16    // LED on the side of the processor
#define ESP8266_LED 2     // LED on the side of the wifi unit
#define MOISTURE_LED A0

void setup() 
{
  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(ESP8266_LED, OUTPUT);
  Serial.begin(115200);

  // Print Wi-Fi Information
  WiFi.printDiag(Serial);

  // Print VCC voltage
  Serial.print("Current VCC: ");
  Serial.println(ESP.getVcc());
}

void loop() 
{
  Serial.println(analogRead(MOISTURE_LED));
  delay(500);
}