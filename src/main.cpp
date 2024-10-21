#include <Arduino.h>
#include <WiFi.h>
#include <Esp.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>
#include <NetBIOS.h>
#include "web.cpp"

#define host "fanmanager"

WiFiServer server(80);

uint64_t chipId = 0;
Web *web;

void onSpeedCommand(uint16_t speed, HAFan *sender)
{
  Serial.printf("MAIN onSpeedCommand: %d\n", speed);
  web->SpeedCommand(speed, sender);
}

void onStateCommand(bool state, HAFan *sender)
{
  Serial.printf("MAIN onStateCommand: %d\n", state);
  web->StateCommand(state, sender);
}

void setup()
{

  Serial.println("Starting fan manager");
  web = new Web();
  web->initGPIO();
  web->wifiInit();

  chipId = ESP.getEfuseMac();

  Serial.printf("ESP32 Chip model = %s Rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
  Serial.printf("This chip has %d cores\n", ESP.getChipCores());
  Serial.print("Chip ID: "); // 9778272
  Serial.println(chipId);
  Serial.print("Версия SDK: ");
  Serial.println(ESP.getSdkVersion());
  Serial.printf("Flash chip size:\t%d (bytes)\n", ESP.getFlashChipSize());
  Serial.printf("Free heap size:\t%d (bytes)\n", ESP.getFreeHeap());
  Serial.printf("Flash chip frequency:\t%d (Hz)\n", ESP.getFlashChipSpeed());
  Serial.printf("CPU chip frequency:\t%d (MHz)\n", ESP.getCpuFreqMHz());

  NBNS.begin(host);
  MDNS.begin(host);
  Serial.print("Open http://");
  Serial.print(host);
  Serial.println(".local to see the browser");
  web->onSpeedCommand(onSpeedCommand);
  web->onStateCommand(onStateCommand);
  web->begin();
  Serial.println("Normal web server operation mode");
}

void loop()
{
  BUTTONState st = web->getButton();
  switch (st)
  {
  case TOGGLETOFREE:
    web->wifiInitSTA();
    break;
  case TOGGLETOPRESSED:
    web->wifiInitAP();
    break;
  }
  web->run();
}
