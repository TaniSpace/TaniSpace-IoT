/*
--------------------------------
SMARTHIDROPONIK
--------------------------------
An Easy Way To Plants Your Plant

[+] VERSION : 1.0
[+] RELEASE : 21-12-2018
*/

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <DHT.h>

//current pin used
#define DHTPIN D2
#define PUMP D3
#define VALVE D4
#define LAMP D5
//using DHT11 sensor.
//uncomment sensor that used 
#define DHTTYPE DHT11
//#define DHTTYPE DHT22   
//#define DHTTYPE DHT21 

//WiFi configuration
const char* WIFI_SSID       = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD   = "YOUR_WIFI_PASSWORD";

//Delay configuration
int LOOP_DELAY              = 5000;
int VALVE_DELAY             = 20000;
int PUMP_DELAY              = 20000;

//REST API configuration
String API_KEY              = "YOUR_API_KEY";
String HOST                 = "http://dashboard.tanispace.io/api/v1";
String PATH_PING            = "/ping";
String PATH_HUMIDITY_TEMP   = "/temperature/add";
String PATH_LIGHT_VALUE     = "/light/add";
String PATH_REMOTE_CONTROL  = "/remote";
String PATH_CONFIG          = "/config";

//DHT & Lighting configuration
DHT dht(DHTPIN, DHTTYPE);
const int LIGHTING = A0;

StaticJsonBuffer<200> jsonBuffer;
const size_t bufferSize = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 
      2*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + 
      JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(12) + 390;
  
HTTPClient http;

void setup() {
    Serial.begin(9600);   //Serial connection
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);   //WiFi connection
 
    while (WiFi.status() != WL_CONNECTED) {  //Wait for the WiFI connection completion
      delay(500);
      Serial.println("[DEVICE] Waiting for connection");
    }

    if ((WiFi.status() == WL_CONNECTED)) {
        Serial.println("[DEVICE] WiFi connected ...");
    }

    //Starting the pin
    dht.begin();
    pinMode(PUMP, OUTPUT);
    digitalWrite(PUMP, HIGH);
    pinMode(VALVE, OUTPUT);  
    digitalWrite(VALVE, HIGH);
    pinMode(LAMP, OUTPUT);  
}

void separator() {
    Serial.println("------------------------------------------------------");
}

//Function to post into the server
String curl(String POST_DATA, String PATH) {
    //Serial.println("[HTTP] Post begin ...\n");
    http.begin(HOST + PATH); //Specify request destination
    http.addHeader("Content-Type", "application/x-www-form-urlencoded"); //Specify content-type header

    //Serial.println("[HTTP] Request: " + POST_DATA); //Print request response
    int httpCode = http.POST(POST_DATA); //Send the request
    String httpResponse = http.getString(); //Get the response

    if(httpCode > 0) {
        //Serial.print("[HTTP] Code: "); //Print HTTP return code
        //Serial.println(httpCode);
        //Serial.print("[HTTP] Response: "); //Print request response
        //Serial.println(httpResponse);
        return httpResponse;
    } else {
        //Serial.println("[HTTP] Failed to connecting to the API server. Retrying ... "); //Print to retrying request
        return curl(POST_DATA, PATH); //Tring new request
    }
    
    http.end();
}

//Function running irrigation
void runIrrigation(bool status) {
    if(status == true) {
        Serial.println("[DEVICE] Valve turn on ...");
        digitalWrite(VALVE, LOW); //turn the valve ON
        delay(VALVE_DELAY); //delay 1 minutes to throw water
        Serial.println("[DEVICE] Valve turn off ...");
        digitalWrite(VALVE, HIGH); //turn the valve OFF
        Serial.println("[DEVICE] Pump turn on ...");
        digitalWrite(PUMP, LOW); // turn the PUMP ON
        delay(PUMP_DELAY);//delay 1 minute to fill a new water and nutrition
        Serial.println("[DEVICE] Pump turn off ...");
        digitalWrite(PUMP, HIGH); //turn the pump OFF
    } else {
        digitalWrite(VALVE, HIGH); //turn the valve OFF
        digitalWrite(PUMP, HIGH); //turn the pump OFF
    }
}

//Function running lighting
void runLighting(bool status) {
    if(status == true) {
        Serial.println("[DEVICE] Lighting is on ...");
        digitalWrite(LAMP, LOW); //turn the lamp ON
    } else {
        Serial.println("[DEVICE] Lighting is off ...");
        digitalWrite(LAMP, HIGH); //turn the lamp OFF
    }
}

