
/****
 * TrigBoard
 * 
 * F. Guiet 
 * Creation           : 20190204
 * Last modification  : 20200102
 * 
 * Version            : 1.0
 * History            : 1.0 - First version
 *                      1.1 - Add StillOnCheck
 *                      20200102 - 1.2 - Remove battery voltage (not implemented yet on this board, check for external update first
 *                      20200207 - 1.3 Move to ArduinoJson 6.1.4
 *                      
 * Note               : Credits go to Kevin Darrah who inspired me to build this trigboard (https://www.kevindarrah.com/wiki/index.php?title=TrigBoard)
 *                      ESP Board I am using ESP-12-E
 *                      To program using Arduino IDE use NodeMCU 1.0 (ESP-12E Module)
 *                      
 *                      !!! CAREFUL when sending big message : do not forget to MQTT_MAX_PACKET_SIZE in PubSubClient library !!!
*/

//Light Mqtt library
#include <PubSubClient.h>
//Wifi library
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

//Mqtt settings
#define MQTT_SERVER "192.168.1.25"
#define DEBUG 0

#define LED_PIN 2 //ESP-12-E led pin
#define DONE_PIN 12
#define STILLOPEN_PIN 13 //To check whether reed switch is still open 
#define EXTWAKE_PIN 5 //To check whether it is an external wake up or a tpl5111 timer wake up
#define ADC_PIN A0 //To read battery voltage

// WiFi settings
const char* ssid = "DUMBLEDORE";
const char* password = "frederic";

#define SMS_TOPIC  "guiet/automationserver/smsservice"
#define MQTT_CLIENT_ID "TrigBoardGarageDoorSensor"
#define MAX_RETRY 100
#define FIRMWARE_VERSION "1.3"

WiFiClient espClient;
PubSubClient client(espClient);

struct Sensor {
    //String Address;
    String Name;    
    String SensorId;
    String Mqtt_topic;
    String External_wakeup;
};

#define SENSORS_COUNT 1
Sensor sensors[SENSORS_COUNT];

char message_buff[200];

void InitSensors(bool externalWakeUp) {
  
  sensors[0].Name = "Trigboard - Garage door";
  sensors[0].SensorId = "18";
  sensors[0].Mqtt_topic = "guiet/garage/sensor/18";

  if (externalWakeUp)
     sensors[0].External_wakeup = "oui";
  else
     sensors[0].External_wakeup = "non";
}

/*
 * EXTWAKE_PIN is HIGH when ESP is wake up by TPL5111 
 */
bool isExternalWakeUp() {
  if (digitalRead(EXTWAKE_PIN) == HIGH) {
    return false; 
  }
  else {
    return true;
  }
}

void SendStillOpenMessage() {
  if (WiFi.status() != WL_CONNECTED) {
    if (!connectToWifi())
      weAreDone();
  }  

  if (!client.connected()) {
    if (!connectToMqtt()) {
      weAreDone();
    }
  }

  String mess = ConvertToJSon("Garage door is still opened !");
  debug_message("JSON Sensor : " + mess + ", topic : " +SMS_TOPIC, true);
  mess.toCharArray(message_buff, mess.length()+1);
    
  client.publish(SMS_TOPIC,message_buff);

  disconnectMqtt();
  delay(100);
  disconnectWifi();
  delay(100);
  
}

/*
 * STILLON_PIN is HIGH when reedswitch is opened 
 */
bool isStillOpen() {
  if (digitalRead(STILLOPEN_PIN) == HIGH) {
    return true; 
  }
  else {
    return false;
  }
}

void makeLedBlink(int blinkTimes, int millisecond) {

  for (int x = 0; x < blinkTimes; x++) {
    digitalWrite(LED_PIN, HIGH);
    delay(millisecond);
    digitalWrite(LED_PIN, LOW);
    delay(millisecond);
  } 
}

/* The DONE pin is driven by a μC to signal that the μC is working properly. The TPL5111 recognizes a valid
DONE signal as a low to high transition. */
void weAreDone() {

  debug_message("Bye bye dude...", true);
  
  for (int i=0;i<10;i++) {
    digitalWrite(DONE_PIN, HIGH);
    delay(100);
    digitalWrite(DONE_PIN, LOW);
    delay(100);
  }
}

//not implemented of this board for the moment...
float readVoltage() {

  return 0;

  //Based on TrigBoard Rev7
  /*float batteryVoltage = (4 - 3.5)/(712 - 621) ;
  batteryVoltage = analogRead(A0) * batteryVoltage + (4 - batteryVoltage * 712);
  return batteryVoltage;

  //return analogRead(ADC_PIN);
    
  //return 0.00342 * analogRead(ADC_PIN) + .823;//scale that 1V max to 0-4.5V
  //R1 = 33kOhm
  //R2 = 7.5kOhm

  float sensorValue = 0.0f;
  float R1 = 32800;
  //float R2 =  7460;
  float R2 =  9600;
  float vmes = 0.0f;
  float vin = 0.f;

  float voltageInput = (analogRead(A0)* 3.4) / 1023.0;
  float voltage = voltageInput / (R1 / (R2 + R1));
  return voltage;
    
  sensorValue = analogRead(ADC_PIN);

  debug_message("Analog value : " + String(sensorValue,2), true);

  //ADC between 0 and 1v...maybe ajusted a little
  //1.1v is a little lower than expected...
  vmes = (sensorValue) / 1024;

  //Calcul issue du pont diviseur
  vin = vmes / (R2 / (R1 + R2));

  debug_message("Battery voltage : " + String(vin,2), true);

  return vin;  */
}

