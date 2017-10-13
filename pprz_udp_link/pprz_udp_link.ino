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

#define LED_PIN 13

uint8_t logheader[] = "LOG package start!"; // 
uint8_t logfooter[] = "End log";
/* PPRZ message parser states */
enum log_states {
  HEADER,
  LENGTH,  
  DATA
};
log_states log_state;

#define LOGBUFSIZE 4096
uint8_t logBuf[LOGBUFSIZE]; //buffer to hold outgoing log packet incoming from serial
uint8_t outbuf[256]; //buffer to hold outgoing other packet incoming from serial
uint8_t serial_connect_info = 0; // Serial print wifi connection info

WiFiUDP udp;

WiFiClient client;
WiFiServer server(666);

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
    
    WiFi.softAP(ssid, password,7);
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

  server.begin();
  server.setNoDelay(true);
  
}

void loop() {
  
  /* Check for OTA */
  ArduinoOTA.handle();

  /* Check for UDP data from host */
  int packetSize = udp.parsePacket();
  if(packetSize) { /* data received on udp line*/
    // read the packet into packetBufffer
    uint8_t buf[packetSize];
    size_t len = udp.read(buf, packetSize);
    Serial.write(buf, len);
  }

  //direct data transfer for log
  if (server.hasClient() && !client.connected()){
    if (client)
      client.stop();
    client=server.available();
  } else {
    server.available().stop();
  }

  size_t len = Serial.available();
  uint8_t sbuf[len];   
  if (len > 0) {               
    Serial.readBytes(sbuf, len);
    //check for log stuff
    for (int i = 0 ; i < len; i++) {
      check_log_package(sbuf[i]);
    }    
  
    if (log_state==LENGTH || log_state==DATA) {
      //TODO: ignore log data for udp sender
    } else {  //normal datalink telemetry      
      udp.beginPacketMulticast(broadcastIP, txPort, myIP);    
      udp.write(sbuf, len);
      udp.endPacket();
      delay(1);
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }     
  }
}

int cnt;
int pcnt;
int plength;
void check_log_package(uint8_t b) {
  switch (log_state) {

  case HEADER:
  if (b == logheader[cnt]){
    cnt ++;
    if (cnt == 18) {
      log_state=LENGTH;
      plength=0;
      cnt =0;
    }
  } else
    cnt == 0;
  case LENGTH:
    plength += b << (cnt*8);
    cnt ++;
    if (cnt> 3) {
      cnt = 0;
      log_state=DATA;
      pcnt=0;
    }
  case DATA:    
    logBuf[pcnt] = b;
    cnt++;
    pcnt++;
    if (pcnt >= LOGBUFSIZE) {      
      send_log_buf(LOGBUFSIZE);
      pcnt=0;
    }    
    if (cnt >= plength){
      send_log_buf(pcnt);
      pcnt=0;
      cnt=0;
      log_state=HEADER;
    }
  default:
      // Should never get here
      break;
  }
}

void send_log_buf(int len) {
  if (client && client.connected()) {
        //send log data over tcp           
        client.write((const uint8_t*)logBuf, len);
        delay(1);
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));    
      }
}

