// Web services
#include <ESP8266WiFi.h>
#include <ESPAsyncWiFiManager.h>    
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266HTTPClient.h> 
// File System
#include <fs.h>
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include <ArduinoJson.h> // ArduinoJson : https://github.com/bblanchon/ArduinoJson
// ota mise à jour sans fil
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
//mqtt
#include <PubSubClient.h>
#define mqtt_user "Batvoltage"

//***********************************
//************* variables 
//***********************************
int sensorPin = A0; 
int sensorValue = 0;
String logs ="";

float voltage; 
const String VERSION = "Version 1.0" ;

//***********************************
//************* Gestion du serveur WEB
//***********************************
// Create AsyncWebServer object on port 80
WiFiClient domotic_client;
// mqtt
PubSubClient client(domotic_client);

AsyncWebServer server(80);
DNSServer dns;
HTTPClient http;

//***********************************
//************* Gestion de la configuration
//***********************************

struct Config {
  char hostname[15];
  String IDX;
  float Vmax;
  int refresh; 
};

const char *filename_conf = "/config.json";
Config config; 

//***********************************
//************* Gestion de la configuration - Lecture du fichier de configuration
//***********************************

// Loads the configuration from a file
void loadConfiguration(const char *filename, Config &config) {
  // Open file for reading
  File configFile = SPIFFS.open(filename_conf, "r");

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<1024> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, configFile);
  if (error) {
    Serial.println(F("Failed to read file, using default configuration in function loadConfiguration"));
    logs += "Failed to read file, using default configuration in function loadConfiguration/r/n";

  }

  
  // Copy values from the JsonDocument to the Config
  strlcpy(config.hostname,                  // <- destination
          doc["hostname"] | "192.168.1.20", // <- source
          sizeof(config.hostname));         // <- destination's capacity
  config.IDX = doc["IDX"] | 200; 
  config.Vmax = doc["Vmax"] | 42.00; 
  config.refresh = doc["refresh"] | 5; 
  configFile.close();
      
}

//***********************************
//************* Gestion de la configuration - sauvegarde du fichier de configuration
//***********************************

