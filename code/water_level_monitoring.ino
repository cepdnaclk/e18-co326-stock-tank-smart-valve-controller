/*CO326 - Stock-tank-smart-valve-controller*/
#include <WiFi.h>
#include <PubSubClient.h>
#include <DallasTemperature.h>

const int trigPin = 5;    // GPIO2
const int echoPin = 18;   // GPIO4
const int ledPin = 15;    // GPIO15
const char* ssid="Wifi";
const char* pass="Passcode";
const char* brokerUser="";
const char* brokerPass="";
const char* broker="91.121.93.94"; //test.mosquitto.org
const char* outTopic1="/device/distance"; //output the distance
const char* outTopic2="/device/motorOnOff"; //Output the status that motor on or off
const char* outTopic3="/device/modeManual";
const char* outTopic4="/device/modeAuto"; 
const char* outTopic5="/device/motorOnCount";
const char* outTopic6="/device/motorOnTime";
const char* outTopic7="/device/temp";
const char* inTopic1="/device/mode";  //select the mode
const char* inTopic2="/device/motorStatus";  //select the motor status

WiFiClient espClient;
PubSubClient client(espClient);
long distance;
char waterLevel[50];
char motorOnCountStr[50];
char motorTimeDifferenceStr[50];
char temperatureStr[50];
byte manualMode=0; //flag to indicate manual/auto mode
byte motorOn=0; //flag to check motor status
int motorOnCount = 0;
int motorCountFlag = 1;
long motorOnTime = 0;  // variable to store the time when motor is turned on
long motorOffTime = 0;  // variable to store the time when motor is turned off
long motorTimeDifference = 0;  // variable to store the time difference

#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);  // Pass our oneWire reference to Dallas Temperature

//set wifi connection
void setupWifi(){
  delay(100);
  Serial.print("/nConnecting to");
  Serial.println(ssid);

  WiFi.begin(ssid,pass); 

  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print("-");
  }

  Serial.print("\nConnected to");
  Serial.println(ssid);
}

//reconnect function to ensure the connection of mqtt with esp32
void reconnect(){
  while (!client.connected()) {
  Serial.print("\nConnecting to ");
  Serial.println(broker);
  String clientId = "ESP32Client-";
  clientId += String(random(0xffff), HEX);
  if(client.connect(clientId.c_str())){
    Serial.print("\nConnected to");
    Serial.println(broker);
    client.subscribe(inTopic1);
    client.subscribe(inTopic2);
  } else{
    Serial.println("\nTrying connect again");
    delay(5000);
  }
  }
}

//getting a response from mqtt broker
void callback(char* topic, byte* payload, unsigned int length){
  // Convert the toipc to a string
  String topicStr= String(topic);
  Serial.print("Received topic: ");
  Serial.println(topic);

  // Convert the payload to a string
  String payloadStr = "";
  for (int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }
  Serial.print("Received Payload: ");
  Serial.println(payloadStr);

  //select the mode (auto/manual)
  if (topicStr.equals("/device/mode") && payloadStr.equals("true")) { 
    manualMode=1;
    Serial.println("Manuall mode");
  } else if (topicStr.equals("/device/mode") && payloadStr.equals("false")){
    manualMode=0;  
    Serial.println("Auto mode");
  }

  //turn on and off the led at manual mode
  //Here 50 is full depth of tank
  if (topicStr.equals("/device/motorStatus") && payloadStr.equals("true") && manualMode==1 && distance > 3 && distance < 20) { 
    digitalWrite(ledPin, HIGH);   // Turn the LED on when user manually turn on the LED
    motorOn=1;
    motorOnTime=millis(); // store the time when motor is turned on
    motorOnCount++;
    Serial.println("Manually turned on led");
  } else if (topicStr.equals("/device/motorStatus") && payloadStr.equals("false") && manualMode==1 && distance > 3 && distance < 20) {
    digitalWrite(ledPin, LOW);   // Turn the LED off when user manually turn off the LED
    motorOn=0;
    motorOffTime=millis(); // store the time when motor is turned off
    motorCountFlag=1;
    Serial.println("Manually turned off led");
  }
}

// Function to calculate the distance in centimeters
long calculateDistance() {
  long duration_val,distance_val;

  // Send ultrasonic pulse
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Measure the echo pulse duration
  duration_val = pulseIn(echoPin, HIGH);

  // Calculate the distance in centimeters
  distance_val = duration_val * 0.034 / 2;

  return distance_val;
}

