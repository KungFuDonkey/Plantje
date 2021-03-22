#include <ESP8266WiFi.h>
#include <Servo.h>
#include <Wire.h> 
#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"

#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12
#define D7 13
#define D8 15


#define SERVO_PIN D0
#define ONBOARD_LED 16
#define ESP8266_LED 2
#define MOISTURE_LED A0

Servo servo;
SSD1306Wire  display(0x3c, D3, D5);

OLEDDisplayUi ui ( &display );
int screenW = 128;
int screenH = 64;

void setup() 
{
  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(ESP8266_LED, OUTPUT);
  servo.attach(SERVO_PIN);
  Serial.begin(115200);

  // Print Wi-Fi Information
  WiFi.printDiag(Serial);

  // Print VCC voltage
  Serial.print("Current VCC: ");
  Serial.println(ESP.getVcc());
}

void loop() 
{
  
}