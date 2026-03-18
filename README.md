<p align="center">
  <img src="https://github.com/gidano/myRadio/blob/main/images/myradiologo_240.png" alt="myRadio logo 320x240" width="320" height="240">
</p>

<p align="center">
  <!-- 
 <img src="https://img.shields.io/github/downloads/gidano/myRadio/total?style=for-the-badge&cacheSeconds=60" alt="Total Downloads"> -->
  <img src="https://img.shields.io/github/stars/gidano/myRadio?style=for-the-badge" alt="Stars">
  <img src="https://img.shields.io/github/repo-size/gidano/myRadio?style=for-the-badge" alt="Repo size">
</p>

<p align="left">
  ESP32S3 alapú, saját szoftverre épülő webrádió projekt<br>
  ESP32S3-based web radio project built on custom software<br>
  <strong style="color: #e74c3c;">!!! Fejlesztési szakaszban lévő projekt !!!</strong><br>
  <strong style="color: #e74c3c;">!!! Project under development !!!</strong>
</p>

---

🌍 Támogatott nyelvek / Supported languages

<p align="left">
**Magyar (HU) Angol (EN) Német (DE) Lengyel (PL)**
**Hungarian (HU) English (EN) German (DE) Polish (PL)**
</p>

<p align="left">
A nyelv fordításkor választható, a <b>Lovyan_config.h</b> fájlban.<br>
The language can be selected at compile time in <b>Lovyan_config.h</b>.
</p>

<p align="left">
  Támogatás nélküli tesztüzemre.<br>
  For test use without support.
</p>

<p align="center">
  <img src="https://raw.githubusercontent.com/gidano/myRadio/main/images/myRadio_320x240px.jpg" alt="myRadio 320x240" width="320" height="240"><br>
  <img src="https://raw.githubusercontent.com/gidano/myRadio/main/images/myRadio_480x320px.jpg" alt="myRadio 480x320" width="480" height="320"><br>
</p>

---

## A használt hardver elemek / Hardware components

- ESP32-S3 Supermini N4R2
- ESP32-S3 N16R8
- ST7789, ILI9341 320x240
- ST7796, ILI9488 480x320
- DAC 5102A, vagy / or CJMCU CS4344
- EC11 vagy / or KY-040 rotary encoder

---

## Szoftver / Software

