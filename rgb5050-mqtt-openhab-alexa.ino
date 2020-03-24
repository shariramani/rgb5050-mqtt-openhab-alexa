/*
  RGBLedExample
  Example for the RGBLED Library
  Created by Bret Stateham, November 13, 2014
  You can get the latest version from http://github.com/BretStateham/RGBLED
  sh: Changed 255 to 1024 in RGBLED.cpp
  red_esp= map(value_red_esp, 0, 255, 0, 1024);
  green_esp= map(value_green_esp, 0, 255, 0, 1024);
  blue_esp= map(value_blue_esp, 0, 255, 0, 1024);
  ESP pwm is 1024 steps while alexa gives color and brightness in 0-255 range. We map R,G,B values according to brightness set by Alexa.
*/
#include <FS.h>
#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
//#define ESPALEXA_DEBUG    //uncomment to see debug info in serial monitor
//#include <Espalexa.h>
#include <ESP8266Ping.h>
#include <NTPClient.h>
#include <TimeLib.h> //used in function displaying power on time in debug page
#include <EEPROM.h>
#include <RGBLED.h>
#include "index.h"
#include <PubSubClient.h>

const char* ssid = "Suresh";
const char* password = "mvls$1488";

IPAddress staticIP(192, 168, 1, 158);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dnsIP1(192, 168, 1, 1);
const char* deviceName = "kitchenled.com";


// Turn on debug statements to the serial output
#define  DEBUG  1

#if  DEBUG
#define  PRINT(s, x) { Serial.print(F(s)); Serial.print(x); }
#define PRINTS(x) Serial.print(F(x))
#define PRINTLN(x) Serial.println(F(x))
#define PRINTD(x) Serial.println(x, DEC)
//include ESP8266 SDK C functions to get heap
extern "C" {                                          // ESP8266 SDK C functions
#include "user_interface.h"
}

#else
#define PRINT(s, x)
#define PRINTS(x)
#define PRINTLN(x)
#define PRINTD(x)

#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//OTA Section
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <ESP8266HTTPUpdateServer.h>
const char* otahost = "rgb5050-webupdate";
const char* update_path = "/firmware";
const char* update_username = "admin";
const char* update_password = "admin";

ESP8266WebServer httpServer(90);  //for OTA
ESP8266HTTPUpdateServer httpUpdater;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//Web Interface
///////////////////////////////////////////////////////////////////////////////
#define HTTP_PORT 80

// QUICKFIX...See https://github.com/esp8266/Arduino/issues/263
//#define min(a,b) ((a)<(b)?(a):(b))
//#define max(a,b) ((a)>(b)?(a):(b))

ESP8266WebServer server(HTTP_PORT);
///////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////
//NTP
/////////////////////////////////////////////
WiFiUDP ntpUDP;

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 19800, 300000);

/////////////////////////////////////////////

time_t myPowerOnTime = 0; // Variable to store ESP Power on time
time_t prevEvtTime = 0; // Variable to store ping time

////////////////////////////////////////////////////////

//MQTT
const char* mqtt_server = "192.168.1.29";
WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;


//const int mqtt_port = 1883;
const char* mqtt_user = "admin";
const char* mqtt_password = "password";
const char* mqtt_colorLight_state_topic = "kitchenLed/color/state";
const char* mqtt_colorLight_command_topic = "kitchenLed/color/command";

//const char* mqtt_colorLight_clock_state = "colorLight/clock/state";
//const char* mqtt_colorLight_clock_command = "colorLight/clock/command";
///  const char* mqtt_colorLight_clock_dimmer_state = "kitchenLed/clock/dimmer/state";
///  const char* mqtt_colorLight_clock_dimmer_command = "kitchenLed/clock/dimmer/command";

///////////////////////////////


// Declare an RGBLED instanced named rgbLed
// Red, Green and Blue LED legs are connected to PWM pins 11,9 & 6 respectively
// In this example, we have a COMMON_ANODE LED, use COMMON_CATHODE otherwise
RGBLED rgbLed(D2, D1, D3, COMMON_CATHODE);

int R = 255;
int G = 255;
int B = 255;
boolean randomPattern = false;


