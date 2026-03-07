# myRadio
ESP32S3 alapú, saját szoftverre épülő webrádió projekt<br> 
!!! Fejlesztési szakaszban lévő, nem refaktorált projekt !!!
Egyelőre adott hardver konfigurációval használható, Arduino Ide által flashelhető, bináris fájlokat tudok adni.
Támogatás nélküli tesztüzemre, lelkes amatőrök számára.

<img src="https://github.com/gidano/myRadio/blob/main/images/myRadio_felulet_480x320px.jpg" alt="myRadio" width="max-width:50%;">

A használt hardver elemek:

ESP32-S3 Supermini N4R2
ESP32-S3 N16R8

ST7789, ILI9341 320x240
ST7796, ILI9488 480x320

DAC 5102A, vagy CJMCU CS4344
EC11 vagy KY-040 rotary encoder

Szoftver:

Arduino IDE 2.3.8 - CORE 3.3.7 - (magas bitrátájú adatfolyamok lejátszása csak a megfelelő liblwip módosítások elvégezése után!)
Arduino IDE paraméterek az alap yoRadio szerint (szükség van PSRAM meglétére)
SPIFFS-t használunk az állomás lista (stations.txt), a .vlw típusú fontok, a webfelület és a WiFi adatok (wifi.txt) tárolására

Könyvtárak:

LovyanGFX by lovyan03 v1.2.19
ESP32-audioI2S-master by schreibfaul1 v3.4.4
Adafruit ST7735 and ST7789 Library by Adafruit v1.11.0
A GitHub buxtronix/Arduino kódtárból a Rotary könyvtárt be kell helyezni a ..\Dokumentumok\Arduino\libraries mappába (https://github.com/buxtronix/arduino/tree/master/libraries/Rotary)

Funkciók:

AAC, MP3, OPUS, FLAC, OGG/VORBIS FÁJLOK LEJÁTSZÁSA 1.5m-IG
WiFi térerő alul jobbra, és egy kis wifi rádió logó a jobb felső sarokban
aktuálisan játszott stream kodek elhelyezve bal felső sarokba
a bitrate adatok áthelyezve középre a Stream sorba a többi audio adat közé (..CH | ..KHz | ..bit | ..kbps)
hangerő megjelenítés ikonnal és szám értékkel 
állomás választás előre feltöltött listából (max. 120db)
képesség új állomás hozzáadására/törlésére/sorrendezésére a webes felületen
az utoljára hallgatott állomással indul, az állomáslista kereshető
első induláskor feldob egy kis ablakot a csatlakozási leírással, majd az adatokat elmenti
audio puffer kijelzés állapot-színezéssel
web felületen Reboot gomb az ESP32-höz
képesség PC adott zene mappa tartalmának lejátszására playlist.m3u alapon, a PC-n egy Python script indítja a stream-et
ID3Tag-ból olvas címet + előadót
webes felületen SPIFFS fájl feltöltés indítható, állomás és PC-stream-ben előre-hátra léptetés, új állomás felvétele, szerkesztése, SPIFFS-re másolása, törlése 
DAC 5102A / CJMCU CS4344 DAC is alkalmazható, utóbbihoz szükséges MCLK pin rendelkezésre áll
mégis csak került bele egy minimalista VU, szerintem illik is a felületbe, így nem csupán egy statikus felület látható lejátszás közben...
beépítésre került a yoRadio-hoz készült "Mirosław B. • Radio-Browser API" állomások kereséséhez/mentéséhez
..folyt.köv.
