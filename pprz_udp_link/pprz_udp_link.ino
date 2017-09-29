#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "wifi_config.h"

/* Struct with available modes */
enum wifi_modes {
  WifiModeClient,
  WifiModeAccessPoint
} wifi_mode = WIFI_MODE;

#define PPRZ_STX 0x99
#define LED_PIN 13

/* PPRZ message parser states */
enum normal_parser_states {
  SearchingPPRZ_STX,
  ParsingLength,
  ParsingSenderId,
  ParsingMsgId,
  ParsingMsgPayload,
  CheckingCRCA,
  CheckingCRCB
};

struct normal_parser_t {
  enum normal_parser_states state;
  unsigned char length;
  int counter;
  unsigned char sender_id;
  unsigned char msg_id;
  unsigned char payload[256];
  unsigned char crc_a;
  unsigned char crc_b;
};

struct normal_parser_t parser;
#define BUFSIZE 256
char packetBuffer[BUFSIZE]; //buffer to hold incoming packet
char outBuffer[BUFSIZE];    //buffer to hold outgoing data
uint8_t serial_connect_info = 1; // Serial print wifi connection info

WiFiUDP udp;

IPAddress myIP;

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  
  wifi_mode = WIFI_MODE;
  delay(1000);
  /* Configure LED */
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  if (serial_connect_info) {
    Serial.println();
    Serial.print("Connnecting to ");
    Serial.println(ssid);
  }
  if (wifi_mode == WifiModeClient) {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      if (serial_connect_info) {
        Serial.print(".");
      }
      /* Toggle LED */
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
    myIP = WiFi.localIP();
  }
  else {
    /* AccessPoint mode */

    Serial.print("$");
    
    WiFi.softAP(ssid, password);
    myIP = WiFi.softAPIP();
    /* Reconfigure broadcast IP */
    IPAddress AP_broadcastIP(192,168,4,255);
    broadcastIP = AP_broadcastIP;
  }
  
  if (serial_connect_info) {
    Serial.println(myIP);
  }

  udp.begin(localPort);


  /* OTA Configuration */

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("esp-module");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

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

  /* Connected, LED ON */
  digitalWrite(LED_PIN, HIGH);
}

void loop() {
  
  /* Check for OTA */
  ArduinoOTA.handle();

  /* Check for UDP data from host */
  int packetSize = udp.parsePacket();
  int len = 0;
  if(packetSize) { /* data received on udp line*/
    // read the packet into packetBufffer
    len = udp.read(packetBuffer, 255);
    Serial.write(packetBuffer, len);
  }

/*
  //is it really necessary to put stuff in a buffer????
  int cnt = 0;  
  while(Serial.available() > 0) {
    unsigned char inbyte = Serial.read();
    outBuffer[cnt] = inbyte ;
    cnt ++;
    if (cnt >= BUFSIZE) {    
      sendBuffer(cnt);
    }
  }
  if (cnt > 0) {
    sendBuffer(cnt);
  }  

*/
  if (Serial.available() > 0) {
    udp.beginPacketMulticast(broadcastIP, txPort, myIP);    
    while(Serial.available() > 0) {    
      udp.write(Serial.read());
    }
     udp.endPacket();
     digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }

}


void sendBuffer(int cnt) {
  udp.beginPacketMulticast(broadcastIP, txPort, myIP);
  udp.write(outBuffer, cnt);
  udp.endPacket();
  digitalWrite(LED_PIN, !digitalRead(LED_PIN));
}