String setcolor = "#ff00ff"; //Set color for HTML
//How long to show each color in the example code (in milliseconds);
int delayMs = 2000;

void setup() {
  //Initialize Serial communications
  Serial.begin(115200);
  Serial.println("\r\n");
  EEPROM.begin(512);


  // init WiFi
  WIFI_Connect();

  PRINT("\nConnected to ", ssid);
  PRINTS("\nIP address: ");
  Serial.println(WiFi.localIP());



  //Report the LED type and pins in use to the serial port...
  Serial.println("Welcome to the RGBLED Sample Sketch with Alexa integration");
  String ledType = (rgbLed.commonType == 0) ? "COMMON_CATHODE" : "COMMON_ANODE";
  Serial.println("Your RGBLED instancse is a " + ledType + " LED");
  Serial.println("And the Red, Green, and Blue legs of the LEDs are connected to pins:");
  Serial.println("r,g,b = " + String(rgbLed.redPin) + "," + String(rgbLed.greenPin) + "," + String(rgbLed.bluePin) );
  Serial.println("");


  ///////////////////////////////////////////////////////////////
  //HTTP Server
  //////////////////////////////////////////////////////////////

  Serial.println("HTTP server setup");
  server.on("/", srv_handleRoot);
  server.on("/form", handleForm);

  server.onNotFound(srv_handleNotFound);
  server.begin();
  Serial.println("HTTP server started.");

  Serial.println("ready!");

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //OTA Code
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  MDNS.begin(otahost);

  httpUpdater.setup(&httpServer, update_path, update_username, update_password);
  httpServer.begin();

  MDNS.addService("http", "tcp", 90);

  Serial.printf("HTTPUpdateServer ready! Open http://%s.local%s in your browser and login with username '%s' and password '%s'\n", otahost, update_path, update_username, update_password);

  //OTA code end
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////
  //NTP
  ///////////////////////////////////////////////
  timeClient.begin();

  //todo// lastUpdate = millis();

  //////////////////////////////////////////////


  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);   //MQTT Callback


  //Set the RGBLED to show RED only
  rgbLed.writeRGB(255, 0, 0);
  printRgbValues();
  delay(1000);

  //Set the RGBLED to show GREEN only
  rgbLed.writeRGB(0, 1024, 0);
  printRgbValues();
  delay(1000);

  //Set the RGBLED to show BLUE only
  rgbLed.writeRGB(0, 0, 1024);
  printRgbValues();
  delay(1000);
  //turn on RGB LED on previous stored color in EPROM
  rgbLed.turnOff();
  loadSettingsFromEEPROM(); //Load previous settings
  //rgbLed.writeRGB(R,G,B);
  rgbLed.writeRed(R);
  rgbLed.writeGreen(G);
  rgbLed.writeBlue(B);
  printRgbValues();

}

