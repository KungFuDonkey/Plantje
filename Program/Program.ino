#include <ESP8266WiFi.h>
#include <Servo.h>

<<<<<<< HEAD
#define ONBOARD_LED 16
#define ESP8266_LED 2
#define MOISTURE_LED A0
=======
#define ONBOARD_LED 16    // LED on the side of the processor
#define ESP8266_LED 2     // LED on the side of the wifi unit

// ADC_MODE(ADC_VCC); will result in compile error. use this instead.
int __get_adc_mode(void) { return (int) (ADC_VCC); }
>>>>>>> 20994ed62fb850912f7a80b89bd74c796258cca4

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