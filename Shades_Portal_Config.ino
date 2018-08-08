#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <SimpleTimer.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <AH_EasyDriver.h>

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40];
char mqtt_port[6] = "1883";
char mqtt_user[20] = "MQTT_USER";
char mqtt_pass[20] = "MQTT_PW";
char mqtt_client_id[20] = "UNIQUE_ID_NO_SPACES";
char rotations[6] = "13";

//Global Variables
bool boot = true;
const int stepsPerRevolution = 200; 
int unrolled = 13;
int currentPosition = 0;
int newPosition = 0;


WiFiClient espClient;
PubSubClient client(espClient);
SimpleTimer timer;

//AH_EasyDriver(int RES, int DIR, int STEP, int MS1, int MS2, int SLP);
AH_EasyDriver shadeStepper(300,4,0,14,12,13);    // init w/o "enable" and "reset" functions

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
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

void checkIn()
{
  char topic[40];
  strcpy(topic, "checkIn/");
  strcat(topic, mqtt_client_id);
  client.publish(topic, "OK"); 
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

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();

  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_pass, json["mqtt_pass"]);
          strcpy(mqtt_client_id, json["mqtt_client_id"]);
          strcpy(rotations, json["rotations"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read



  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 20);
  WiFiManagerParameter custom_mqtt_pass("pass", "mqtt pass", mqtt_pass, 20);
  WiFiManagerParameter custom_mqtt_client_id("ID", "unique ID", mqtt_client_id, 20);
  WiFiManagerParameter custom_rotations("rotations", "13", rotations, 6);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);
  wifiManager.addParameter(&custom_mqtt_client_id);
  wifiManager.addParameter(&custom_rotations);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("ShadeConfig")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_pass, custom_mqtt_pass.getValue());
  strcpy(mqtt_client_id, custom_mqtt_client_id.getValue());
  strcpy(rotations, custom_rotations.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_pass"] = mqtt_pass;
    json["mqtt_client_id"] = mqtt_client_id;
    json["rotations"] = rotations;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }
  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  shadeStepper.setMicrostepping(2);            // 0 -> Full Step                                
  shadeStepper.setSpeedRPM(200);               // set speed in RPM, rotations per minute
  shadeStepper.sleepOFF();                     // set Sleep mode OFF
  unsigned long int_port = strtoul(mqtt_port, NULL, 10);
  unrolled = strtoul(rotations, NULL, 10);
  client.setServer(mqtt_server, int_port);
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
