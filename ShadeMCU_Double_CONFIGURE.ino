#include <SimpleTimer.h> //https://github.com/marcelloromani/Arduino-SimpleTimer/tree/master/SimpleTimer
#include <ESP8266WiFi.h>
#include <PubSubClient.h> //https://github.com/knolleary/pubsubclient
#include <ESP8266mDNS.h> 
#include <WiFiUdp.h>
#include <ArduinoOTA.h> //https://github.com/esp8266/Arduino/tree/master/libraries/ArduinoOTA
#include <AH_EasyDriver.h> //http://www.alhin.de/arduino/downloads/AH_EasyDriver_20120512.zip


//USER CONFIGURED SECTION START//
const char* ssid = "YOUR_WIRELESS_SSID";
const char* password = "YOUR_WIRELESS_PASSWORD";
const char* mqtt_server = "YOUR_MQTT_SERVER_ADDRESS";
const int mqtt_port = 1883;
const char *mqtt_user = "YOUR_MQTT_USERNAME";
const char *mqtt_pass = "YOUR_MQTT_PASSWORD";
const char *mqtt_client_id = "PICK_UNIQUE_NAME_NO_SPACES"; //This will be used for your MQTT topics
const int unrolled = 13; //number of full rotations from fully rolled to fully unrolled
//USER CONFIGURED SECTION END//


WiFiClient espClient;
PubSubClient client(espClient);
SimpleTimer timer;

//Global Variables
bool boot = true;
bool bootR = true;
bool bootL = true;
const int stepsPerRevolution = 200; 
int leftPosition = 0;
int leftNew = 0;
int rightPosition = 0;
int rightNew = 0;
char topic2[40];
char topic1[40];

//AH_EasyDriver(int RES, int DIR, int STEP, int MS1, int MS2, int SLP);
AH_EasyDriver leftStepper(300,16,13,2,0,4);    // init w/o "enable" and "reset" functions
AH_EasyDriver rightStepper(300,14,12,2,0,15);    // init w/o "enable" and "reset" functions