float measureTemperature() { 
  sensors.requestTemperatures(); // Send the command to get temperatures
  float tempC = sensors.getTempCByIndex(0); //get the temperature values using celcius

  // Check if reading was successful
  if(tempC != DEVICE_DISCONNECTED_C) {
    return tempC;
  } else{
    return -1;
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(ledPin, OUTPUT);  // Set the LED pin as an output
  setupWifi();
  client.setServer(broker,1883);
  client.setCallback(callback);
}

void loop() {
  // put your main code here, to run repeatedly:
  static long previousDistance = -1;
  static byte previousMotorStatus = -1;
  static byte previousModeStatus = -1;
  static int previousMotorOnCount = -1;
  static long previousMotorTimeDifference = -1;
  static float previousTemperature = -1;
  float temperature =0;

  //if cliient not connected to broker reconneect the client
  if(!client.connected()){ 
    reconnect();
  }
  client.loop();

  // Calculate the distance
  distance = calculateDistance();
  temperature = measureTemperature();

  //To check that motor is on manual mode or not
  if(manualMode!=1){//auto mode
    // Control the LED based on the distance
    if (distance <= 3) {
      digitalWrite(ledPin, LOW);   // Turn off the LED
      motorOn=0;
      if(!motorCountFlag && !motorOn){
        motorOffTime=millis(); // store the time when motor is turned off
      }
      motorCountFlag=1;
    } else if (distance >= 17) {
      digitalWrite(ledPin, HIGH);  // Turn on the LED
      motorOn=1;
      if(motorCountFlag && motorOn){
        motorOnCount++;
        motorOnTime=millis(); // store the time when motor is turned on
        motorCountFlag=0;
      }
    }
  } else{ //manual mode
    // Turn off the LED if the water level approach the highest level when it is on manual mode
    if (distance <= 3) {
      digitalWrite(ledPin, LOW);   
      motorOn=0;
      motorCountFlag=1;
    }
  }

  //publish only if distance is changed
  if(distance!=previousDistance){
    snprintf(waterLevel,75,"%ld",(20-distance));
    Serial.print("Sending water level: ");
    Serial.println(waterLevel);
    client.publish(outTopic1,waterLevel);
    previousDistance=distance;
  }

  //publish only if motorOn state is changed
  if(motorOn!=previousMotorStatus){
    //publish only if motor is on or off
    if(motorOn){
      client.publish(outTopic2,"ON");
    } else{
      client.publish(outTopic2,"OFF");
    }
    previousMotorStatus=motorOn;
  }

  //publish only if manualMode is changed
  if(manualMode!=previousModeStatus){
    if(manualMode){
      client.publish(outTopic3,"ON");
      client.publish(outTopic4,"OFF");
    } else{
      client.publish(outTopic3,"OFF");
      client.publish(outTopic4,"ON");
    }
    previousModeStatus=manualMode;
  }

  //publish only if motorOnCount is changed
  if(motorOnCount!=previousMotorOnCount){
    snprintf(motorOnCountStr,75,"%ld",motorOnCount);
    Serial.print("Sending Motor Count: ");
    Serial.println(motorOnCountStr);
    client.publish(outTopic5,motorOnCountStr);
    previousMotorOnCount=motorOnCount;
  }

  // Calculate the time difference if motor has been turned off and on
  if (motorOnTime < motorOffTime) {
    motorTimeDifference = motorOffTime - motorOnTime;
    motorTimeDifference = motorTimeDifference/1000.0;
  } else {
    motorTimeDifference = 0;  // Motor is still on or no valid time difference
  }

  //publish only if motorTimeDifference is changed
  if(motorTimeDifference!=previousMotorTimeDifference){
    snprintf(motorTimeDifferenceStr,75,"%ld s",motorTimeDifference);
    Serial.print("Sending Motor Time: ");
    Serial.println(motorTimeDifferenceStr);
    client.publish(outTopic6,motorTimeDifferenceStr);
    previousMotorTimeDifference=motorTimeDifference;
  }

  if(temperature!=previousTemperature && temperature!=-1){
    snprintf(temperatureStr,75,"%.2f C",temperature);
    Serial.print("Sending Temperature Value: ");
    Serial.println(temperatureStr);
    client.publish(outTopic7,temperatureStr);
    previousTemperature=temperature;
  }
  
  delay(1000);
}
