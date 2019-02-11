#include "OneWire.h"
#include <math.h>

// Temperature chip i/o
int DS18S20_Pin = 2; //DS18S20 Signal pin on digital 2
OneWire ds(DS18S20_Pin);  // on digital pin 2

// Ph Meter
int PhSensorPin = 0;          //pH meter Analog output to Arduino Analog Input 0
unsigned long int avgValue;  //Store the average value of the sensor feedback
float b;
int buf[10],temp;
float OffsetPh = 0; //Offset value to calibrate PhProbe

unsigned long pumpTime = 0; // in miliseconds
unsigned long pumpStartTime;
unsigned long sendInfoTime; 
bool pumpRunning = false;
unsigned long sendInfoInterval = 20000;
bool sendInfoPeriodic = false;
bool newData = false;

// Function declarations
void actionPumpOn(int);
void actionPumpOff();
void(* resetFunc) (void) = 0; // Reset arduino


void setup() {
  
  Serial.begin(9600);
  delay(3000);

  //pinMode(3, OUTPUT);
  //pinMode(5, OUTPUT);
  //pinMode(6, OUTPUT);
  //pinMode(13,OUTPUT); 
  sendInfo();
  
  
}


void loop() {

  recvInfo();
  sendInfoInterv();
  checkTimers();
  
}

void recvInfo() {

  if (Serial.available() > 0) {
    
    String receivedString = "";
    receivedString = Serial.readString();
 
    newData = true;
    // Reset arduino
    if (receivedString == "r"){
      resetFunc(); 
    }
    if (receivedString == "i"){
      sendInfo(); 
    }
    if (receivedString == "t"){
      sendTempReading();
    }
    if (receivedString == "p"){
      sendPhReading();
    }
    if (receivedString == "o"){
      sendOrpReading();
    }
    // PowerOn pump
    if (receivedString.substring(0,1) == "m"){
      int timePump = receivedString.substring(2).toInt(); // extract number from string
      actionPumpOn(timePump);
    }
    // SendInfo shuttingDownPump
    if (receivedString == "g"){
      sendInfoWPump();
    }
    // Set send information interval
    if (receivedString.substring(0,1) == "i"){
      int interval = receivedString.substring(2).toInt(); // extract number from string
      sendInfoInterval = interval;
      sendInfoPeriodic = true;
    }
  }
  
}

void sendInfoInterv() {
  // Check interval
  if (((unsigned long)(millis() - sendInfoTime) >= sendInfoInterval) and sendInfoPeriodic) {
    sendInfo();
    sendInfoTime = millis();
  }
}

void sendInfo() {
  char phData[10];
  dtostrf(readPh(), 4, 2, phData);  //4 is mininum width, 2 is precision
  char orpData[10];
  dtostrf(readOrp(), 4, 2, orpData);  //4 is mininum width, 2 is precision
  String data = "{ \"type\": \"data\", \"data\": {";
              data += "\"temperature\":";data += readTemp(); data += ",";
              data += "\"ph\":"; data += phData; data += ",";
              data += "\"orp\":"; data += orpData;
              data += " }}";
  Serial.println(data);
}

int readTemp() {
  int analogPin = DS18S20_Pin;
  float temperature = getTemp();
  return (int) temperature;
}

void sendTempReading() {

  int temperature = readTemp();
  //Sending a JSON string over Serial/USB.
  Serial.println("{\"temperature\":\"" + String((long)round(temperature)) + " }");
}

float readPh()  {
  int analogPin = PhSensorPin;
  for(int i=0 ; i<10 ; i++)       //Get 10 sample value from the sensor for smooth the value
  { 
    buf[i]=analogRead(analogPin);
    delay(10);
  }
  for(int i=0;i<9;i++)        //sort the analog from small to large
  {
    for(int j=i+1;j<10;j++)
    {
      if(buf[i]>buf[j])
      {
        temp=buf[i];
        buf[i]=buf[j];
        buf[j]=temp;
      }
    }
  }
  avgValue = 0;
  for(int i=2 ; i<8 ; i++)                      //take the average value of 6 center sample
    avgValue += buf[i];
  float phValue = (float)avgValue*5.0/1024/6; //convert the analog into millivolt
  phValue=3.5 * phValue + OffsetPh;                      //convert the millivolt into pH value
  return phValue;
}