void setup() {

  //Initialise PINS
  pinMode(EXTWAKE_PIN, INPUT);
  pinMode(STILLOPEN_PIN, INPUT_PULLUP);
  pinMode(DONE_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(ADC_PIN, OUTPUT);

  //First of all check whether it is an external wakeup or not
  bool externalWakeUp = isExternalWakeUp();

  //Turn off LED
  digitalWrite(LED_PIN, HIGH);

  if (DEBUG) {
     //For debug purpose
     Serial.begin(115200);
     delay(100);
  }

  //External or TPL5111 wake up?
  if (!externalWakeUp) {

    debug_message("TPL5111 woke me up! leave me alone...", true);
    
    makeLedBlink(6,200);

    externalWakeUp = false;    

    //Check if reed switch is open or not
    if (isStillOpen()) {
      debug_message("reedswitch is open", true);
      InitSensors(externalWakeUp);
      SendStillOpenMessage();
    }
    else {
      debug_message("reedswitch is NOT open", true);
    }

    //Some delay so debug message can be printed
    if (DEBUG) 
      delay(100);
      
    //Go to sleep immediatly
    weAreDone();
    return;
  }
  else {
    
    debug_message("Someone has opened the garage door, time to send a message!...", true);
    InitSensors(externalWakeUp);
    
  }
}

void disconnectMqtt() {
  debug_message("Disconnecting from mqtt...", true);
  client.disconnect();
}

void disconnectWifi() {
  debug_message("Disconnecting from wifi...", true);
  WiFi.disconnect();
}

void debug_message(String message, bool doReturnLine) {
  if (DEBUG) {
    if (doReturnLine)
      Serial.println(message);
    else
      Serial.print(message);
  }
}

boolean connectToMqtt() {

   client.setServer(MQTT_SERVER, 1883); 

  int retry = 0;
  // Loop until we're reconnected
  while (!client.connected() && retry < MAX_RETRY) {
    debug_message("Attempting MQTT connection...", true);
    
    if (client.connect(MQTT_CLIENT_ID)) {
      debug_message("connected to MQTT Broker...", true);
    } else {
      retry++;
      // Wait 5 seconds before retrying
      delay(500);
      //yield();
    }
  }

  if (retry >= MAX_RETRY) {
    debug_message("MQTT connection failed...", true);  
    return false;
    //goDeepSleep();
  }

  return true;
}

boolean connectToWifi() {

  // WiFi.forceSleepWake();
  WiFi.mode(WIFI_STA);
  WiFi.setOutputPower(20.5); // this sets wifi to highest power 
  
  int retry = 0;
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && retry < MAX_RETRY) {
    retry++;
    delay(500);
    debug_message(".", false);
  }

  if (WiFi.status() == WL_CONNECTED) {  
     debug_message("WiFi connected", true);  
     // Print the IP address
     if (DEBUG) {
      Serial.println(WiFi.localIP());
     }
     
     return true;
  } else {
    debug_message("WiFi connection failed...", true);   
    return false;
  }  
}

String ConvertToJSon(String message) {
    //Create JSon object
    DynamicJsonDocument  jsonBuffer(200);
    JsonObject root = jsonBuffer.to<JsonObject>(); 

    //Garage door still open guid
    String guid ="cce5d7f1-7aeb-443b-9b6d-c41db55da832";
    
    root["messageid"] = guid;
    root["messagetext"] = message;
           
    String result;
    serializeJson(root, result);

    return result;
}

/*String ConvertToJSon(String battery) {
    //Create JSon object
    DynamicJsonDocument  jsonBuffer(200);
    JsonObject root = jsonBuffer.to<JsonObject>();    
    
    root["sensorid"] = sensors[0].SensorId;
    root["name"] = sensors[0].Name;
    root["firmware"]  = FIRMWARE_VERSION;
    root["battery"] = battery;
    root["externalwakeup"] = sensors[0].External_wakeup; 
       
    String result;
    serializeJson(root, result);

    return result;
}*/

void loop() {
  //Turn on LED...
  digitalWrite(LED_PIN, LOW);

  if (WiFi.status() != WL_CONNECTED) {
    if (!connectToWifi())
      weAreDone();
  }  

  if (!client.connected()) {
    if (!connectToMqtt()) {
      weAreDone();
    }
  }

  //Read battery voltage
  //float vin = readVoltage();

  String mess = ConvertToJSon("Garage door has just opened ! (battery : check from time to time !)");
  //String mess = ConvertToJSon(String(vin,2));
  debug_message("JSON Sensor : " + mess + ", topic : " +SMS_TOPIC, true);
  mess.toCharArray(message_buff, mess.length()+1);
    
  client.publish(SMS_TOPIC,message_buff);

  //Turn off LED... 
  digitalWrite(LED_PIN, HIGH);

  disconnectMqtt();
  delay(100);
  disconnectWifi();
  delay(100);

  weAreDone();
}
