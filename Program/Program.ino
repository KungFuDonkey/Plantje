// DEBUG or RELEASE for logging
#define DEBUG

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
#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
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
#define ONBOARD_LED 16
#define ESP8266_LED 2
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

//Oled display
SSD1306Wire display(0x3c, D3, D5);
OLEDDisplayUi ui ( &display );

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
  servo.attach(SERVO_PIN);
  BEGINLOGGING;
  WAITONLOGGER;

  setup_wifi();

  client.setServer(SERVER,SERVERPORT);
  client.setCallback(callback);

  // Print VCC voltage
  LOG("Current VCC: ");
  LOGLN(ESP.getVcc());
  display.init();

  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  temp = 20;
  queue.Enqueue(UpdateTemp,10000);
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
  /*
  display.clear();

  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(10, 128, String(millis()));
  // write the buffer to the display
  display.display();
  */
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
  // Read temp here

  temp = 1;
  MQTT_publishTemp();
  queue.Enqueue(UpdateTemp,10000);
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
  // Read moisture here
  moisture = 1;
  MQTT_publishMoisture();
  queue.Enqueue(UpdateMoisture,10000);
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
  // Read light here
  light = 1;
  MQTT_publishLight();
  queue.Enqueue(UpdateLight,10000);
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
  // Read humidity here
  humidity = 1;
  MQTT_publishHumidity();
  queue.Enqueue(UpdateHumidity,10000);
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
  // Read pressure here
  pressure = 1;
  MQTT_publishPressure();
  queue.Enqueue(UpdatePressure,10000);
}

void MQTT_publishPressure(){
  if(pressure != prevpressure && client.publish(PRESSURETOPIC,numToCharArray(pressure))){
    LOG("Published pressure: ");
    LOGLN(pressure);
    prevpressure = pressure;
  }
}
