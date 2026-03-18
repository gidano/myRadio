📻 myRadio – ESP32 Internet Radio Firmware

myRadio is a multilingual internet radio firmware configurable via a web interface;
for ESP32-S3-based systems with a TFT display.

The goal of the project is to create a stable, easy-to-use, and customizable internet radio firmware.

✨ Features

🌐 Internet radio streaming

🖥️ Web-based control interface

🔎 Built-in radio station search

🌍 Multi-language firmware and web UI

📡 WiFi captive setup mode

🗂️ SPIFFS-based configuration

🔊 Stream buffer monitoring

💡 Display brightness control

🔄 ESP32 restart from web interface

🎛️ Rotary encoder support

📱 Responsive web interface

🌍 Supported Languages

The firmware currently supports:

Language	Code
Hungarian	HU
English	EN
German	DE
Polish	PL

Language selection is done at compile time in:

Lovyan_config.h
🖥️ Supported Displays

The firmware currently supports four TFT display types.

Controller	Typical resolution
ILI9488	480×320
ILI9341	320×240
ST7796	480×320
ST7789	320×240

Display configuration is handled in:

Lovyan_config.h
🚀 First Boot

If the device does not find wifi.txt in SPIFFS, it automatically starts WiFi setup mode.

The ESP32 creates a network called:

myRadio-Setup

Steps:

Connect to the WiFi network

Open a browser and navigate to

http://192.168.4.1

Enter your WiFi credentials

The device will restart automatically

🌐 Web Interface

The built-in web UI allows you to:

manage radio stations

edit station names and stream URLs

reorder the playlist

search internet radio stations

control display brightness

monitor stream buffer status

restart the ESP32

📁 SPIFFS File Structure
/wifi.txt
/stations.txt
/web/*
/fonts/*

Example:

/web/index_en.html
/web/search_en.html
/fonts/test_24.vlw
⚙️ Lovyan_config.h Configuration

The main firmware configuration file:

Lovyan_config.h

Important settings available here include:

Firmware version
#define MYRADIO_VERSION "0.2"
Language selection
#define MYRADIO_LANG_HU 1
#define MYRADIO_LANG_EN 2
#define MYRADIO_LANG_DE 3
#define MYRADIO_LANG_PL 4

#define MYRADIO_LANG MYRADIO_LANG_EN
Display configuration

The display driver and hardware-related configuration are defined in Lovyan_config.h.

Typical parameters include:

display type

SPI pins

display resolution

rotation

Encoder configuration

Rotary encoder pins can also be configured in this file.

🔤 Fonts

The firmware uses VLW fonts generated for LovyanGFX.

The included fonts support:

Hungarian characters

German umlauts

Polish characters

extended punctuation

🛠️ Build

The project is intended to be compiled in the Arduino IDE for ESP32-S3 based hardware.

📦 Project Structure
src/
  ui/
  net/
  lang/

data/
  web/
  fonts/
👤 Author

myRadio v0.2

by gidano

⭐ Support the Project

If you like the project, consider giving it a ⭐ on GitHub.

<p align="left">
.. so that you can control it from an Android device: <a href="https://github.com/gidano/YoRadio-Controller">**YoRadio Controller**</a><br>
.. so you can easily edit the station list on your PC (stations.txt): <a href="https://github.com/gidano/myRadio-Editor">**myRadio Editor**</a><br>
.. so you can listen to music streamed from your PC on the radio: <a href="https://github.com/gidano/myRadio-Music-Server">**myRadio Music Server**</a>
</p>