void loop() {

  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    WIFI_Connect();
  }

  else {  //do following only if WiFi is connected
    server.handleClient();
    httpServer.handleClient(); // OTA code

    timeClient.update();
    if ((myPowerOnTime == 0) && (timeClient.getEpochTime() > 1483228800)) {
      myPowerOnTime = timeClient.getEpochTime(); // Set the power on time after NTP sync. NTP sync is confirmed by comparing epoch of 1-Jan-2017 with epoch of time library.
    }

    //MQTT
    if (!mqttClient.connected()) {
      reconnect();
    }
    else {
      mqttClient.loop();
    }

    //Ping GW, if not reachable reset ESP.
    if ((millis() - prevEvtTime) > 360000) {          //every 6 min (6*60*1000 ms)
      prevEvtTime = millis();
      if (Ping.ping(gateway)) {
        Serial.print("\nGW Pinging!!");
      }
      else {
        Serial.print("\nGW not Pinging, Restarting Now :");
        ESP.restart();
      }
    }

  } //end of else

  rgbLed.writeRed(R);
  rgbLed.writeGreen(G);
  rgbLed.writeBlue(B);
  //printRgbValues();

  if (randomPattern) {
    //The code in the loop shows multiple exampls

    //Set the RGBLED to show RED only
    //printRgbValues() prints various LED values to the Serial port
    //you can monitor the serial port to see the values printed
    //The delay(delayMs) waits for 1 second to be able to see the color shown
    rgbLed.writeRGB(255, 0, 0);
    printRgbValues();
    handleClientInDelay(delayMs);

    //Set the RGBLED to show GREEN only
    rgbLed.writeRGB(0, 1024, 0);
    printRgbValues();
    handleClientInDelay(delayMs);

    //Set the RGBLED to show BLUE only
    rgbLed.writeRGB(0, 0, 1024);
    printRgbValues();
    handleClientInDelay(delayMs);

    //Set the RGBLED to show YELLOW (RED & GREEN) only
    rgbLed.writeRGB(1024, 1024, 0);
    printRgbValues();
    handleClientInDelay(delayMs);

    //Set the RGBLED to show ORANGE (RED & partial GREEN) only
    rgbLed.writeRGB(1024, 512, 0);
    printRgbValues();
    handleClientInDelay(delayMs);

    //Set the RGBLED to show PURPLE (RED & BLUE) only
    rgbLed.writeRGB(1024, 0, 1024);
    printRgbValues();
    handleClientInDelay(delayMs);

    //Set the RGBLED to show PINK (RED & partial BLUE) only
    rgbLed.writeRGB(1024, 0, 512);
    printRgbValues();
    handleClientInDelay(delayMs);

    //Set the RGBLED to show a random color
    rgbLed.writeRandom();
    printRgbValues();
    handleClientInDelay(delayMs);

    //Set the pins individually if needed
    rgbLed.writeRed(1024);
    rgbLed.writeGreen(1024);
    rgbLed.writeBlue(1024);
    printRgbValues();
    handleClientInDelay(delayMs);

    //The above code does the same thing as...
    rgbLed.writeRGB(1024, 1024, 1024);
    printRgbValues();
    handleClientInDelay(delayMs);

    //Show the color wheel
    Serial.println("Showing RGB Color Wheel...");
    Serial.println("------------------------------");
    //Use a 120ms delay between each color in the wheel, default was 25ms
    myColorWheel(120);

    //Turn off the RGBLED
    rgbLed.turnOff();
    printRgbValues();
    handleClientInDelay(100);
  }


} //end of loop

//printRgbValues prints the LED pins and values to the serial port
//You can monitor the serial port to see those values
void printRgbValues() {
  Serial.println("Requested RGB Values:");
  Serial.println("(r,g,b)=(" + String(rgbLed.redValue) + "," + String(rgbLed.greenValue) + "," + String(rgbLed.blueValue) + ")");
  Serial.println("Mapped RGB Values based on type (COMMON_ANODE or COMMON_CATHODE):");
  Serial.println("Mapped(r,g,b)=(" + String(rgbLed.redMappedValue) + "," + String(rgbLed.greenMappedValue) + "," + String(rgbLed.blueMappedValue) + ")");
  Serial.println("------------------------------");
}


void WIFI_Connect() {
  WiFi.disconnect();
  PRINTS("\nConnecting WiFi...");
  WiFi.hostname(deviceName);      // DHCP Hostname (useful for finding device for static lease)
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);
  WiFi.config(staticIP, gateway, subnet, dnsIP1);
  // Wait for connection
  for (uint8_t i = 0; i < 25; i++) {
    if (WiFi.status() != WL_CONNECTED)  {
      delay ( 500 );
      PRINTS ( "." );
    }
  }
} // end of WIFI_Connect



