// DEBUG or RELEASE for logging
#define DEBUG

#define ARDUINO 10813

#define numToCharArray(x) String(x).c_str()
#define numToString(x) String(x)
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
#include "Encrypting.h"


#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12
#define D7 13
#define D8 15


#define SERVO_PIN D5
#define ONBOARD_LED 16    // LED on the side of the processor
#define ESP8266_LED 2     // LED on the side of the wifi unit
#define AMUX_PIN A0
#define AMUX_SELECT D3

//publish topics
#define TEMPERATURETOPIC GENERALTOPIC "home/bedroom/temperature\0"
#define WILLTOPIC GENERALTOPIC "home/bedroom/espConnected\0"
#define MOISTURETOPIC GENERALTOPIC "home/bedroom/moisture\0"
#define PRESSURETOPIC GENERALTOPIC "home/bedroom/airpressure\0"
#define HUMIDITYTOPIC GENERALTOPIC "home/bedroom/humidity\0"
#define LIGHTTOPIC GENERALTOPIC "home/bedroom/light\0"
#define GESTURETOPIC GENERALTOPIC "home/bedroom/gestureDevice\0"

//subscribe topics
#define BUTTONTOPIC GENERALTOPIC "home/bedroom/button"

//Network
WiFiClient espClient;
PubSubClient client(espClient);
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];

//Servo
Servo servo;
int lastServoTime = 0;
int servoDelay = 3000;
bool servoUp = false;

//Real OLED display
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3c
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);

//Queue
EventQueue queue;

//BME280
Adafruit_BME280 bme;

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

//Watering the plant
#define WaterTime 3000
#define CancelWaterTime 10000
unsigned long lastWateredTime = 0;

//Mode
bool manual = false;

//Menus
int menu = 1;
#define menuAmount 2
#define AutoMenuSwitchTimer 5000
int plantStatus = 0; // 0 = sad; 1 = normal; 2 = happy

//Flash button
int prevButton = LOW;
int button = LOW;

//Encyptor
Encryptor encryptor;

