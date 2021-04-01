// DEBUG or RELEASE for logging
#define DEBUG

#define ARDUINO 10813

#define numToCharArray(x) String(x).c_str()
#ifdef DEBUG
#define LOG(x) Serial.print(x)
#define LOGLN(x) Serial.println(x)
#define BEGINLOGGING Serial.begin(115200)
#define WAITONLOGGER while(!Serial)
#else
#define LOG(x)
#define LOGLN(x)
#define BEGINLOGGING
#define WAITONLOGGER
#endif

#include <ESP8266WiFi.h>
#include <Servo.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_SSD1306.h>
#include "WifiCredentials.h"
#include "PubSubClient.h"
#include "EventQueue.h"


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
#define ONBOARD_LED 16    // LED on the side of the processor
#define ESP8266_LED 2     // LED on the side of the wifi unit
#define MOISTURE_LED A0

//publish topics
#define TEMPERATURETOPIC GENERALTOPIC "home/bedroom/temperature"
#define WILLTOPIC GENERALTOPIC "home/bedroom/espConnected"
#define MOISTURETOPIC GENERALTOPIC "home/bedroom/moisture"
#define PRESSURETOPIC GENERALTOPIC "home/bedroom/airpressure"
#define HUMIDITYTOPIC GENERALTOPIC "home/bedroom/humidity"
#define LIGHTTOPIC GENERALTOPIC "home/bedroom/light"

//subscribe topics
#define BUTTONTOPIC GENERALTOPIC "home/bedroom/button"

//Network
WiFiClient espClient;
PubSubClient client(espClient);
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];

//Servo
Servo servo;

//BME280
Adafruit_BME280 bme;

//Real OLED display
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3c
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);

//Queue
EventQueue queue;

//Temperature sensor
float temp = 0;
float prevtemp = 0;

//Light sensor
float light = 0;
float prevlight = 0;

//Moisture sensor
float moisture = 0;
float prevmoisture = 0;
bool moistureRead = false;

//Humidity sensor
float humidity = 0;
float prevhumidity = 0;

//Airpressure sensor
float pressure = 0;
float prevpressure = 0;

void setup() 
{
  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(ESP8266_LED, OUTPUT);
  pinMode(D3, OUTPUT);
  pinMode(A0, INPUT);
  servo.attach(SERVO_PIN);
  BEGINLOGGING;
  WAITONLOGGER;
  
  // Setup BME
  if (!bme.begin(0x76))
  {
    LOGLN("Could not find a valid BME280 sensor");
    while (1);
  }

  // Setup display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    LOGLN("SSD1306 allocation failed");
    while (1);
  }
  display.display();

  setup_wifi();

  client.setServer(SERVER,SERVERPORT);
  client.setCallback(callback);

  EnqueueSensors();
}

void EnqueueSensors(){
  queue.Enqueue(UpdateTemp, 1000);
  queue.Enqueue(UpdateHumidity, 1000);
  queue.Enqueue(UpdateLight, 1000);
  queue.Enqueue(UpdateMoisture, 1000);
  queue.Enqueue(UpdatePressure, 1000);
}

void setup_wifi(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID,PASSWORD);
  int i = 0;
  while(WiFi.status() != WL_CONNECTED){
    delay(1000);
    LOG(++i); 
    LOG(' ');
  }
  LOGLN();
  LOG("Connection established: ");
  LOGLN(SSID);
  LOG("Ip address: ");
  LOGLN(WiFi.localIP());
}

void loop() 
{
  MQTT_checkConnection();
  
  queue.PerformEvents();
  TestI2C();
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------
// MQTT networking

void MQTT_checkConnection(){
  if(!client.connected()){
    reconnect();
  }
  client.loop();
}

void reconnect(){
  while (!client.connected()) {
    LOG("Attempting MQTT connection...");
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(),USERNAME,KEY,WILLTOPIC,1,true,"DISCONNECTED")) {
      LOGLN("connected");
      client.publish(WILLTOPIC,"CONNECTED",true);
      //subsciptions here
      client.subscribe(BUTTONTOPIC);
      break;
    } 
    LOG("failed, rc=");
    LOG(client.state());
    LOGLN(" try again in 5 seconds");
    delay(5000);
  }
}

void callback(char* topic, byte* payload, unsigned int length){
  LOG("Message arrived [");
  LOG(topic);
  LOG("] ");
  char* msg = new char[length];
  for(int i = 0; i < length; i++){
    LOG((char)payload[i]);
    msg[i] = (char)payload[i];
  }
  LOGLN();
  String check = String(topic);
  if(check.equals(BUTTONTOPIC)){
    ButtonAction(String(msg));
  }
  delete(msg);
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Online Button

void ButtonAction(String msg){
  if(msg == "1"){
    LOGLN("HI");
  }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Temperature Sensor

void UpdateTemp(){
  temp = bme.readTemperature();
  MQTT_publishTemp();
  queue.Enqueue(UpdateTemp, 5000);
}

void MQTT_publishTemp(){
  if(temp != prevtemp && client.publish(TEMPERATURETOPIC,numToCharArray(temp))){
    LOG("Published Temp: ");
    LOGLN(temp);
    prevtemp = temp;
  }
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Moisture Sensor

void UpdateMoisture(){
  moistureRead = true;
  digitalWrite(D3, HIGH);
  queue.Enqueue(ReadMoisture, 100);
  queue.Enqueue(UpdateMoisture, 5000);
}

void ReadMoisture(){
  moisture = analogRead(A0);
  digitalWrite(D3, LOW);
  MQTT_publishMoisture();
  moistureRead = false;
}

void MQTT_publishMoisture(){
  if(moisture != prevmoisture && client.publish(MOISTURETOPIC,numToCharArray(moisture))){
    LOG("Published Moisture: ");
    LOGLN(moisture);
    prevmoisture = moisture;
  }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Light Sensor

void UpdateLight(){
  if (!moistureRead)
  {
    digitalWrite(D3, LOW);
    light = analogRead(A0);
    MQTT_publishLight();
    queue.Enqueue(UpdateLight, 2000);
    return;
  }
  queue.Enqueue(UpdateLight, 100);
}

void MQTT_publishLight(){
  if(light != prevlight && client.publish(LIGHTTOPIC,numToCharArray(light))){
    LOG("Published Light: ");
    LOGLN(light);
    prevlight = light;
  }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Humidity Sensor

void UpdateHumidity(){
  humidity = bme.readHumidity();
  MQTT_publishHumidity();
  queue.Enqueue(UpdateHumidity, 5000);
}

void MQTT_publishHumidity(){
  if(humidity != prevhumidity && client.publish(HUMIDITYTOPIC,numToCharArray(humidity))){
    LOG("Published Humidity: ");
    LOGLN(humidity);
    prevhumidity = humidity;
  }
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Airpressure Sensor

void UpdatePressure(){
  pressure = bme.readPressure();
  MQTT_publishPressure();
  queue.Enqueue(UpdatePressure, 5000);
}

void MQTT_publishPressure(){
  if(pressure != prevpressure && client.publish(PRESSURETOPIC,numToCharArray(pressure))){
    LOG("Published pressure: ");
    LOGLN(pressure);
    prevpressure = pressure;
  }
}

void TestI2C(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("Temperature: ");
  display.println(temp);
  display.print("Humidity: ");
  display.println(humidity);
  display.print("Pressure: ");
  display.println(pressure);
  display.print("Moisture: ");
  display.println(moisture);
  display.print("Light: ");
  display.println(light);
  display.display();
}