////////////////////////////////////////////////////////////////////////////
//Handle root
////////////////////////////////////////////////////////////////////////////
void srv_handleRoot() {
  Serial.println(server.arg(0) + "##" + server.arg(1) + "##" + server.arg(2) + "##" + server.arg("Random"));


  //R = server.arg("r").toInt();          // read RGB arguments
  //G = server.arg("g").toInt();
  //B = server.arg("b").toInt();

  if (server.arg("Random") == "On") {
    Serial.println("\nRandom On");
    randomPattern = true;
    saveSettingsToEEPROM();
  }
 else if (server.arg("Random") == "Off") {
    Serial.println("\nRandom Off");
    randomPattern = false;
    saveSettingsToEEPROM();
  }
 else if (server.hasArg("Brightness")) {
    int b=server.arg("Brightness").toInt();
      Serial.print("Brightness: ");
      Serial.print(b);
      b = (b*1024)/255; //ESP PWM range is 1024 whereas input is 255 range.
   //ESP pwm is 1024 steps while alexa gives color in 0-255 range. Lets map R,G,B values according to brightness set by Alexa
  R=map(R, 0, 1024, 0, b);
  G=map(G, 0, 1024, 0, b);
  B=map(B, 0, 1024, 0, b);
  rgbLed.writeRed(R);
  rgbLed.writeGreen(G);
  rgbLed.writeBlue(B);  
  printRgbValues();
    
  } 

  // String green = server.arg(1);
  //String blue = server.arg(2);

  //  analogWrite(R, red.toInt());
  //  analogWrite(G, green.toInt());
  //  analogWrite(B, blue.toInt());

  Serial.println(R);   // for TESTING
  Serial.println(G); // for TESTING
  Serial.println(B);  // for TESTING


  String p = index_html;
  p.replace("@@color@@", setcolor);

  if (WiFi.status() == WL_CONNECTED)
  {
    IPAddress ip = WiFi.localIP();
    String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
    p.replace("@@ip@@", ipStr);
  }

  server.send(200, "text/html", p);
}

//=======================================================================
//                    Handle Set Color
//=======================================================================
void handleForm() {
  String color = server.arg("color");
  //form?color=%23ff0000
  setcolor = color; //Store actual color set for updating in HTML
  Serial.println(color);

  //See what we have recived
  //We get #RRGGBB in hex string

  // Get rid of '#' and convert it to integer, Long as we have three 8-bit i.e. 24-bit values
  long number = (int) strtol( &color[1], NULL, 16);

  //Split them up into r, g, b values
  R = number >> 16;
  G = (number >> 8) & 0xFF;
  B = number & 0xFF;

  //ESP pwm is 1024 steps while webpage form gives color in 0-255 range.
  R = map(R, 0, 255, 0, 1024);
  G = map(G, 0, 255, 0, 1024);
  B = map(B, 0, 255, 0, 1024);
  //ESP supports analogWrite All IOs are PWM
  //Set the pins individually if needed
  randomPattern = false; //set randomPattern off
  rgbLed.writeRed(R);
  rgbLed.writeGreen(G);
  rgbLed.writeBlue(B);
  printRgbValues();


  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "Updated-- Press Back Button");
  saveSettingsToEEPROM();
  delay(500);
}
/////////////////////////////////////////////////////////////


void srv_handleNotFound() {
  //digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  //digitalWrite(led, 0);
}


////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////
//Keep checking for web client request while in delay
void handleClientInDelay(int x) {
  while (x > 50) {
    delay (50);
    x = x - 50;
    server.handleClient();
    httpServer.handleClient(); // OTA code
    if (!randomPattern) {
      return ;
    }
  }
  delay (x);
}
/////////////////////////////////////////////////////////////////////////////////////////

