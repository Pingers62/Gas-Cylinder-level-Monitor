# Gas-Cylinder-level-Monitor
This project was created out of the need to monitor the amount of gas left in the cylinders at my caravan.
I have used a Wemos D1 Mini micro controller and a 1.8" TFT display to provide a visual representation of the gas level.

The project uses 4 x 50 Kg load cells connected to a HX711 board. and works by calculating the weight of the gas within the bottle (Total weight of gas bottle - Tare weight of gas bottle = gas weight).

The configuration / calibration is done via a web page (The D1 Mini is set to AP mode so broadcasts its own SSID - this is selected from your mobile device and then you can access the configuration page by browsing to http://192.168.4.1.

Required libraries are...

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SPI.h> // for the SPI interface of the TFT screen
#include <HX711.h> // for the HX711 load cell board
#include <EEPROM.h> // used to store the configuration to EEprom
#include <ESP8266WiFi.h> // for the AP webserver
#include <ESP8266WebServer.h> // for the AP webserver

The complete project build can be found on YouTube (XXXXX)
