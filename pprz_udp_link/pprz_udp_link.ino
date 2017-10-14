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

#define BUFSIZE 256
char packetBuffer[BUFSIZE]; //buffer to hold incoming packet
char outBuffer[BUFSIZE];    //buffer to hold outgoing data
uint8_t serial_connect_info = 0; // Serial print wifi connection info

char logheader[10];

#define LOGBUFSIZE 2048
uint8_t logBuf[LOGBUFSIZE] = {0}; //buffer to hold outgoing log packet incoming from serial 

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

  String cs = "log_st";
  cs.toCharArray(logheader,15);
  //for (int i = 0; i<LOGBUFSIZE;i++)
//    logBuf[i] = 66;
  
}

static int state = 0;
static int lcnt = 0;
  
void loop() {
  
  /* Check for OTA */
  ArduinoOTA.handle();


  if (server.hasClient() && !client.connected()){ 
    if (client) 
      client.stop(); 
    client=server.available(); 
  } else { 
    server.available().stop(); 
  }

  /* Check for UDP data from host */
  int packetSize = udp.parsePacket();
  size_t len = 0;
  if(packetSize) { /* data received on udp line*/
    // read the packet into packetBufffer
    len = udp.read(packetBuffer, 255);
    Serial.write(packetBuffer, len);
  }

  len = Serial.available(); 
  if (len > 0) {
    uint8_t sbuf[len];
    Serial.readBytes(sbuf, len); 
    size_t flen = len;     

    //the log begins with LOGHEADER, then LOGBUFSIZE bytes
    for (int i = 0 ; i < len; i++) {
      if (state != 6) {
        if (sbuf[i] == logheader[state]) 
          state++;
        else
          state = 0;

      } else if (state == 6 && lcnt < LOGBUFSIZE) {
        flen = 0;
        logBuf[lcnt] = sbuf[i];
        lcnt++;
      } 
      if (state == 6 && lcnt == LOGBUFSIZE) {        
        send_log_buf(LOGBUFSIZE);
        flen = 0;
        lcnt = 0;
        state = 0;
      }
    }
    
    if (flen > 0) {
      udp.beginPacketMulticast(broadcastIP, txPort, myIP);     
      udp.write(sbuf, flen); 
      udp.endPacket(); 
      delay(10); 
      digitalWrite(LED_PIN, !digitalRead(LED_PIN)); 
    }
  } 
}

int counter = 0;
void send_log_buf(int len) { 
  if (client && client.connected()) { 
        client.write((const uint8_t*)logBuf, LOGBUFSIZE); 
        delay(1); 
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));     
      } 
} 

