#include <ESP8266WiFi.h>        // WiFi Access Point
#include <WiFiClient.h>         // WiFi Client
#include <ESP8266WebServer.h>   // Webserver
#include <ESP8266HTTPClient.h>  // For the update function
#include <ESP8266mDNS.h>        // For the tracker.local name
#include <AccelStepper.h>       // Running the step motor
#include <ArduinoOTA.h>
#include <ArduinoJson.h>        // JSON encoding/decoding for the javascript interface
#include <LittleFS.h>           // Filesystem containing html, js, css etc.
#include <WiFiClient.h>         // Needed for HTTP Client
#include <EEPROM.h>             // USed for storing WiFi credentials, without storing them in the LittleFS



#include "version.h"            // File containing current version in VERSION

// TODOs
// OTA updates, with littlefs support
// Setup page, with SSID/password for network, with reboot to use new parameters (store SSID in flash)
// All submits via javascript. Auto update of parameters?
// OTA from a URL on github. With a VERSION file to compare if update is needed
// and a "check now" button on the setup page.
// With the OTA from github, the wifi.json file can not be served (contianing sensetive information)
// So it must be created on the first visit to the setup page. The fallback AP wifi can be hard coded.

// Intervalometer and web page from:
// https://github.com/DennisStav/ESP8266AstrophotographyIntervalometer

// ULN2003 Motor Driver Pins
// Stepper motor control pins
#define IN1 5
#define IN2 4
#define IN3 14
#define IN4 12

// Shutter release/auto focus on Nikon
#define SHUTTER 15

// the number of the pushbutton pin
 #define BUTTON 13

// Intervalometer variables, definitions and structs
unsigned long timenow=0, picstart=0, cooloffstart=0; 

enum astrostatus
{
  NOTREADY = 0,
  RUNNING = 1,
  STOPPED = 2
};

enum shutterstatus
{
  SCLOSED = 0,
  SOPEN = 1,
  COOLING=2
};

struct astroconf
{
  unsigned short exptime; 
  unsigned short cooloff;
  unsigned short numPics;
  unsigned short picscomp;
  astrostatus curstatus;
  shutterstatus curshutterstatus;
} astrojob;

String astroStatusToString(astrostatus dummyStat)
{
  String retVal;
  switch (dummyStat)
  {
    case NOTREADY:
      retVal="NOTREADY";
      break;
    case RUNNING:
      retVal="RUNNING";
      break;
    case STOPPED:
      retVal="STOPPED";
      break;
    default:
      retVal="Uknown Proc Status";
  }
  return retVal;
}

String shutterStatusToString(shutterstatus dummyStat)
{
  String retVal;
  switch (dummyStat)
  {
    case SOPEN:
      retVal="OPEN";
      break;
    case SCLOSED:
      retVal="CLOSED";
      break;
    case COOLING:
      retVal="COOLING";
      break;
    default:
      retVal="Uknown Shutter Status";
  }
  return retVal;
}

// initialize the stepper library
AccelStepper stepper(AccelStepper::HALF4WIRE, IN1, IN3, IN2, IN4);

// Variables for the configured client WiFi connection
String ssid;
String password;

// Static credentials for the Access Point WiFi (fallback)
const char *apssid = "tracker";
const char *appassword = "tracker123";

// Tracking or stopped (with manual control)
// 0 = stop, 1 = run
int run = 0;

/* 
  100 turns for 24h. 4096 steps for one turn
  204800 * 2 steps in 24h

  But celestial day is only 23h 56m 4s = 86164 seconds
  So 204 800 * 2 / 86164 = 4.75372546 spes/sec
  For northen hemisphere: Negative rotation needed on our tripod
*/
float speed = -4.7537255;  // Number rounded to what the ESP8266 can store
int manualSpeed = 0;

/* Button debounce control variables: */
int buttonState;             // the current reading from the input pin
int lastButtonState = 1;   // the previous reading from the input pin

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

ESP8266WebServer intwebserver(80);

// page.h contains two versions of the html page and the status javascript
// After changing the tracker.html or status.js files, run the make.sh script to update page.h
// #include "page.h"

String tracker_page;
String setup_page;
String css_page;
String js_page;

// String selected_page;

