/**
 * The MySensors Arduino library handles the wireless radio link and protocol
 * between your home built sensors/actuators and HA controller of choice.
 * The sensors forms a self healing radio network with optional repeaters. Each
 * repeater and gateway builds a routing tables in EEPROM which keeps track of the
 * network topology allowing messages to be routed to nodes.
 *
 * Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
 * Copyright (C) 2013-2015 Sensnology AB
 * Full contributor list: https://github.com/mysensors/Arduino/graphs/contributors
 *
 * Documentation: http://www.mysensors.org
 * Support Forum: http://forum.mysensors.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 *******************************
 *
 * DESCRIPTION
 *
 * Simple binary switch example 
 * Connect button or door/window reed switch between 
 * digitial I/O pin 3 (BUTTON_PIN below) and GND.
 * http://www.mysensors.org/build/binary
 */


// WiFi OTA
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// 1wire
#include <OneWire.h>
#include <DallasTemperature.h>

// Timer
#include <SimpleTimer.h>
SimpleTimer timer;

const char* ssid = "maison";
const char* password = "password";
// MySensors
// Enable debug prints to serial monitor
#define MY_DEBUG 
#define MY_RADIO_RFM95
#define MY_RFM95_IRQ_PIN 26
#define MY_RFM95_RST_PIN 14
#define MY_REPEATER_FEATURE
#define MY_NODE_ID 10


//#include <SPI.h>
#include <MySensors.h>

// 1wire
#define ONE_WIRE_BUS 17
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

#define TEMP_ID 3
#define ALERT_ID  4

#define LEDOK   12  // Température correcte (LED verte)
#define COK 0       // Canal LED Ok
#define LEDFROID 25 // Température congélation (LED bleue)
#define LEDALERT 13 // Température trop élevée (LED rouge)

#define INTERVALLE  300 // Intervalle de mesures en secondes

#define TALERT  -17     // Température d'alerte
#define TCONG   -30     // Température congélation

// Change to V_LIGHT if you use S_LIGHT in presentation below
MyMessage msgTemp(TEMP_ID,V_TEMP);
MyMessage msgAlert(ALERT_ID,V_LIGHT);

void setup()  
{  
  // WiFi
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int nb=5;
  while ((WiFi.waitForConnectResult() != WL_CONNECTED) && (nb>0)) {
    Serial.println("Connection Failed! Restart...");
    //WiFi.begin(ssid, password);
    nb--;
    delay(5000);
    //ESP.restart();
  } 
  
  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("Congelateur");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  //pinMode(LEDOK,OUTPUT);
  pinMode(LEDALERT,OUTPUT);
  pinMode(LEDFROID,OUTPUT);

  // 1wire
  sensors.begin();
  timer.setInterval(INTERVALLE*1000L,GetTemp);
  GetTemp();

  // LEDs
  ledcAttachPin(LEDOK,COK);
  ledcSetup(COK,5000,8);
  ledcWrite(COK, 32);
  }

void presentation() {
  // Register binary input sensor to gw (they will be created as child devices)
  // You can use S_DOOR, S_MOTION or S_LIGHT here depending on your usage. 
  // If S_LIGHT is used, remember to update variable type you send in. See "msg" above.
  sendSketchInfo("Alarme congelateur", "1.1");
  present(TEMP_ID, S_TEMP);
  present(ALERT_ID, S_LIGHT);  
}

//  Check if digital input has changed and send in new value
void loop() 
{
  ArduinoOTA.handle();
  timer.run();
}

void GetTemp()
{
    sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  if(tempC != DEVICE_DISCONNECTED_C && tempC < 80) 
  {
    Serial.print("Temperature for the device 1 (index 0) is: ");
    Serial.println(tempC);
    send(msgTemp.set(tempC,1));
    if (tempC <= TCONG) {
      // Température de congélation: LED BLEUE
      digitalWrite(LEDFROID,HIGH);
      digitalWrite(LEDALERT,LOW);
      //digitalWrite(LEDOK,LOW);
      ledcWrite(COK, 0);
      send(msgAlert.set(0));
      
    }
    else if (tempC > TALERT) {
      // Alarme température
      digitalWrite(LEDFROID,LOW);
      digitalWrite(LEDALERT,HIGH);
      ledcWrite(COK, 0);
      //digitalWrite(LEDOK,LOW);
      send(msgAlert.set(1));
      
    }
    else {
      // Température correcte LED verte
      digitalWrite(LEDFROID,LOW);
      digitalWrite(LEDALERT,LOW);
      //digitalWrite(LEDOK,HIGH);
      ledcWrite(COK, 8);
      send(msgAlert.set(0));
    }
  } 
}
