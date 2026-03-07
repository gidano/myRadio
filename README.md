<h1 align="center">myRadio</h1>

<p align="center">
  ESP32S3 alapú, saját szoftverre épülő webrádió projekt<br>
  <strong style="color: #e74c3c;">!!! Fejlesztési szakaszban lévő, nem refaktorált projekt !!!</strong>
</p>

<p align="center">
  Egyelőre adott hardver konfigurációval használható, Arduino Ide által flashelhető, bináris fájlokat tudok nyújtani St7789 és ILI9488 kijelzőkre.<br>
  Támogatás nélküli tesztüzemre, lelkes amatőrök számára.
</p>

<p align="center">
  <img src="https://github.com/gidano/myRadio/raw/main/images/myRadio_felulet_480x320px.jpg" alt="myRadio" style="max-width:30%;">
</p>

<h2>A használt hardver elemek:</h2>

<ul>
  <li>ESP32-S3 Supermini N4R2</li>
  <li>ESP32-S3 N16R8</li>
  <li>ST7789, ILI9341 320x240</li>
  <li>ST7796, ILI9488 480x320</li>
  <li>DAC 5102A, vagy CJMCU CS4344</li>
  <li>EC11 vagy KY-040 rotary encoder</li>
</ul>

<h2>Szoftver:</h2>

<ul>
  <li>Arduino IDE 2.3.8 - CORE 3.3.7 - (magas bitrátájú adatfolyamok lejátszása csak a megfelelő liblwip módosítások elvégezése után!)</li>
  <li>Arduino IDE paraméterek az alap yoRadio szerint (szükség van PSRAM meglétére)</li>
  <li>SPIFFS-t használunk az állomás lista (stations.txt), a .vlw típusú fontok, a webfelület és a WiFi adatok (wifi.txt) tárolására</li>
</ul>

<h3>Könyvtárak:</h3>

<ul>
  <li>LovyanGFX by lovyan03 v1.2.19</li>
  <li>ESP32-audioI2S-master by schreibfaul1 v3.4.4</li>
  <li>Adafruit ST7735 and ST7789 Library by Adafruit v1.11.0</li>
  <li>A GitHub buxtronix/Arduino kódtárból a Rotary könyvtárt be kell helyezni a ..\Dokumentumok\Arduino\libraries mappába[](https://github.com/buxtronix/arduino/tree/master/libraries/Rotary)</li>
</ul>

<h2>Funkciók:</h2>

<ul>
  <li>AAC, MP3, OPUS, FLAC, OGG/VORBIS FÁJLOK LEJÁTSZÁSA 1.5m-IG</li>
  <li>WiFi térerő alul jobbra, és egy kis wifi rádió logó a jobb felső sarokban</li>
  <li>aktuálisan játszott stream kodek elhelyezve bal felső sarokba</li>
  <li>a bitrate adatok áthelyezve középre a Stream sorba a többi audio adat közé (..CH | ..KHz | ..bit | ..kbps)</li>
  <li>hangerő megjelenítés ikonnal és szám értékkel</li>
  <li>állomás választás előre feltöltött listából (max. 120db)</li>
  <li>képesség új állomás hozzáadására/törlésére/sorrendezésére a webes felületen</li>
  <li>az utoljára hallgatott állomással indul, az állomáslista kereshető</li>
  <li>első induláskor feldob egy kis ablakot a csatlakozási leírással, majd az adatokat elmenti</li>
  <li>audio puffer kijelzés állapot-színezéssel</li>
  <li>web felületen Reboot gomb az ESP32-höz</li>
  <li>képesség PC adott zene mappa tartalmának lejátszására playlist.m3u alapon, a PC-n egy Python script indítja a stream-et</li>
  <li>ID3Tag-ból olvas címet + előadót</li>
  <li>webes felületen SPIFFS fájl feltöltés indítható, állomás és PC-stream-ben előre-hátra léptetés, új állomás felvétele, szerkesztése, SPIFFS-re másolása, törlése</li>
  <li>DAC 5102A / CJMCU CS4344 DAC is alkalmazható, utóbbihoz szükséges MCLK pin rendelkezésre áll</li>
  <li>mégis csak került bele egy minimalista VU, szerintem illik is a felületbe, így nem csupán egy statikus felület látható lejátszás közben...</li>
  <li>beépítésre került a yoRadio-hoz készült "Mirosław B. • Radio-Browser API" állomások kereséséhez/mentéséhez</li>
  <li>..folyt.köv.</li>
</ul>
