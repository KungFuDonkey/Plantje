#include <ESP8266WiFi.h>

#define ONBOARD_LED 16
#define ESP8266_LED 2

// ADC_MODE(ADC_VCC); will result in compile error. use this instead.
int __get_adc_mode(void) { return (int) (ADC_VCC); }

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
  digitalWrite(ONBOARD_LED, HIGH);
  digitalWrite(ESP8266_LED, HIGH);
  Serial.println("LED on");
  delay(500);
  digitalWrite(ONBOARD_LED, LOW);
  digitalWrite(ESP8266_LED, LOW);
  Serial.println("LED off");
  delay(500);
}