- Arduino IDE 2.3.8 - CORE 3.3.7 - (magas bitrátájú adatfolyamok lejátszása csak a megfelelő [**liblwip módosítások elvégezése**](https://github.com/gidano/myRadio/Audio_IDF_Mod/Playing_high-bitrate_stations.pdf) után! / high bitrate stream playback only after applying the necessary [**liblwip modifications**](https://github.com/gidano/myRadio/Audio_IDF_Mod/Playing_high-bitrate_stations.pdf)!
- A rádió működése PSRAM meglétére építkezik / The radio relies on the presence of PSRAM
- Arduino IDE ESP32S3 N16R8 esetén / for ESP32S3 N16R8 uses [egyedi partíciós táblát / custom partition table](https://github.com/gidano/myRadio/blob/main/images/16mb.partition_table.jpg)
- SPIFFS-t használunk az állomáslista (stations.txt), a fontok, a webfelület és a WiFi adatok (wifi.txt) tárolására / SPIFFS is used to store the station list (stations.txt), fonts, web interface and WiFi credentials (wifi.txt)

### Könyvtárak / Libraries

- LovyanGFX by lovyan03 v1.2.19
- ESP32-audioI2S-master by schreibfaul1 v3.4.5
- Adafruit ST7735 and ST7789 Library by Adafruit v1.11.0
- A GitHub buxtronix/Arduino kódtárból a Rotary könyvtárt be kell helyezni a `..\Dokumentumok\Arduino\libraries` mappába / The Rotary library from the GitHub buxtronix/Arduino repository must be placed in the `..\Documents\Arduino\libraries` folder: [Rotary könyvtár / Rotary library](https://github.com/buxtronix/arduino/tree/master/libraries/Rotary)

---

## Üzembe helyezés / Setup

- Készíts egy `wifi.txt` nevű állományt / Create a file named `wifi.txt`
- Írd be a használni kívánt SSID/jelszó páros(oka)t (max. 5) és töltsd fel a [leírás szerint / according to the instructions](https://github.com/gidano/myRadio/blob/main/binary%20files/SPIFSS%20upload.md)

---

## Funkciók / Features

- Enkóder hangerő, rövid nyomás: lejátszás/szünet, hosszan nyomva: egy soros állomásválasztó menü. A menüben egy kattintással aktivál - OK, hosszan nyomva kilép aktiválás nélkül. / Encoder for volume, short press: play/pause, long press: single-row station selector menu. In the menu, one click activates (OK), long press exits without activating.
- AAC, MP3, OPUS, FLAC, OGG/VORBIS fájlok lejátszása 1.5M-ig / Playback of AAC, MP3, OPUS, FLAC, OGG/VORBIS files up to 1.5M bitrate
- WiFi térerő alul jobbra, és egy kis wifi rádió logó a jobb felső sarokban / WiFi signal strength bottom-right, and a small wifi radio logo in the top-right corner
- Aktuálisan játszott stream kodek elhelyezve bal felső sarokba / Currently playing stream codec displayed in the top-left corner
- A bitrate adatok áthelyezve középre a Stream sorba a többi audio adat közé (..CH | ..KHz | ..bit | ..kbps) / Bitrate data moved to the center Stream row alongside other audio info (..CH | ..KHz | ..bit | ..kbps)
- Hangerő megjelenítés ikonnal és számértékkel / Volume display with icon and numeric value
- Állomásválasztás előre feltöltött listából (max. 120 db) / Station selection from a preloaded list (max. 120 entries)
- Képesség állomás hozzáadására, törlésére, sorrendezésére a webes felületen / Ability to add, delete and reorder stations via the web interface
- Az utoljára hallgatott állomással indul / Starts with the last listened station
- Az állomáslista kereshető / Station list is searchable
- Első induláskor feldob egy kis ablakot a WiFi-csatlakozás leírásával (SSID: WebRadio-Setup, IP: http://192.168.4.1), majd az adatokat elmenti / On first start, displays a small window with WiFi setup instructions (SSID: WebRadio-Setup, IP: http://192.168.4.1), then saves the credentials
- Audio puffer kijelzés telítettség-állapot színezéssel (piros-sárga 40%-zöld 75%) / Audio buffer display with fill-level color coding (red–yellow 40%–green 75%)
- Webfelületen Reboot gomb az ESP32-höz / Reboot button for the ESP32 on the web interface
- Képesség PC adott mappa zene tartalmának lejátszására playlist.m3u alapon, a PC-n egy Python script indítja a streamet / Ability to play music from a PC folder via playlist.m3u, with a Python script on the PC initiating the stream
- PC-zene lejátszás alatt ID3 tag-ból olvas címet és előadót / During PC music playback, reads title and artist from ID3 tags
- Webes felületen SPIFFS fájlfeltöltés indítható, állomás- és PC-stream-ben előre-hátra léptetés, új állomás felvétele, szerkesztése, SPIFFS-re másolása, törlése, fényerősség szabályozás csúszkával / Web interface supports SPIFFS file upload, forward/backward track stepping in station and PC stream, adding/editing/copying/deleting stations, and brightness control via slider
- DAC 5102A / CJMCU CS4344 DAC is alkalmazható, utóbbihoz szükséges MCLK pin rendelkezésre áll / Both DAC 5102A and CJMCU CS4344 DAC are supported; the MCLK pin required by the CS4344 is available
- Mégis csak került bele egy minimalista VU, szerintem illik is a felületbe, így nem csupán egy statikus felület látható lejátszás közben / A minimalist VU meter was added after all — it fits the interface nicely, so the display is no longer static during playback
- Beépítésre került a yoRadio-hoz készült "Mirosław B. • Radio-Browser API" az állomások kereséséhez és mentéséhez / The "Mirosław B. • Radio-Browser API" originally made for yoRadio has been integrated for station search and saving
- Elkészült hozzá egy állomás lista kezelő szoftver, amely beolvassa/konvertálja a yoRadio-féle playlist.csv fájlt Yoradio-ról és PC-ről is. Bővebb info a kezdőlap "tools" mappában / A station list manager application has been created that reads/converts the yoRadio-style playlist.csv file from both yoRadio and PC. More info in the "tools" folder of the repository
- myRadio Music Server - Egy könnyen használható Windows alkalmazás, amely a számítógépedet zenei streaming szerverré alakítja ESP32 alapú (myRadio, yoRadio) webrádiók számára. / myRadio Music Server — An easy-to-use Windows application that turns your computer into a music streaming server for ESP32-based (myRadio, yoRadio) web radios.
- ..folyt. köv. / ..to be continued

---

<p align="left">
.. hogy legyen mivel androidról irányítani / .. to control it from Android:
<a href="https://github.com/gidano/YoRadio-Controller"><b>YoRadio Controller</b></a><br>
.. hogy egyszerűen, PC-n szerkeszd az állomás listát (stations.txt) / .. to easily edit the station list (stations.txt) on PC:
<a href="https://github.com/gidano/myRadio-Editor"><b>myRadio Editor</b></a><br>
.. hogy PC-ről streamelt zenét is tudj hallgatni a rádión / .. to listen to music streamed from your PC on the radio:
<a href="https://github.com/gidano/myRadio-Music-Server"><b>myRadio Music Server</b></a>
</p>