//Function to get last humidity & temperature
void humidityTemp() {
    separator();
    Serial.println("[DEVICE] Check Humidity, Temperature and Light Value");
    //Some code to get variabel HUMIDITY, TEMPERATURE, & LIGHT
    //Read humidity
    float HUMIDITY = dht.readHumidity();
    //Read temperature
    float TEMPERATURE = dht.readTemperature();
    //Read serial monitor
    int LIGHT = analogRead(LIGHTING);

    //Show current condition 
    Serial.print("[DEVICE] Humidity: ");
    Serial.println(HUMIDITY);
    Serial.print("[DEVICE] Temperature: ");
    Serial.println(TEMPERATURE);
    Serial.print("[DEVICE] Light Value: ");
    Serial.println(LIGHT);

    String postData = "api_key=" + API_KEY + "&humidity=" + HUMIDITY + "&temperature=" + TEMPERATURE;
    String humTemp = curl(postData, PATH_HUMIDITY_TEMP);

    String postDataLight = "api_key=" + API_KEY + "&light_value=" + LIGHT ;
    String lightResponse = curl(postDataLight, PATH_LIGHT_VALUE);

    Serial.print("[HTTP] Light Sensor Response: ");
    Serial.println(lightResponse);
    
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject& humidityTemp = jsonBuffer.parseObject(humTemp);
    JsonObject& lightJson = jsonBuffer.parseObject(lightResponse);

    bool isSave = humidityTemp["data"]["is_save"];
    String condition = humidityTemp["data"]["reason"];
    String conditionLight = lightJson["data"]["reason"];

    if(isSave == false) {
        Serial.println("[PLANT] Not in save condition ...");
        Serial.print("[PLANT] Current condition: ");
        Serial.println(condition);
        
        if(condition == "hot" && conditionLight == "hot") {
            //turn the valve ON Where a temperature so HOT and LDR HOT too
            Serial.println("[DEVICE] So HOT. Running irrigation ...");
            runIrrigation(true); //Running irrigation to coller the plant
        }
        if(condition == "cold" && conditionLight == "cold") {
            //turn the lighting ON Where a temperature so COLD and LDR COLD too
            Serial.println("[DEVICE] So COLD. Running lighting ...");
            runLighting(true); //Running lighting
        }
    } else {
        runIrrigation(false); //Turn off cooler or irrigation
        runLighting(false); //Turn off lighting
        Serial.println("[PLANT] In save condition ...");
    }
}

// Function to run automation of irrigation
void automationScheduling() {
    separator();
    Serial.println("[DEVICE] Check Automation Scheduling");
    String postData = "api_key=" + API_KEY;
    String conf = curl(postData, PATH_CONFIG);
    
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject& autoConfig = jsonBuffer.parseObject(conf);

    const char* IRRIGATION = autoConfig["data"]["irrigation"];
    bool SCHEDULED_TIME = autoConfig["data"]["schedule_status"];
    bool HARVEST_TIME = autoConfig["data"]["harvest_status"];
    const char* HARVEST_DAY = autoConfig["data"]["harvest_day"];

    Serial.print("[PLANT] Scheduled Time (True: 1 & False: 0): ");
    Serial.println(SCHEDULED_TIME);
    Serial.print("[PLANT] Is Harvest Time (True: 1 & False: 0): ");
    Serial.println(HARVEST_TIME);
    Serial.print("[PLANT] Harvest Day: ");
    Serial.println(HARVEST_DAY);
    Serial.print("[DEVICE] Irrigation: ");
    Serial.println(IRRIGATION);

    if(IRRIGATION == "on" && SCHEDULED_TIME == true) {
        runIrrigation(true); //Running irrigation
    } else if(IRRIGATION == "off" && SCHEDULED_TIME == false) {
        runIrrigation(false); //Turn off irrigation
    }
}

// Function to get action from remote control
void remoteControl() {
    separator();
    Serial.println("[DEVICE] Check Status of Remote Control");
    String postData = "api_key=" + API_KEY;
    String remoteJson = curl(postData, PATH_REMOTE_CONTROL);
    
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject& remote = jsonBuffer.parseObject(remoteJson);

    String IRRIGATION = remote["data"]["irrigation"];
    String LIGHTING = remote["data"]["lighting"];

    Serial.print("[DEVICE] Irrigation: ");
    Serial.println(IRRIGATION);
    Serial.print("[DEVICE] Lighting: ");
    Serial.println(LIGHTING);

    if(IRRIGATION == "on") {
        runIrrigation(true); //Running irrigation
    } else if(IRRIGATION == "off") {
        runIrrigation(false); //Turn off irrigation
    }

    if(LIGHTING == "on") {
        runLighting(true); //Running lighting
    } else if(LIGHTING == "off") {
        runLighting(false); //Turn off lighting
    }
}

void loop() {
    // Wait for WiFi connection
    if ((WiFi.status() == WL_CONNECTED)) {
        separator();
        Serial.println("[DEVICE] Ping and Create Session");

        String ping = curl("api_key=" + API_KEY, PATH_PING);
        Serial.print("[HTTP] Ping Response: ");
        Serial.println(ping);
        
        DynamicJsonBuffer jsonBuffer(bufferSize);
        JsonObject& pingJson = jsonBuffer.parseObject(ping);
  
        bool isValidAPI = pingJson["data"]["is_valid"];
        
        if(isValidAPI == true) {
            Serial.println("[HTTP] API Key registered and session updated ...");
            // Submit last humidity & temperature
            humidityTemp();
            // Get schedule
            automationScheduling();
            // Get action of manual control
            remoteControl();
        } else if(isValidAPI == false) {
            Serial.println("[HTTP] Invalid API Key ...");
        } else {
            Serial.println("[HTTP] Ping to server failed ...");
        }
    } else {
        Serial.println("[DEVICE] Error in WiFi connection ..."); 
    }

    delay(LOOP_DELAY);
}