void setup() 
{
  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(ESP8266_LED, OUTPUT);
  pinMode(AMUX_SELECT, OUTPUT);
  pinMode(AMUX_PIN, INPUT);
  pinMode(2, OUTPUT);
  pinMode(0, INPUT_PULLUP);
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

  servo.write(135);
  EnqueueSensors();
  queue.Enqueue(AutoMenuUp,AutoMenuSwitchTimer);
  digitalWrite(2,LOW);
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
  Menu(menu);

  button = digitalRead(0);
  if(button != prevButton && button == LOW){
    SwitchMode();
  }
  prevButton = button;
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
      client.subscribe(GESTURETOPIC);
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
  char* msg = new char[length + 1];
  for(int i = 0; i < length; i++){
    LOG((char)payload[i]);
    msg[i] = (char)payload[i];
  }
  msg[length] = '\0';
  LOGLN();
  char* message = encryptor.Decrypt(msg,topic,String(topic).length());
  LOGLN(message);
  String check = String(topic);
  if(check.equals(BUTTONTOPIC)){
    ButtonAction(message);
  }
  if(check.equals(GESTURETOPIC)){
    GestureAction(message);
  }
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//Encryption

bool publishMessage(const char *topic, String payload){
  return client.publish(topic,encryptor.Encrypt(payload.c_str(),payload.length(), topic, String(topic).length()),true);
}



//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Gestures

void GestureAction(String msg){
  if(manual && msg[0] == "a"){
    WaterPlant();
  }
  else if(msg[0] == "b"){
    SwitchMode();
  }
  else if(manual && msg[0] == "c"){
    MenuUp();
  }
  else if(manual && msg[0] == "d"){
    ReloadVariables();
  }
}

void SwitchMode(){
  LOGLN("Switch mode");
  manual = !manual;
  LOGLN(manual);
  digitalWrite(2,!manual);
}

void ReloadVariables(){
  temp = bme.readTemperature();
  pressure = bme.readPressure();
  humidity = bme.readHumidity();
  digitalWrite(AMUX_SELECT, LOW);
  light = analogRead(AMUX_PIN);
  digitalWrite(AMUX_SELECT, HIGH);
  queue.Enqueue(ReadMoisture, 100);
  queue.Enqueue(ShowVariables, 101);
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Online Button

void ButtonAction(String msg){
  if(msg == "1"){
    WaterPlant();
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
  if(temp != prevtemp && publishMessage(TEMPERATURETOPIC,String(temp))){
    LOG("Published Temp: ");
    LOGLN(temp);
    prevtemp = temp;
  }
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Moisture Sensor

void UpdateMoisture(){
  moistureRead = true;
  digitalWrite(AMUX_SELECT, HIGH);
  queue.Enqueue(ReadMoisture, 100);
  queue.Enqueue(UpdateMoisture, 5000);
}

void ReadMoisture(){
  moisture = analogRead(AMUX_PIN);
  digitalWrite(AMUX_SELECT, LOW);
  MQTT_publishMoisture();
  moistureRead = false;
}

void MQTT_publishMoisture(){
  if(moisture != prevmoisture && publishMessage(MOISTURETOPIC,String(moisture))){
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
    light = analogRead(AMUX_PIN);
    MQTT_publishLight();
    queue.Enqueue(UpdateLight, 2000);
    return;
  }
  queue.Enqueue(UpdateLight, 100);
}

void MQTT_publishLight(){
  if(light != prevlight && publishMessage(LIGHTTOPIC,String(light))){
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
  if(humidity != prevhumidity && publishMessage(HUMIDITYTOPIC,String(humidity))){
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
  if(pressure != prevpressure && publishMessage(PRESSURETOPIC,String(pressure))){
    LOG("Published pressure: ");
    LOGLN(pressure);
    prevpressure = pressure;
  }
}

void AutoMenuUp(){
  if(!manual){
    MenuUp();
  }
  queue.Enqueue(AutoMenuUp, AutoMenuSwitchTimer);
}

void MenuUp(){
  menu = menu + 1 == menuAmount ? 0 : menu + 1;
}

void Menu(int menu){
  if(menu == 0)
  {
    ShowVariables();
  }
  else if (menu == 1)
  {
    ShowSmiley();
  }
  else if (true)
  {
    /* code */
  }
  
}

void ShowVariables(){
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

const unsigned char sad_Pepe [] PROGMEM = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x3f, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe3, 0xff, 0x0f, 0x80, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9f, 0xff, 0xe6, 0x3f, 0xe7, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbf, 0xff, 0xf0, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xf3, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xf9, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xe3, 0xfd, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xf9, 0xfe, 0x1e, 0x0c, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xf9, 0xf9, 0xff, 0xe0, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xf3, 0xe7, 0xff, 0xfc, 0x1f, 0xf0, 0x1f, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xf3, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xc7, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xe7, 0xff, 0xff, 0xff, 0x3f, 0xff, 0xf3, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xef, 0xff, 0xff, 0x00, 0x1f, 0x80, 0x01, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xcf, 0xff, 0xf0, 0xff, 0xc6, 0x3f, 0xf8, 0x7f, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xcf, 0xff, 0x87, 0xe0, 0x63, 0x80, 0x03, 0x3f, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0xfe, 0x3e, 0x06, 0x04, 0x1f, 0xf8, 0x3f, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xfc, 0x0f, 0xf1, 0xc1, 0xff, 0xc9, 0xf8, 0x03, 0x9f, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xfc, 0x9f, 0xfe, 0x1f, 0xf8, 0x03, 0x01, 0xfe, 0x0f, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xf9, 0x9f, 0xe0, 0xff, 0x03, 0xf8, 0x3f, 0x07, 0xcf, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xf1, 0x9f, 0xef, 0xe0, 0x40, 0xfb, 0xfd, 0x83, 0xef, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xf3, 0xbf, 0xe4, 0x03, 0xb0, 0x7b, 0xf8, 0x83, 0xef, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xf3, 0xbf, 0xfe, 0x7f, 0x02, 0x3b, 0xf0, 0x31, 0xdf, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xe7, 0xff, 0xff, 0x3e, 0x07, 0x3b, 0xf1, 0x31, 0x9f, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xe7, 0xff, 0xff, 0xc6, 0x02, 0x00, 0xf0, 0x00, 0x1f, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xcf, 0xff, 0xfe, 0x70, 0x00, 0x3b, 0x00, 0x0e, 0x3f, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xdf, 0xff, 0xff, 0x0e, 0x01, 0xe7, 0xff, 0xfc, 0x7f, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0x9f, 0xff, 0xff, 0xf0, 0x00, 0x07, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0x9f, 0xff, 0xff, 0xff, 0x00, 0x7f, 0xff, 0xf9, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0x3f, 0xff, 0xff, 0xff, 0xfe, 0x7e, 0x7f, 0xe7, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0x3f, 0xff, 0xff, 0xff, 0xf9, 0xff, 0x1f, 0x0f, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xff, 0xe7, 0xff, 0x80, 0x07, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xff, 0x1f, 0xff, 0xe7, 0xe7, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xf3, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xfc, 0x00, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xf0, 0x00, 0x1f, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xe7, 0xff, 0xf0, 0x7f, 0xff, 0x82, 0x7f, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xef, 0xbf, 0xff, 0x00, 0x00, 0x0e, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xec, 0x00, 0x3f, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfc, 0x7f, 0xff, 0xef, 0xff, 0xc1, 0xff, 0xff, 0xf3, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfc, 0x7f, 0xff, 0xef, 0xff, 0xfc, 0x00, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfe, 0x3f, 0xfe, 0xe0, 0x3f, 0xff, 0xfe, 0x1f, 0xfb, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfe, 0x3f, 0xfe, 0xf0, 0x00, 0xff, 0xff, 0xff, 0xf3, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0x1f, 0xff, 0x7f, 0xf8, 0x00, 0x7f, 0xff, 0xe7, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0x8f, 0xff, 0x9f, 0xff, 0xfc, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xc7, 0xff, 0xc7, 0xff, 0xff, 0xff, 0xf8, 0x7f, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xe1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xf8, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xf3, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0xff, 0xff, 0xff, 0xff, 0x8f, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x0f, 0xff, 0xff, 0xfe, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

const unsigned char happy_Pepe [] PROGMEM = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9f, 0xe3, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xfc, 0x7f, 0x06, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0x18, 0xff, 0xcf, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xc3, 0xff, 0xf7, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xf7, 0xff, 0xff, 0xef, 0xff, 0xf3, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xef, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xdf, 0xff, 0x07, 0xf7, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xdf, 0xf0, 0xf8, 0xf7, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xbf, 0xc7, 0xff, 0x37, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xbf, 0x9f, 0xff, 0xc0, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xf1, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xfd, 0xff, 0xff, 0xe7, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xf0, 0x00, 0xff, 0x9f, 0xf3, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0x8f, 0xff, 0x38, 0x20, 0x01, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xff, 0x20, 0x03, 0x83, 0xf0, 0xf9, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xf9, 0xff, 0xfc, 0x00, 0x70, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xe5, 0xff, 0xc0, 0x00, 0x20, 0x67, 0xc0, 0x0c, 0x7f, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0x9d, 0xff, 0xf0, 0x20, 0x1f, 0x1e, 0x40, 0x63, 0x3f, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xbb, 0xff, 0x04, 0xe0, 0x1f, 0xd9, 0xd0, 0x3c, 0xbf, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0x7b, 0xfe, 0x19, 0xc8, 0xcf, 0xe7, 0x81, 0xbe, 0x3f, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfe, 0x7b, 0xfe, 0x77, 0xc0, 0xcf, 0xe7, 0x81, 0xbf, 0xbf, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0x87, 0xc0, 0x0f, 0xef, 0x80, 0x3f, 0xbf, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xf8, 0xc0, 0x0f, 0xcf, 0xc0, 0x3e, 0x3f, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xfe, 0x00, 0x1f, 0x83, 0xc0, 0x70, 0x7f, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfd, 0xff, 0xff, 0xff, 0x80, 0x38, 0x08, 0x00, 0x06, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfd, 0xff, 0xff, 0xf9, 0xf8, 0x07, 0xcf, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xfe, 0x0f, 0xff, 0x1f, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xff, 0xff, 0x01, 0xff, 0xff, 0xe7, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xcf, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xff, 0xe7, 0xfb, 0xff, 0x1f, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xff, 0x8f, 0xfc, 0x7c, 0x3f, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xfc, 0x7f, 0xff, 0x1f, 0xbf, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9f, 0xdf, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdf, 0xef, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf7, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf3, 0xff, 0xff, 0x07, 0xff, 0xff, 0xff, 0xff, 0xf7, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf3, 0xff, 0xfe, 0xf9, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf3, 0xff, 0xfe, 0xfe, 0x7f, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf3, 0xff, 0xfe, 0xff, 0x9f, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf3, 0xff, 0xfe, 0xef, 0xe3, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf3, 0xff, 0xfe, 0xf7, 0xf8, 0x3f, 0xff, 0xff, 0xc1, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf3, 0xff, 0xff, 0x79, 0xff, 0x83, 0xff, 0xfc, 0x3d, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf3, 0xff, 0xff, 0x3e, 0x7f, 0xfe, 0x00, 0x03, 0xfd, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xdf, 0xcf, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xe7, 0xf8, 0x1f, 0xff, 0xff, 0xf7, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xf0, 0xff, 0xe0, 0x00, 0x00, 0x07, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xff, 0xf7, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xef, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xff, 0x80, 0xff, 0xff, 0xcf, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf1, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf3, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xc1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe7, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xf8, 0x1f, 0xff, 0xff, 0xff, 0xff, 0x8f, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0x03, 0xff, 0xff, 0xff, 0xfe, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x0f, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x03, 0x00, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

bool changingStatus = false;

void ShowSmiley(){
  display.clearDisplay();
  if (plantStatus == 0)
  {
    display.drawBitmap(0, 0, sad_Pepe, 128, 64, WHITE);
  }
  else if (plantStatus == 2)
  {
    display.drawBitmap(0, 0, happy_Pepe, 128, 64, WHITE);
  }
  display.display();

  if (!changingStatus)
  {
    changingStatus = true;
    queue.Enqueue(ChangePlantStatus, AutoMenuSwitchTimer + 500);
  }
  
}

void ChangePlantStatus(){
  if (plantStatus == 0)
  {
    plantStatus = 2;
  }
  else
  {
    plantStatus = 0;
  }
  changingStatus = false;
}

void WaterPlant(){
  if (millis() - lastWateredTime > CancelWaterTime)
  {
    servo.write(45);
    queue.Enqueue(StopWateringPlant,3000);
  }
}
void StopWateringPlant(){
  servo.write(135);
}
