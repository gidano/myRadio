<h1 align="center">myRadio</h1>

<p align="center">
  <!-- 
  <img src="https://img.shields.io/github/downloads/gidano/myRadio/total?style=for-the-badge&cacheSeconds=60" alt="Total Downloads">
  -->
  <img src="https://img.shields.io/github/stars/gidano/myRadio?style=for-the-badge" alt="Stars">
  <img src="https://img.shields.io/github/repo-size/gidano/myRadio?style=for-the-badge" alt="Repo size">
</p>

<p align="center">
  ESP32S3 alapú, saját szoftverre épülő webrádió projekt<br>
  <strong style="color: #e74c3c;">!!! Fejlesztési szakaszban lévő projekt !!!</strong>
</p>

🌍 Támogatott nyelvek
<p align="center">

Magyar	HU  Angol	EN  Német	DE  Lengyel	PL

A nyelv fordításkor választható, a következő fájlban:  Lovyan_config.h
</p>

<p align="center">
  Támogatás nélküli tesztüzemre, lelkes amatőrök számára.
</p>

<p align="center">
  <img src="https://raw.githubusercontent.com/gidano/myRadio/main/images/myRadio_320x240px.jpg" alt="myRadio 320x240" width="320" height="240"><br><br>
  <img src="https://raw.githubusercontent.com/gidano/myRadio/main/images/myRadio_480x320px.jpg" alt="myRadio 480x320" width="480" height="320">
  <img src="https://github.com/gidano/myRadio/blob/main/tools/myradio_editor/myRadio_Editor.jpg" alt="myRadio Editor" width="720" height="">
  <img src="https://github.com/gidano/myRadio/blob/main/tools/myradio_music_server/myradio.music.server.jpg" alt="myRadio Music Server" width="720" height="">
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
  <li>A rádió működése PSRAM meglétére építkezik</li>
  <li>Arduino IDE ESP32S3 N16R8 esetén <a href="https://github.com/gidano/myRadio/blob/main/images/16mb.partition_table.jpg">egyedi partíciós táblát</a> használ</li>
  <li>SPIFFS-t használunk az állomáslista (stations.txt), a fontok, a webfelület és a WiFi adatok (wifi.txt) tárolására</li>
</ul>

<h3>Könyvtárak:</h3>

<ul>
  <li>LovyanGFX by lovyan03 v1.2.19</li>
  <li>ESP32-audioI2S-master by schreibfaul1 v3.4.4</li>
  <li>Adafruit ST7735 and ST7789 Library by Adafruit v1.11.0</li>
  <li>A GitHub buxtronix/Arduino kódtárból a Rotary könyvtárt be kell helyezni a ..\Dokumentumok\Arduino\libraries mappába: <a href="https://github.com/buxtronix/arduino/tree/master/libraries/Rotary">Rotary könyvtár</a></li>
</ul>

<h2>Üzembe helyezés:</h2>

<ul>
  <li>Készíts egy wifi.txt nevű állományt</li>
  <li>Írd be a használni kívánt SSID/jelszó páros(oka)t (max. 5) és töltsd fel a <a href="https://github.com/gidano/myRadio/blob/main/binary%20files/SPIFSS%20upload.md">leírás szerint</a></li>
</ul>

<h2>Funkciók:</h2>

<ul>
  <li>Enkóder hangerő, rövid nyomás: lejátszás/szünet, hosszan nyomva: egy soros állomásválasztó menü. A menüben egy kattintással aktivál - OK, hosszan nyomva kilép aktiválás nélkül.</li>
  <li>AAC, MP3, OPUS, FLAC, OGG/VORBIS fájlok lejátszása 1.5M-ig</li>
  <li>WiFi térerő alul jobbra, és egy kis wifi rádió logó a jobb felső sarokban</li>
  <li>Aktuálisan játszott stream kodek elhelyezve bal felső sarokba</li>
  <li>A bitrate adatok áthelyezve középre a Stream sorba a többi audio adat közé (..CH | ..KHz | ..bit | ..kbps)</li>
  <li>Hangerő megjelenítés ikonnal és számértékkel</li>
  <li>Állomásválasztás előre feltöltött listából (max. 120 db)</li>
  <li>Képesség állomás hozzáadására, törlésére, sorrendezésére a webes felületen</li>
  <li>Az utoljára hallgatott állomással indul</li>
  <li>Az állomáslista kereshető</li>
  <li>Első induláskor feldob egy kis ablakot a WiFi-csatlakozás leírásával (SSID: WebRadio-Setup, IP: http://192.168.4.1), majd az adatokat elmenti</li>
  <li>Audio puffer kijelzés telítettség-állapot színezéssel (piros-sárga 40%-zöld 75%)</li>
  <li>Webfelületen Reboot gomb az ESP32-höz</li>
  <li>Képesség PC adott mappa zene tartalmának lejátszására playlist.m3u alapon, a PC-n egy Python script indítja a streamet</li>
  <li>PC-zene lejátszás alatt ID3 tag-ból olvas címet és előadót</li>
  <li>Webes felületen SPIFFS fájlfeltöltés indítható, állomás- és PC-stream-ben előre-hátra léptetés, új állomás felvétele, szerkesztése, SPIFFS-re másolása, törlése, fényerősség szabályozás csúszkával</li>
  <li>DAC 5102A / CJMCU CS4344 DAC is alkalmazható, utóbbihoz szükséges MCLK pin rendelkezésre áll</li>
  <li>Mégis csak került bele egy minimalista VU, szerintem illik is a felületbe, így nem csupán egy statikus felület látható lejátszás közben</li>
  <li>Beépítésre került a yoRadio-hoz készült "Mirosław B. • Radio-Browser API" az állomások kereséséhez és mentéséhez</li>
  <li>Elkészült hozzá egy állomás lista kezelő szoftver, amely beolvassa/konvertálja a yoRadio-féle playlist.csv fájlt Yoradio-ról és PC-ről is.
  Bővebb info a kezdőlap "tools" mappában</li>
  <li>myRadio Music Server - Egy könnyen használható Windows alkalmazás, amely a számítógépedet zenei streaming szerverré alakítja ESP32 alapú (myRadio, yoRadio) webrádiók számára.</li>
  <li>..folyt. köv.</li>
</ul>

..és, hogy legyen mivel androidról irányítani: <a href="https://github.com/gidano/YoRadio-Controller">YoRadio-Controller</a>