String error_page="<html>Error reading filesystem, ESP8266 broken?</html>";


int writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++)
  {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
  return addrOffset + 1 + len;
}

int readStringFromEEPROM(int addrOffset, String *strToRead, int max)
{
  int newStrLen = EEPROM.read(addrOffset);
  if (newStrLen > max) {
    Serial.println("EEPROM not meaningfull, skipping");
    return 0;  // Not meaningful SSID or password, so offset not important
  }
  char data[newStrLen + 1];
  for (int i = 0; i < newStrLen; i++)
  {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[newStrLen] = '\0';
  *strToRead = String(data);
  Serial.println("Found in EEPROM: "+String(data));
  return addrOffset + 1 + newStrLen;
}

void store_wifi() {

  // Storing global variable ssid and password in EEPROM
  // Both need to be of type String
  // Fresh ESP/program should have ssid/password as empty strings.
  // Thus nothing to save...
  if (ssid.length() > 0 && password.length() > 0) {

    Serial.println("'"+ssid+"'");
    Serial.print("SSID Length: ");
    Serial.println(ssid.length());
    Serial.println("'"+password+"'");
    Serial.println("");

    Serial.println("writing eeprom ssid:");
    EEPROM.begin(512); //Initialasing EEPROM
    int pwdOffset = writeStringToEEPROM(0, ssid);
    writeStringToEEPROM(pwdOffset, password);
    EEPROM.end();
    // EEPROM.commit();
  }
}

// read_wifi() must be called before store_wifi()
void read_wifi() {
  String essid;
  String epassword;

  Serial.println("Reading EEPROM");

  EEPROM.begin(512); //Initialasing EEPROM
  int pwdOffset = readStringFromEEPROM(0, &essid,32);
  readStringFromEEPROM(pwdOffset, &epassword,64);
  if (essid.length() > 0 && epassword.length() > 0) {
    ssid = essid;
    password = epassword;
    Serial.println("SSID after reading from eeprom: '"+ssid+"'");
  } else {
    Serial.println("Not using the following:");
    Serial.println("SSID: "+essid);
    Serial.println("Password: "+epassword);
  }
  EEPROM.end();
}


void start_astropic() {
  Serial.println("GET start astropic called");
  if (intwebserver.args() == 0) {
    intwebserver.send(200,"text/plain", "ERROR: No Arguments");
  } else {
    for (uint8_t i = 0; i < intwebserver.args(); i++) {
      if ( intwebserver.argName(i) == "exptime" ) {
          astrojob.exptime=intwebserver.arg(i).toFloat();
      } else if ( intwebserver.argName(i) == "cooloff" ) {
          astrojob.cooloff=intwebserver.arg(i).toFloat();
      } else if ( intwebserver.argName(i) == "numPics" ) {
          astrojob.numPics=intwebserver.arg(i).toInt();
      }
    }
    astrojob.curstatus=RUNNING;
    astrojob.picscomp=0;
    intwebserver.send(200,"text/json", "{}");
  }
  return;
}

void stop_astropic() {
  Serial.println("GET stop astropic called");
  astrojob.curstatus=STOPPED;
  intwebserver.send(200,"text/json", "{}");
}

// GET request to set speed!
void get_speed() {
  Serial.println("GET Speed Config called");
  if (intwebserver.args() != 0) {
    Serial.println("Arguments:");
    Serial.println(intwebserver.args());
    for (uint8_t i = 0; i < intwebserver.args(); i++) {
      if ( intwebserver.argName(i) == "speed" ) {
        speed=intwebserver.arg(i).toFloat();
        stepper.setSpeed(speed);
        Serial.println("New speed:");
        Serial.println(speed);
      }
    }
  }
  get_status();
}

void get_set() {
  Serial.println("GET Set Config called");
  if (intwebserver.args() != 0) {
    Serial.println("Arguments:");
    Serial.println(intwebserver.args());
    for (uint8_t i = 0; i < intwebserver.args(); i++) {
      if ( intwebserver.argName(i) == "speed" ) {
        speed=intwebserver.arg(i).toFloat();
        stepper.setSpeed(speed);
        Serial.println("New speed:");
        Serial.println(speed);
      } else if ( intwebserver.argName(i) == "ssid") {
        ssid = intwebserver.arg(i);
        Serial.println("New SSID: '"+ssid+"'");
      } else if ( intwebserver.argName(i) == "password") {
        password = intwebserver.arg(i);
      }
    }
    store_wifi();
    writeWiFi();
  }
  get_setup();
}


void config_rest_server_routing() {
    Serial.println("Registering web server paths");
    intwebserver.on("/", HTTP_GET, get_configpage);
    intwebserver.on("/start", HTTP_GET, start_astropic);
    intwebserver.on("/stop", HTTP_GET, stop_astropic);
    intwebserver.on("/status", HTTP_GET, get_status);
    intwebserver.on("/speed", HTTP_GET, get_speed);
    intwebserver.on("/set", HTTP_GET, get_set);
    intwebserver.on("/status.js", HTTP_GET, get_status_js);
    intwebserver.on("/setup.js", HTTP_GET, get_setup_js);
    intwebserver.on("/setup", HTTP_GET, get_setup);
    intwebserver.on("/def.css", HTTP_GET, get_css);
    intwebserver.on("/setup.html", HTTP_GET, get_setup_html);
    intwebserver.on("/check", HTTP_GET, get_check);
    intwebserver.on("/update", HTTP_GET, get_update);
    intwebserver.on("/restart", HTTP_GET, get_restart);
}

void get_restart() {
  const char* response ="{\"status\":\"Restart in progres\"}";
  intwebserver.send(200, F("application/json"), response);
  delay(300);  // Just to make sure the response is sendt
  ESP.restart();
}

// Planning to check github VERSION file and possibly
// download updated firmware and filesystem.
// Currently only checking dev host...
void get_check(){
  Serial.println("Get check for firmware update");

  String URL="http://192.168.10.51:8000/VERSION";
  WiFiClient client;

  DynamicJsonDocument doc(512); 

  HTTPClient http;
  http.begin(client, URL.c_str());
  int httpResponseCode = http.GET();
  if (httpResponseCode>0) {
    String payload = http.getString();
    payload.remove(payload.length()-1);
    doc["new_version"] = payload.toInt();
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    doc["new_version"] = "Error";
  }
  doc["version"] = VERSION;

  // Free resources
  http.end();
  String buf;
  serializeJson(doc, buf);
  intwebserver.send(200, F("application/json"), buf);
}

void get_update() {
  Serial.println("Serving update request");
}

void get_configpage(){
  Serial.println("Serving main page");
  File file = LittleFS.open("tracker.html.gz", "r");
  size_t sent = intwebserver.streamFile(file, F("text/html"));
  file.close();
}

void get_setup_html() {
  Serial.println("Serving setup_html");
  File file = LittleFS.open("setup.html.gz", "r");
  size_t sent = intwebserver.streamFile(file, F("text/html"));
  file.close();
}


void get_setup_js() {
  Serial.println("Serving setup.js");
  File file = LittleFS.open("setup.js.gz", "r");
  size_t sent = intwebserver.streamFile(file, F("text/javascript"));
  file.close();
}

void get_css() {
  Serial.println("Serving def.css");
  File file = LittleFS.open("def.css.gz", "r");
  size_t sent = intwebserver.streamFile(file, F("text/css"));
  file.close();
}

void get_status_js() {
  Serial.println("Serving status.js (gz)");
    File file = LittleFS.open("status.js.gz", "r");
  size_t sent = intwebserver.streamFile(file, F("text/javascript"));
  file.close();
}

void get_status() {
  Serial.println("Get status called");
  DynamicJsonDocument doc(512); 
  doc["status"] = astroStatusToString(astrojob.curstatus);
  doc["tracking"] = String(run);
  char buffer[16];
  sprintf(buffer, "%.8f", speed);
  doc["speed"] = buffer;
  doc["shutter"] = shutterStatusToString(astrojob.curshutterstatus);
  doc["completed"] = String(astrojob.picscomp);
  doc["pics"] = String(astrojob.numPics);
  if (astrojob.curstatus==RUNNING) {
    doc["current"] = String(astrojob.picscomp+1);
    doc["seconds"] = String((timenow-picstart)/1000);
    doc["exptime"] = String(astrojob.exptime);
    doc["waittime"] = String(astrojob.cooloff);
  } else {
    doc["current"] = String("N/A");
    doc["seconds"] = String("N/A");
    doc["exptime"] = String("N/A");
    doc["waittime"] = String("N/A");
  }
  String buf;
  serializeJson(doc, buf);
  intwebserver.send(200, F("application/json"), buf);
}

void get_setup() {
  Serial.println("Get setup called");
  DynamicJsonDocument doc(512);
  Serial.println("SSID: '"+ssid+"'");
  Serial.println("Password: '"+password+"'");
  Serial.println(ssid.length());
  doc["wifi_ssid"] = ssid;
  doc["wifi_password"] = password;
  doc["speed"] = speed;
  doc["version"] = VERSION;
  Serial.print("Version: ");
  Serial.println(VERSION);

  String buf;
  serializeJson(doc, buf);
  Serial.println(buf);
  intwebserver.send(200, F("application/json"), buf);
}

void init_resource()
{
    Serial.println("Initializing resources");
    astrojob.exptime=0;
    astrojob.cooloff=0;
    astrojob.numPics=0;
    astrojob.picscomp=0;
    astrojob.curstatus=STOPPED;
    pinMode(SHUTTER,OUTPUT);
    digitalWrite(SHUTTER,LOW);
    closeShutter();
}

void take_pic()
{
      astrojob.curstatus=RUNNING;
      astrojob.picscomp=0;
      astrojob.exptime=3;
      astrojob.cooloff=1;
      astrojob.numPics=5;
}

void openShutter()
{
    Serial.println("Open Shutter");
    digitalWrite(SHUTTER, HIGH);
    astrojob.curshutterstatus=SOPEN;
    picstart=millis();
}

void closeShutter()
{
    Serial.println("Close Shutter");
    digitalWrite(SHUTTER, LOW);
    astrojob.curshutterstatus=SCLOSED;
    if(astrojob.curstatus==RUNNING){
      //Just finished another picture, count it.
      astrojob.picscomp=astrojob.picscomp+1;
    }//No need to cound if we are stopping
    cooloffstart=millis();
}

void doPics()
{
  //put pic logic here.
  timenow=millis();

  if(astrojob.curstatus==RUNNING) //We are in the middle of a run
  {
    if(astrojob.picscomp<astrojob.numPics){ //Have we finished the pics?
      if(astrojob.curshutterstatus==SOPEN) //the shutter is currently open
      {
        //Need to convert millis to sec - multiply by 1000
        if((timenow-picstart)>=1000*astrojob.exptime) //we have finished the exposure
        {
          Serial.println("Start Cool-off after "+String(timenow-picstart)+"ms");
          closeShutter();
          astrojob.curshutterstatus=COOLING; //start the cooloff
        }
      }
      else if(astrojob.curshutterstatus==COOLING) //we are currently cooling
      {
        //Need to convert millis to sec - multiply by 1000
        if((timenow-cooloffstart)>=1000*astrojob.cooloff) //Cooloff has finished, start another picture
        {
          Serial.println("Start Pic after cool off after "+String(timenow-cooloffstart)+"ms");
          openShutter();
        }
      }else{ //Shutter is closed, but we are in the middle of a run, start pic
        Serial.println("Start First Pic");
        openShutter();     
      }
    }
    else //we hace finished a run, let's put status to stopped so we can stop everything
    {
      Serial.println("Stopping");
      astrojob.curstatus=STOPPED;
    }
  }
  else //We are in STOPPED mode - close shutter if open.
  {
      if(astrojob.curshutterstatus!=SCLOSED)
      {
        Serial.println("Make sure everything is closed");
        closeShutter();
      }
  }
}

#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 20

int init_wifi() {
    int retries = 0;

    Serial.println("Connecting to WiFi AP..........");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    // check the status of WiFi connection to be WL_CONNECTED
    while ((WiFi.status() != WL_CONNECTED) && (retries < MAX_WIFI_INIT_RETRY)) {
        retries++;
        delay(WIFI_RETRY_DELAY);
        Serial.print("#");
    }
    return WiFi.status(); // return the WiFi connection status
}

int init_wifiAP(){
  Serial.println("Configuring access point:"+String(apssid));
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apssid, appassword);  
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP status:"+String(WiFi.status())+"\n");
  Serial.print("AP IP address:");
  Serial.println(myIP);
  return WiFi.status();
}

