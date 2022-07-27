// ESP32 Modbus RTU to TCP converter
// thanks to https://github.com/eModbus/eModbus


#include <Arduino.h>
#include <WiFi.h>
#include "HardwareSerial.h"
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "Logging.h"

#include "ModbusBridgeWiFi.h"
#include "ModbusClientRTU.h"

#define SERVER_ADDR 20

#ifndef STASSID
#define STASSID "SSID"
#define STAPSK  "PASSWORD"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;
WiFiClient espClient;

const uint ServerPort = 23;
WiFiServer Server(ServerPort);

IPAddress local_IP(192, 168, 0, 189);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
uint16_t port = 502;                       // port of modbus server

ModbusBridgeWiFi MBbridge;
ModbusClientRTU MB(Serial2, 4, 2000);   //RTU on Serial2 with DE_RE pin 4 and timeout 2000

void setup_wifi() {
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  delay(200);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
    // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
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
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

}

void setup() {
  
  Serial.begin(9600);
  setup_wifi();
  Serial.println("setup ready");
  // Init Serial2 conneted to the RTU Modbus
  // UART2: Rx = GPIO16 Tx = GPIO17
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  MB.setTimeout(20000);
  MB.begin(1);

  // ServerID 4: RTU Server with remote serverID 1, accessed through RTU client MB - FC 03 accepted only
  //MBbridge.attachServer(4, 1, READ_HOLD_REGISTER, &MB);
  // Add FC 04 to it
  //MBbridge.addFunctionCode(4, READ_INPUT_REGISTER);

  // ServerID 5: RTU Server with remote serverID 10, accessed through RTU client MB - all FCs accepted
  MBbridge.attachServer(1, SERVER_ADDR, ANY_FUNCTION_CODE, &MB);

  // Remove FC 04 from it
  //MBbridge.denyFunctionCode(5, READ_INPUT_REGISTER);
  MBbridge.listServer();
  MBbridge.start(port, 4, 20000);
  Serial.printf("Use the shown IP and port %d to send requests!\n", port);
}

void loop() {

  ArduinoOTA.handle();
   // checking for WIFI connection
  if (WiFi.status() != WL_CONNECTED){
    Serial.println("Resetting");
    ESP.restart();
}
}