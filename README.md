<h1 align="center">myRadio</h1>

<p align="center">
  ESP32-S3 alapú webrádió projekt saját szoftverrel<br>
  <strong style="color:#e74c3c;">Fejlesztés alatt – nem refaktorált, jelenlegi állapotában használható</strong>
</p>

<p align="center">
  <img src="https://github.com/gidano/myRadio/raw/main/images/myRadio_felulet_480x320px.jpg" alt="myRadio felület" width="65%"><br>
  <small>aktuális megjelenés (480×320-as kijelzőn)</small>
</p>

<hr>

<h2>Hardver</h2>

<ul>
  <li>ESP32-S3 Supermini N4R2 vagy ESP32-S3 N16R8</li>
  <li>Kijelzők:
    <ul>
      <li>ST7789 / ILI9341 (320×240)</li>
      <li>ST7796 / ILI9488 (480×320)</li>
    </ul>
  </li>
  <li>DAC: PCM5102A vagy CJMCU CS4344</li>
  <li>Rotary encoder: EC11 vagy KY-040</li>
</ul>

<h2>Szoftver & Beállítások</h2>

<ul>
  <li>Arduino IDE 2.3.x + ESP32 core 3.0.x / 3.x</li>
  <li><strong>PSRAM</strong> megléte kötelező a magasabb bitrátájú stream-ekhez</li>
  <li>SPIFFS fájlrendszer:
    <ul>
      <li><code>stations.txt</code> – állomáslista</li>
      <li><code>.vlw</code> fontok</li>
      <li>webfelület fájljai</li>
      <li><code>wifi.txt</code> – WiFi adatok</li>
    </ul>
  </li>
</ul>

<h3>Használt könyvtárak</h3>

<ul>
  <li>LovyanGFX by lovyan03 <code>v1.2.19</code></li>
  <li>ESP32-audioI2S by schreibfaul1 <code>v3.4.4</code> (master)</li>
  <li>Adafruit ST7735 and ST7789 Library by Adafruit <code>v1.11.0</code></li>
  <li>Rotary by buxtronix → <a href="https://github.com/buxtronix/arduino/tree/master/libraries/Rotary">GitHub</a></li>
</ul>

<hr>

<h2>Főbb funkciók</h2>

<ul>
  <li>Támogatott formátumok: AAC · MP3 · OPUS · FLAC · OGG/Vorbis (kb. 1.5 Mbit/s-ig)</li>
  <li>WiFi térerő jelző + kis rádió logó</li>
  <li>Aktuális kodek / bitrate / mintavételezés / csatorna info</li>
  <li>Hangerő ikon + numerikus érték</li>
  <li>Előre feltöltött állomáslista (max ~120 db)</li>
  <li>Webes felületen: állomás keresés, hozzáadás, törlés, sorrendezés</li>
  <li>Utoljára hallgatott állomás menti és azzal indul</li>
  <li>Első indításkor csatlakozási segéd</li>
  <li>Audio puffer állapot jelző (színes)</li>
  <li>Webes Reboot gomb</li>
  <li>PC-s zenelejátszás <code>playlist.m3u</code> + Python script segítségével</li>
  <li>ID3 tag cím/előadó olvasás</li>
  <li>SPIFFS fájlkezelés a webes felületről (feltöltés, másolás, törlés)</li>
  <li>Mini VU-mérő kijelzés (minimalista)</li>
  <li>Radio-Browser API keresés & mentés (yoRadio implementáció alapján)</li>
</ul>

<hr>

<p align="center">
  <small>Jelenleg csak lelkes amatőröknek / tesztüzemre ajánlott.<br>
  Bináris fájlokat igény esetén tudok küldeni a megfelelő hardverre.</small>
</p>

<p align="center">…folytatjuk 🛠️</p>