// Start LittleFS and read in stored configuration
void readFS() {
 Serial.println("Start LittleFS:");
 if (!LittleFS.begin()) {
  Serial.println("An Error has occurred while mounting LittleFS");
 }
 File file = LittleFS.open("wifi.json", "r");
 DynamicJsonDocument doc(128);
 deserializeJson(doc, file);
 file.close();
 speed=doc["speed"];
}

// Write global variable ssid and password to wifi.json
void writeWiFi() {
  DynamicJsonDocument doc(128);
//  doc["ssid"]=ssid;
//  doc["password"]=password;
  char buffer[16];
  sprintf(buffer, "%.8f", speed);
  doc["speed"] = buffer;
  File file = LittleFS.open("wifi.json", "w");
  serializeJson(doc, file);
  serializeJson(doc, Serial);
  file.close();
}

void setup() {
 pinMode(LED_BUILTIN, OUTPUT);
 // Tracker not ready
 digitalWrite(LED_BUILTIN, 0);
 //delay(3000);

 init_resource();
 astrojob.curstatus=STOPPED;
 
 Serial.begin(115200);
 Serial.println();

 read_wifi();

 readFS();

  Serial.print("SSID: '");
  Serial.print(ssid);
  Serial.println("'");

  Serial.print("Password: '");
  Serial.print(password);
  Serial.println("'");

  Serial.print("Speed: '");
  Serial.print(speed);
  Serial.println("'");

 if (init_wifi() == WL_CONNECTED) {
  Serial.print("Connetted to ");
  Serial.print(ssid);
  Serial.print("--- IP: ");
  Serial.println(WiFi.localIP());
 } else {
  //Couldn't connect as client, setup our own AP.
  Serial.print("Error connecting to: ");
  Serial.println(ssid);
  init_wifiAP();
 }

 Serial.println("WiFi connected");
 Serial.println("IP address: ");
 Serial.println(WiFi.localIP());

 ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();

 if (!MDNS.begin("tracker")) {
  Serial.println("Error setting up MDNS responder!");
  Serial.println("The tracker.local address will not work");
 } else {
   Serial.println("mDNS responder started");
 }

 config_rest_server_routing();
 intwebserver.begin();
 Serial.println ( "HTTP server started" );

 MDNS.addService("http", "tcp", 80);

 stepper.setMaxSpeed(500);
 stepper.setAcceleration(100);
 stepper.setSpeed(speed);

 pinMode(BUTTON, INPUT);
 
  // Tracker ready
  digitalWrite(LED_BUILTIN, 1);
}