// Cycle through the color wheel, used from RGBLED.cpp
// Stolen from: http://eduardofv.com/read_post/179-Arduino-RGB-LED-HSV-Color-Wheel-
void myColorWheel(int dly) {
  //The Hue value will vary from 0 to 360, which represents degrees in the color wheel
  for (int hue = 0; hue < 360; hue++)
  {
    rgbLed.writeHSV(hue, 1, 1); //We are using Saturation and Value constant at 1
    handleClientInDelay(dly); //each color will be shown for 10 milliseconds
    if (!randomPattern) {
      return ;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////
//An "int" in ESP8266 takes 4 bytes, so it's a little more complicated, because EEPROM works in bytes, not ints.

void eeWriteInt(int pos, int val) {
  byte* p = (byte*) &val;
  EEPROM.write(pos, *p);
  EEPROM.write(pos + 1, *(p + 1));
  EEPROM.write(pos + 2, *(p + 2));
  EEPROM.write(pos + 3, *(p + 3));
  EEPROM.commit();
}

int eeGetInt(int pos) {
  int val;
  byte* p = (byte*) &val;
  *p        = EEPROM.read(pos);
  *(p + 1)  = EEPROM.read(pos + 1);
  *(p + 2)  = EEPROM.read(pos + 2);
  *(p + 3)  = EEPROM.read(pos + 3);
  return val;
}





void saveSettingsToEEPROM()
{
  Serial.println("Saving Settings to eeprom");
  eeWriteInt(10, R);
  eeWriteInt(20, G);
  eeWriteInt(30, B);
  EEPROM.write(4, randomPattern); //single bit
  EEPROM.commit();
}

void loadSettingsFromEEPROM()
{
  Serial.println("Reading Settings from eeprom");
  R = eeGetInt(10);
  G = eeGetInt(20);
  B = eeGetInt(30);
  randomPattern = EEPROM.read(4);
  Serial.println(R);   // for TESTING
  Serial.println(G); // for TESTING
  Serial.println(B);  // for TESTING
  Serial.println(randomPattern);  // for TESTING
}




////////////////////////////////////////////////////////////////////////////////////////////

//MQTT Functions
void callback(char* topic, byte* payload, unsigned int length) {
  PRINT("\nMessage arrived [", topic);
  PRINTS("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();


  char c_payload[length];
  memcpy(c_payload, payload, length);
  c_payload[length] = '\0';

  String s_topic = String(topic);
  String s_payload = String(c_payload);
  Serial.print(s_topic);
  Serial.println(s_payload);




  // Switch on/off lights or fan, change color
  if (s_topic == mqtt_colorLight_command_topic) {

    uint8_t valueR = s_payload.substring(s_payload.indexOf('(') + 1, s_payload.indexOf(',')).toInt();
    uint8_t valueG = s_payload.substring(s_payload.indexOf(',') + 1, s_payload.lastIndexOf(',')).toInt();
    uint8_t valueB = s_payload.substring(s_payload.lastIndexOf(',') + 1).toInt();
    PRINTLN("Loading color preset with following ");
    Serial.println(valueR);
    Serial.println(valueG);
    Serial.println(valueB);

    //ESP pwm is 1024 steps while alexa gives color in 0-255 range.
    R = map(valueR, 0, 255, 0, 1024);
    G = map(valueG, 0, 255, 0, 1024);
    B = map(valueB, 0, 255, 0, 1024);
    //ESP supports analogWrite All IOs are PWM
    //Set the pins individually if needed
    randomPattern = false; //set randomPattern off
    rgbLed.writeRed(R);
    rgbLed.writeGreen(G);
    rgbLed.writeBlue(B);
    printRgbValues();


    if ( valueR == 0 && valueG == 0 && valueB == 0) {
      randomPattern = false;
    }

    if ( valueR == 30 && valueG == 0 && valueB == 0) { // Little trick to play random color animation when Red color set at 30%
      randomPattern = true;
    }

  }

  saveSettingsToEEPROM();
}


void reconnect() {
  // Loop until we're reconnected
  for (uint8_t i = 0; i < 5; i++) {
    if (!mqttClient.connected()) {
      PRINTLN("Attempting MQTT connection...");
      Serial.print(F("Heap in loop start: "));
      Serial.println(system_get_free_heap_size());
      // Create a random client ID
      String mqttClientId = "ESP8266Client-";
      mqttClientId += String(random(0xffff), HEX);
      // Attempt to connect
      if (mqttClient.connect(mqttClientId.c_str(), mqtt_user, mqtt_password)) {
        PRINTLN("connected");
        // Once connected, publish an announcement...
        //mqttClient.publish("outTopic", "hello world");
        // ... and resubscribe
        if (mqttClient.subscribe(mqtt_colorLight_command_topic)) {
          PRINTLN("Subscribed: colorLight/color/command");
        }

      } else {
        PRINT("\nFailed, rc=", mqttClient.state());
        PRINTLN(" try again in 5 seconds");
        // Wait 5000 ms before retrying
        delay(5000);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