void sendPhReading() {
  float phValue = readPh();
  //Sending a JSON string over Serial/USB.
  Serial.println("{\"ph\":\"" + String((long)round(phValue)) + " }");
  digitalWrite(13, HIGH);       
  delay(800);
  digitalWrite(13, LOW); 

}

float readOrp() {
  int analogPin = 0;
  int reading = analogRead(analogPin);
  return reading;
}

void sendOrpReading(){
  float reading = readOrp();
  //Sending a JSON string over Serial/USB.
  Serial.println("{\"orp\":\"" + String((long)round(reading)) + "}");
}

void actionPumpOn(int minutes){
  pumpTime = 1000*minutes;
  pumpTime = pumpTime*60;
  // check max pump running time: 2 hours
  if (minutes > 120) 
    return;
  pumpStartTime = millis();
  pumpRunning = true;
  int digitalPin = 0;
  digitalWrite(digitalPin, HIGH);
  Serial.println("{ \"type\": \"info\", \"data\": { \"message\": \"PumpOn " + String(minutes) + " min\" }}");
}

void actionPumpOff(){
  pumpTime = 0;
  pumpRunning = false;
  int digitalPin = 0;
  digitalWrite(digitalPin, LOW);
  Serial.println("{ \"type\": \"info\", \"data\": { \"message\": \"PumpOff\" }}");
}

// SendInfo shuttingDown the Pump to avoid turbulence and wrong metered data
void sendInfoWPump(){
  if(!pumpRunning) { // if pump is not running
    int timePump = 2;
    actionPumpOn(timePump); // run 2 minutes
    unsigned long timeDelay = 1000*timePump;
    timeDelay = 60*timeDelay;
    delay( timeDelay + 1000); // wait until pump is off
    actionPumpOff();
    delay(1*30*1000UL); // wait 0.5 minute
    sendInfo(); // sendData
  }
  else{ // if pump is running wait 1 minute
    unsigned long remaing_minutes = (pumpTime - (unsigned long)(millis() - pumpStartTime)); // pump remaining time
    remaing_minutes = remaing_minutes/1000;
    remaing_minutes = remaing_minutes/60; // convert to minutes
    actionPumpOff(); // powerOff Pump
    delay(1*30*1000UL); // wait 0.5 minute
    sendInfo(); // sendData
    actionPumpOn(remaing_minutes); // restore pump task
  }
}

void checkTimers(){
  // Check pumpTime
  if (((unsigned long)(millis() - pumpStartTime) >= pumpTime) and pumpRunning ) {
    actionPumpOff();
  }
}

//returns the temperature from one DS18S20 in DEG Celsius
float getTemp(){

  byte data[12];
  byte addr[8];

  if ( !ds.search(addr)) {
      //no more sensors on chain, reset search
      ds.reset_search();
      return -1000;
  }

  if ( OneWire::crc8( addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return -1000;
  }

  if ( addr[0] != 0x10 && addr[0] != 0x28) {
      Serial.print("Device is not recognized");
      return -1000;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44,1); // start conversion, with parasite power on at the end

  byte present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE); // Read Scratchpad

  
  for (int i = 0; i < 9; i++) { // we need 9 bytes
    data[i] = ds.read();
  }
  
  ds.reset_search();
  
  byte MSB = data[1];
  byte LSB = data[0];

  float tempRead = ((MSB << 8) | LSB); //using two's compliment
  float TemperatureSum = tempRead / 16;
  
  return TemperatureSum;
  
}