void saveConfiguration(const char *filename, const Config &config) {
  
  // Open file for writing
   File configFile = SPIFFS.open(filename_conf, "w");
  if (!configFile) {
    Serial.println(F("Failed to open config file for writing in function Save configuration"));
    logs += "Failed to open config file for writing in function Save configuration/r/n";
    return;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<512> doc;

  // Set the values in the document
  doc["hostname"] = config.hostname;
  doc["IDX"] = config.IDX;
  doc["Vmax"] = config.Vmax;
  doc["refresh"] = config.refresh;
    
  // Serialize JSON to file
  if (serializeJson(doc, configFile) == 0) {
    Serial.println(F("Failed to write to file in function Save configuration "));
    logs += "Failed to write to file in function Save configuration:r/n";
  }

  // Close the file
  configFile.close();
}


const char* PARAM_INPUT_server = "server"; /// paramettre de retour server domotique
const char* PARAM_INPUT_IDX = "idx"; /// paramettre de retour idx
const char* PARAM_INPUT_Vmax = "Vmax"; /// paramettre de retour voltage max
const char* PARAM_INPUT_refresh = "refresh"; /// paramettre de retour refresh
const char* PARAM_INPUT_save = "save"; /// paramettre de retour cosphi

String getconfig() {
     
  String configweb = String(config.hostname) + ";" + config.IDX + ";" + config.Vmax + ";" + config.refresh + ";" + VERSION ;
  return String(configweb);
}

String getVoltage() {
  
  return String(voltage);
}

String processor(const String& var){
   Serial.println(var);
   if (var == "Voltage"){
    return getVoltage();
  }
 
  
}

String getState() {
  String state ; 

  state = String(voltage) ;
  return String(state);
}


          //***********************************
          //************* Setup 
          //***********************************




void setup() {
  // put your setup code here, to run once:
// init port
  
  Serial.begin(115200);
  Serial.println("Demarrage file System");
  SPIFFS.begin();
  
  // vérification de la présence d'index.html
  if(!SPIFFS.exists("/index.html")){
    Serial.println("Attention fichiers SPIFFS non chargé sur l'ESP, ça ne fonctionnera pas.");  
    logs += "Attention fichiers SPIFFS non chargé sur l'ESP, ça ne fonctionnera pas.";
    }

    //***********************************
    //************* Setup -  récupération du fichier de configuration
    //***********************************
// Should load default config if run for the first time
  Serial.println(F("Loading configuration..."));
   logs += "Loading configuration.../r/n";
  loadConfiguration(filename_conf, config);

  // Create configuration file
  Serial.println(F("Saving configuration..."));
  logs += "Saving configuration... /r/n";
  saveConfiguration(filename_conf, config);
  




      //***********************************
    //************* Setup - Connexion Wifi
    //***********************************

   // configuration Wifi
  AsyncWiFiManager wifiManager(&server,&dns);
  wifiManager.autoConnect("analog", "analog");

  
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

//Si connexion affichage info dans console
  Serial.println("");
  Serial.print("Connection ok sur le reseau :  ");
 
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP()); 
  Serial.println(ESP.getResetReason());

    //***********************************
    //************* Setup - OTA 
    //***********************************

  client.connect("batlevel");

  ArduinoOTA.setHostname("batlevel");
  //ArduinoOTA.setPassword(otapassword);
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  


      //***********************************
    //************* Setup - Web pages
    //***********************************

  server.on("/",HTTP_ANY, [](AsyncWebServerRequest *request){
      if(SPIFFS.exists("/index.html")){
     request->send(SPIFFS, "/index.html", String(), false, processor);
    }
    else {request->send_P(200, "text/plain", "fichiers SPIFFS non présent sur l ESP. " ); }
  });

    server.on("/config.html",HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/config.html", String(), false, processor);
  });

  server.on("/all.min.css", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/all.min.css", "text/css");
  });

    server.on("/favicon.ico", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/favicon.ico", "image/png");
  });

  server.on("/fa-solid-900.woff2", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/fa-solid-900.woff2", "text/css");
  });
  
    server.on("/sb-admin-2.js", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/sb-admin-2.js", "text/javascript");
  });

  server.on("/sb-admin-2.min.css", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/sb-admin-2.min.css", "text/css");
  });

server.on("/config.json", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/config.json", "application/json");
  });

  server.on("/state", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getState().c_str());
  });

  server.on("/config", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getconfig().c_str());
  });
  
server.on("/get", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    
    
    if (request->hasParam(PARAM_INPUT_save)) { Serial.println(F("Saving configuration..."));
                          saveConfiguration(filename_conf, config);   
                            }

     if (request->hasParam(PARAM_INPUT_server)) { request->getParam(PARAM_INPUT_server)->value().toCharArray(config.hostname,15);  }
     if (request->hasParam(PARAM_INPUT_IDX)) { config.IDX = request->getParam(PARAM_INPUT_IDX)->value().toInt();}                    
     if (request->hasParam(PARAM_INPUT_Vmax)) { config.Vmax = request->getParam(PARAM_INPUT_Vmax)->value().toFloat();}
     if (request->hasParam(PARAM_INPUT_refresh)) { config.refresh = request->getParam(PARAM_INPUT_refresh)->value().toInt();}                    
     request->send(200, "text/html", getconfig().c_str());

  });
  
    server.begin(); 
    client.setServer(config.hostname, 1883);
    
}





            //***********************************
            //************* loop
            //***********************************


void loop() {
  // put your main code here, to run repeatedly:
  sensorValue = analogRead(sensorPin);
  Serial.println("valeur");
  
  voltage = float(config.Vmax) * float(sensorValue) / 1024;
  mqtt("83", String(voltage));
  Serial.println(voltage);

  
  delay(config.refresh*1000 );
}

// ***********************************
// ************* Fonction interne
// ***********************************

void mqtt(String idx, String value)
{
  String nvalue = "0" ; 
  if ( value != "0" ) { nvalue = "2" ; }
String message = "  { \"idx\" : " + idx +" ,   \"svalue\" : \"" + value + "\",  \"nvalue\" : " + nvalue + "  } ";

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  client.publish("domoticz/in", String(message).c_str(), true);
  
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world it's me !! bat voltage ");
      // ... and resubscribe
      client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

