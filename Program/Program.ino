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
#include "EasyButton.h"
#include "images.h"


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
#define AMUX_SELECT D6

//publish topics
#define TEMPERATURETOPIC GENERALTOPIC "home/bedroom/temperature\0"
#define WILLTOPIC GENERALTOPIC "home/bedroom/esp/espConnected\0"
#define MOISTURETOPIC GENERALTOPIC "home/bedroom/plant/moisture\0"
#define LASTWATERTIMETOPIC GENERALTOPIC "home/bedroom/plant/lastwatertime\0"
#define PRESSURETOPIC GENERALTOPIC "home/bedroom/airpressure\0"
#define HUMIDITYTOPIC GENERALTOPIC "home/bedroom/humidity\0"
#define LIGHTTOPIC GENERALTOPIC "home/bedroom/light\0"
#define MODETOPIC GENERALTOPIC "home/bedroom/esp/mode\0"


//subscribe topics
#define BUTTONTOPIC GENERALTOPIC "home/bedroom/button\0"
#define GESTURETOPIC GENERALTOPIC "home/bedroom/gestureDevice\0"
#define REALTIMETOPIC GENERALTOPIC "home/bedroom/plant/lastwatertimereal\0"

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

//Light sensor
float light = 0;

//Moisture sensor
float moisture = 0;
float minimumMoisture = 300;
bool moistureRead = false;

//Humidity sensor
float humidity = 0;

//Airpressure sensor
float pressure = 0;

//Watering the plant
#define WaterTime 3000
#define CancelWaterTime 10000
unsigned long lastWateredTime = 0;

//Mode
bool manual = true;
unsigned long lastActionTime = 0;

//Menus
String realTime;
int menu = 0;
bool watering = false;
#define menuAmount 4
#define AutoMenuSwitchTimer 4000
int plantStatus = 0; // 0 = sad; 1 = happy

//Flash button
EasyButton button(0);

//Encyptor
Encryptor encryptor;