//Functions
void setup_wifi() {
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() 
{
  int retries = 0;
  while (!client.connected()) {
    if(retries < 150)
    {
      Serial.print("Attempting MQTT connection...");
      if (client.connect(mqtt_client_id, mqtt_user, mqtt_pass)) 
      {
        Serial.println("connected");
        if(boot == false)
        {
          char topic[40];
          strcpy(topic, "checkIn/");
          strcat(topic, mqtt_client_id);
          client.publish(topic, "Reconnected"); 
        }
        if(boot == true)
        {
          char topic[40];
          strcpy(topic, "checkIn/");
          strcat(topic, mqtt_client_id);
          client.publish(topic, "Rebooted"); 
          strcat(topic1, mqtt_client_id);
          strcpy(topic1, "/shadePositionL");
          strcat(topic2, mqtt_client_id);
          strcpy(topic2, "/shadePositionR");
        }
        // ... and resubscribe      
        client.subscribe(topic1);
        client.subscribe(topic2);
      } 
      else 
      {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        retries++;
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
    if(retries > 149)
    {
    ESP.restart();
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) 
{
  Serial.print("Message arrived [");
  String newTopic = topic;
  Serial.print(topic);
  Serial.print("] ");
  payload[length] = '\0';
  String s = String((char*)payload);
  int newPayload = s.toInt();
  Serial.println(newPayload);
  Serial.println();
  if(boot == true)
  {
    if(topic == topic1)
    {
    leftPosition = newPayload;
    leftNew = newPayload;
    bootL = false;
    }
    if(topic == topic2)
    {
    rightPosition = newPayload;
    rightNew = newPayload;
    bootR = false;
    }
    if(bootL == false && bootR == false)
    {
      boot = false;
    }
    
  }
  if(boot == false)
  {
    if(topic == topic1)
    {
    leftNew = newPayload;
    }
    if(topic == topic2)
    {
    rightNew = newPayload;
    }
  }
}

void processStepper()
{
  if (boot == false)
  client.loop();
  {
    if (leftNew > leftPosition  && rightNew > rightPosition)
    {
      rightStepper.sleepON();
      leftStepper.sleepON();
      for (int x = 0; x < 4145; x++)
      {
      ESP.wdtFeed();
      leftStepper.move(1, BACKWARD);
      rightStepper.move(1, BACKWARD);
      }
      leftPosition++;
      rightPosition++;
    }
    if (leftNew < leftPosition  && rightNew < rightPosition)
    {
      rightStepper.sleepON();
      leftStepper.sleepON();
      for (int x = 0; x < 4145; x++)
      {
      ESP.wdtFeed();
      leftStepper.move(1, FORWARD);
      rightStepper.move(1, FORWARD);
      }
      leftPosition--;
      rightPosition--;
    }
    if (leftNew < leftPosition  && rightNew > rightPosition)
    {
      rightStepper.sleepON();
      leftStepper.sleepON();
      for (int x = 0; x < 4145; x++)
      {
      ESP.wdtFeed();
      leftStepper.move(1, FORWARD);
      rightStepper.move(1, BACKWARD);
      }
      leftPosition--;
      rightPosition++;
    }
    if (leftNew > leftPosition  && rightNew < rightPosition)
    {
      rightStepper.sleepON();
      leftStepper.sleepON();
      for (int x = 0; x < 4145; x++)
      {
      ESP.wdtFeed();
      leftStepper.move(1, BACKWARD);
      rightStepper.move(1, FORWARD);
      }
      leftPosition++;
      rightPosition--;
    }
    if (leftNew == leftPosition  && rightNew > rightPosition)
    {
      rightStepper.sleepON();
      rightStepper.move(4144, BACKWARD);
      rightPosition++;
    }
    if (leftNew == leftPosition  && rightNew < rightPosition)
    {
      rightStepper.sleepON();
      rightStepper.move(4144, FORWARD);
      rightPosition--;
    }
    if (leftNew > leftPosition  && rightNew == rightPosition)
    {
      leftStepper.sleepON();
      leftStepper.move(4144, BACKWARD);
      leftPosition++;
    }
    if (leftNew < leftPosition  && rightNew == rightPosition)
    {
      leftStepper.sleepON();
      leftStepper.move(4144, FORWARD);
      leftPosition--;
    }
    if (rightNew == rightPosition)
    {
      if (rightPosition == 0 || rightPosition == unrolled)
      {
        rightStepper.sleepOFF();
      }
    }
    if (leftNew == leftPosition)
    {
      if (leftPosition == 0 || leftPosition == unrolled)
      {
        leftStepper.sleepOFF();
      }
    }
  }
}

void checkIn()
{
  char topic[40];
  strcpy(topic, "checkIn/");
  strcat(topic, mqtt_client_id);
  client.publish(topic, "OK"); 
}


//Run once setup
void setup() {
  Serial.begin(115200);
  leftStepper.setMicrostepping(2);            // 0 -> Full Step                                
  leftStepper.setSpeedRPM(200);               // set speed in RPM, rotations per minute
  leftStepper.sleepOFF();                     // set Sleep mode OFF
  rightStepper.setMicrostepping(2);            // 0 -> Full Step                                
  rightStepper.setSpeedRPM(200);               // set speed in RPM, rotations per minute
  rightStepper.sleepOFF();                     // set Sleep mode OFF
  WiFi.mode(WIFI_STA);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  ArduinoOTA.setHostname("ShadeStepperMCU");
  ArduinoOTA.begin(); 
  delay(10);
  timer.setInterval(800, processStepper);   
  timer.setInterval(90000, checkIn);
}

void loop() 
{
  if (!client.connected()) 
  {
    reconnect();
  }
  client.loop();
  ArduinoOTA.handle();
  timer.run();
}