int i=0;
int j=0;

void loop() {
 stepper.runSpeed();
 doPics();
 MDNS.update();
 ArduinoOTA.handle();
 stepper.runSpeed();
 intwebserver.handleClient();
 stepper.runSpeed();
  if ( run == 0 ) {
   if ( i == 2000 ) {
     i=0;
     manualSpeed = analogRead(A0) - 512;
     if (abs(manualSpeed) < 100) {
       manualSpeed = 0;
     }
     stepper.setSpeed(manualSpeed);
   } else {
     i=i+1;
   }
 }
 stepper.runSpeed();

 /* Button debounce and toggle run */
 int reading = digitalRead(BUTTON);
 if (reading != lastButtonState) {
    lastDebounceTime = millis();
 }
 if ((millis() - lastDebounceTime) > debounceDelay) {
   if (reading != buttonState) {
     buttonState = reading;
     if (buttonState == 0) {
       if ( run == 0 ) {
         run = 1;
         // Fix slack in gears by tracking fast back/forward
         // Ignore the loop, just do it here.
         stepper.setSpeed(speed*-100);
         j=0;
         while (j<3000) {
          stepper.runSpeed();
          delay(1);
          j=j+1;
         }
         j=0;
         stepper.setSpeed(speed*100);
         while (j<3000) {
          stepper.runSpeed();
          delay(1);
          j=j+1;
         }
         // Reset speed, start tracking
         stepper.setSpeed(speed);
       } else {
         run = 0;
       }
     }
   }
 }
 lastButtonState = reading;
 stepper.runSpeed();
}