void setup() 
{
  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(ESP8266_LED, OUTPUT);
  pinMode(AMUX_SELECT, OUTPUT);
  pinMode(AMUX_PIN, INPUT);
  pinMode(2, OUTPUT);
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
  PublishMode();
  button.begin();
  button.onPressed(SwitchMode);
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
  button.read();
  if(manual && millis() - lastWateredTime > 60000){
    SwitchMode();
  }
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
      client.subscribe(REALTIMETOPIC);
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
  if(check.equals(REALTIMETOPIC)){
    realTime = String(message);
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
  lastActionTime = millis();
  if(manual && msg[0] == 'a'){
    WaterPlant();
  }
  else if(msg[0] == 'b'){
    SwitchMode();
  }
  else if(manual && msg[0] == 'c'){
    MenuUp();
  }
  else if(manual && msg[0] == 'd'){
    ReloadVariables();
  }
}

void SwitchMode(){
  lastActionTime = millis();
  LOGLN("Switch mode");
  manual = !manual;
  LOGLN(manual);
  digitalWrite(2,!manual);
  PublishMode();
}

void PublishMode(){
  if(manual){
    publishMessage(MODETOPIC,String(manual));
    return;
  }
  publishMessage(MODETOPIC,String(manual));
  
}

void ReloadVariables(){
  temp = bme.readTemperature();
  pressure = bme.readPressure();
  humidity = bme.readHumidity();
  digitalWrite(AMUX_SELECT, LOW);
  light = analogRead(AMUX_PIN);
  moistureRead = true;
  digitalWrite(AMUX_SELECT, HIGH);
  queue.Enqueue(ReadMoisture, 100);
  queue.Enqueue(SetMenuToMax, 101);
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Online Button

void ButtonAction(String msg){
  LOG("buttonpressed: ");
  LOG(msg);
  LOGLN("!");
  if (msg == "1")
  {
    WaterPlant();
  }
  else if (msg == "2")
  {
    ReloadVariables();
  }
  else if (msg == "3"){
    MenuUp();
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
  if(publishMessage(TEMPERATURETOPIC,String(temp))){
    LOG("Published Temp: ");
    LOGLN(temp);
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
  if (moisture < minimumMoisture)
  {
    plantStatus = 0;
    if (!manual)
    {
      WaterPlant();
    }
  }
  if (moisture >= minimumMoisture)
  {
    plantStatus = 1;
  }
  
  MQTT_publishMoisture();
  moistureRead = false;
}

void MQTT_publishMoisture(){
  if(publishMessage(MOISTURETOPIC,String(moisture))){
    LOG("Published Moisture: ");
    LOGLN(moisture);
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
  if(publishMessage(LIGHTTOPIC,String(light))){
    LOG("Published Light: ");
    LOGLN(light);
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
  if(publishMessage(HUMIDITYTOPIC,String(humidity))){
    LOG("Published Humidity: ");
    LOGLN(humidity);
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
  if(publishMessage(PRESSURETOPIC,String(pressure))){
    LOG("Published pressure: ");
    LOGLN(pressure);
  }
}

void AutoMenuUp(){
  if(!manual){
    MenuUp();
  }
  queue.Enqueue(AutoMenuUp, AutoMenuSwitchTimer);
}

void MenuUp(){
  menu = menu + 1 >= menuAmount ? 0 : menu + 1;
}

// Used for the ShowVariables method, it won't go to this screen automatically
void SetMenuToMax(){
  menu = menuAmount;
}

void Menu(int menu){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  
  if (watering)
  {
    ShowWatering();
  }
  else if (menu == 0)
  {
    ShowTempLight();
  }
  else if (menu == 1)
  {
    ShowHumPres();
  }
  else if (menu == 2)
  {
    ShowMoisture();
  }
  else if (menu == 3)
  {
    ShowLastWaterTime();
  }
  else if (menu == menuAmount)
  {
    ShowVariables();
  }
  display.display();
}

void ShowVariables(){
  display.print("Temp: ");
  float _temp = (int)floor(temp * 10) / 10;
  display.println(_temp);
  display.print("Light: ");
  float _light = floor(light * 10) / 10;
  display.println(_light);
  display.print("Humidity: ");
  float _humidity = floor(humidity * 10) / 10;
  display.println(_humidity);
  display.print("Pressure: ");
  float _pressure = floor(pressure * 10) / 10;
  display.println(_pressure);
  display.print("Moisture: ");
  float _moisture = floor(moisture * 10) / 10;
  display.println(_moisture);
}

void ShowTempLight(){
  display.print("Temp: ");
  float _temp = floor(temp * 10) / 10;
  display.println(_temp);
  display.print("Light: ");
  float _light = floor(light * 10) / 10;
  display.println(_light);
}

void ShowHumPres(){
  display.print("Humidity: ");
  float _humidity = floor(humidity * 10) / 10;
  display.println(_humidity);
  display.print("Pressure: ");
  float _pressure = floor(pressure * 10) / 10;
  display.println(_pressure);
}

void ShowMoisture(){
  display.print("Moisture: ");
  float _moisture = floor(moisture * 10) / 10;
  display.println(_moisture);
  ShowSmiley();
}

void ShowLastWaterTime(){
  display.println("Plant was watered on: ");
  display.println(realTime);
}

void ShowSmiley(){
  int x = display.getCursorX();
  int y = display.getCursorY() + 2;
  if (plantStatus == 0)
  {
    display.drawBitmap(x, y, sad_Pepe, 128, 54, WHITE);
    return;
  }
  display.drawBitmap(x, y, happy_Pepe, 128, 54, WHITE);
}

void ShowWatering(){
  display.drawBitmap(0, 0, watering_can, 128, 64, WHITE);
}

void WaterPlant(){
  if (millis() - lastWateredTime > CancelWaterTime)
  {
    StartWater();
    lastWateredTime = millis();
    client.publish(LASTWATERTIMETOPIC,"1",false);
    watering = true;
  }
}

void StopWateringPlant(){
  StopWater();
}

void StartWater(){
  int current = servo.read();
  if(current > 45){
    servo.write(current - 5);
    queue.Enqueue(StartWater,150);
    return;
  }
  queue.Enqueue(StopWateringPlant,2000);
}

void StopWater(){
  int current = servo.read();
  if(current < 135){
    servo.write(current + 5);
    queue.Enqueue(StopWater,150);
    return;
  }
  watering = false;
}

