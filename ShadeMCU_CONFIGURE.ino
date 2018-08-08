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
const int stepsPerRevolution = 200; 
int currentPosition = 0;
int newPosition = 0;

//AH_EasyDriver(int RES, int DIR, int STEP, int MS1, int MS2, int SLP);
AH_EasyDriver shadeStepper(300,4,0,14,12,13);    // init w/o "enable" and "reset" functions

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
        }
        // ... and resubscribe
        char topic[40];
        strcpy(topic, "shadePosition/");
        strcat(topic, mqtt_client_id);
        client.subscribe(topic);
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
    newPosition = newPayload;
    currentPosition = newPayload;
    boot = false;
  }
  if(boot == false)
  {
    newPosition = newPayload;
  }
}

void processStepper()
{
  if (newPosition > currentPosition)
  {
    shadeStepper.sleepON();
    shadeStepper.move(4144, BACKWARD);
    currentPosition++;
    client.loop();
  }
  if (newPosition < currentPosition)
  {
    shadeStepper.sleepON();
    shadeStepper.move(4144, FORWARD);
    currentPosition--;
    client.loop();
  }
  if (newPosition == currentPosition)
  {
    if (currentPosition == 0 || currentPosition == unrolled)
    {
      shadeStepper.sleepOFF();
      client.loop();
    }
  }
  Serial.println(currentPosition);
  Serial.println(newPosition);
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
  shadeStepper.setMicrostepping(2);            // 0 -> Full Step                                
  shadeStepper.setSpeedRPM(200);               // set speed in RPM, rotations per minute
  shadeStepper.sleepOFF();                     // set Sleep mode OFF
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

