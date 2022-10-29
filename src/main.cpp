// ESP32 Modbus RTU to TCP converter
// thanks to https://github.com/eModbus/eModbus


#include <Arduino.h>
#include <WiFi.h>
#include "HardwareSerial.h"
#include <WiFiUdp.h>
#include "ESPAsyncWebServer.h"
#include <AsyncTCP.h>
#include "SPIFFS.h"
#include "Logging.h"

#include "ModbusBridgeWiFi.h"
#include "ModbusClientRTU.h"

#define SERVER_ADDR 20

#ifndef STASSID
#define STASSID "Bas&Sanja"
#define STAPSK  "0499619079"
#endif
AsyncWebServer server(80);

const char* input_parameter1 = "ssid";
const char* input_parameter2 = "pass";
const char* input_parameter3 = "ip";
const char* input_parameter4 = "port";
const char* input_parameter5 = "slave_id";

//Variables to save values from HTML form
String ssid;
String pass;
String ip;
String port;
String slave_id;

// File paths to save input values permanently
const char* SSID_path = "/ssid.txt";
const char* Password_path = "/pass.txt";
const char* IP_path = "/ip.txt";
const char* port_path = "/port.txt";
const char* slave_id_path = "/slave_id.txt";

IPAddress localIP;
IPAddress gateway(192,168,0,1);
IPAddress subnet(255,255,255,0);

unsigned long previous_time = 0;
const long Delay = 10000; 

const int ledPin = 2;
String ledState;

// Read File from SPIFFS
String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if(!file || file.isDirectory()){
    Serial.println("- failed to open file for reading");
    return String();
  }
  
  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
    break;     
  }
  return fileContent;
}

// Write file to SPIFFS
void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- frite failed");
  }
}

// Initialize WiFi
bool initialize_Wifi() {
  if(ssid=="" || ip==""){
    Serial.println("Undefined SSID or IP address.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  localIP.fromString(ip.c_str());

  if (!WiFi.config(localIP, gateway, subnet)){
    Serial.println("STA Failed to configure");
    return false;
  }
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");

  unsigned long current_time = millis();
  previous_time = current_time;

  while(WiFi.status() != WL_CONNECTED) {
    current_time = millis();
    if (current_time - previous_time >= Delay) {
      Serial.println("Failed to connect.");
      return false;
    }
  }

  Serial.println(WiFi.localIP());
  return true;
}

// Replaces placeholder with LED state value
String processor(const String& var) {
  if(var == "GPIO_STATE") {
    if(digitalRead(ledPin)) {
      ledState = "ON";
    }
    else {
      ledState = "OFF";
    }
    return ledState;
  }
  return String();
}

const uint ServerPort = 23;
WiFiServer Server(ServerPort);

ModbusBridgeWiFi MBbridge;
ModbusClientRTU MB(Serial2, 4, 2000);   //RTU on Serial2 with DE_RE pin 4 and timeout 2000


void setup() {
  
  Serial.begin(9600);
 if (!SPIFFS.begin(true)) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  
  ssid = readFile(SPIFFS, SSID_path);
  pass = readFile(SPIFFS, Password_path);
  ip = readFile(SPIFFS, IP_path);
  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(ip);

  if(initialize_Wifi()) {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/index.html", "text/html", false, processor);
    });
    server.serveStatic("/", SPIFFS, "/");
    
    server.on("/led2on", HTTP_GET, [](AsyncWebServerRequest *request) {
      digitalWrite(ledPin, HIGH);
      request->send(SPIFFS, "/index.html", "text/html", false, processor);
    });

    server.on("/led2off", HTTP_GET, [](AsyncWebServerRequest *request) {
      digitalWrite(ledPin, LOW);
      request->send(SPIFFS, "/index.html", "text/html", false, processor);
    });
    server.begin();
  }
  else {
    Serial.println("Setting Access Point");
    WiFi.softAP("ESP32-WIFI-MANAGER", NULL);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP); 

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/wifimanager.html", "text/html");
    });
    
    server.serveStatic("/", SPIFFS, "/");
    
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == input_parameter1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile(SPIFFS, SSID_path, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == input_parameter2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            writeFile(SPIFFS, Password_path, pass.c_str());
          }
          // HTTP POST ip value
          if (p->name() == input_parameter3) {
            ip = p->value().c_str();
            Serial.print("IP Address set to: ");
            Serial.println(ip);
            // Write file to save value
            writeFile(SPIFFS, IP_path, ip.c_str());
          }
        }
      }
      request->send(200, "text/plain", "Success. ESP32 will now restart. Connect to your router and go to IP address: " + ip);
      delay(3000);
      ESP.restart();
    });
    server.begin();
  }
  // Init Serial2 conneted to the RTU Modbus
  // UART2: Rx = GPIO16 Tx = GPIO17
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  MB.setTimeout(20000);
  int SlaveID = slave_id.toInt();
  Serial.print(SlaveID);
  MB.begin(1);

  // ServerID 4: RTU Server with remote serverID 1, accessed through RTU client MB - FC 03 accepted only
  //MBbridge.attachServer(4, 1, READ_HOLD_REGISTER, &MB);
  // Add FC 04 to it
  //MBbridge.addFunctionCode(4, READ_INPUT_REGISTER);

  // ServerID 5: RTU Server with remote serverID 10, accessed through RTU client MB - all FCs accepted
  MBbridge.attachServer(1, SlaveID, ANY_FUNCTION_CODE, &MB);

  // Remove FC 04 from it
  //MBbridge.denyFunctionCode(5, READ_INPUT_REGISTER);
  MBbridge.listServer();
  int iPort = port.toInt();
  Serial.print(iPort);
  MBbridge.start(iPort, 4, 20000);
  Serial.printf("Use the shown IP and port %d to send requests!\n", iPort);
}

void loop() {

}