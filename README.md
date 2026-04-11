# 📻 myRadio Firmware

ESP32 alapú internet rádió projekt TFT és OLED kijelző támogatással, webes kezelőfelülettel és bővíthető funkciókkal.
ESP32-based internet radio project with support for TFT and OLED displays, a web-based interface, and expandable features.

---

## Képernyőkép/Screenshots

<img src="https://github.com/gidano/myRadio/blob/main/images/myRadio_320x240px.jpg" alt="320x240" width="320"><img src="https://github.com/gidano/myRadio/blob/main/images/myRadio_480x320px.jpg" alt="480x320" width="480">

---

## 🇭🇺 Funkciók

* Enkóder hangerőszabályzás

  * rövid nyomás: play/pause
  * hosszú nyomás: állomásválasztó menü

* AAC, MP3, OPUS, FLAC, OGG/VORBIS lejátszás (~2Mbit)

* WiFi jelerősség kijelzés (jobb alsó sarok)

* Stream információk:

  * codec (bal felső sarok)
  * audio adatok (CH | KHz | bit | kbps)

* Hangerő kijelzés ikon + érték

* Állomáslista:

  * max. 300 állomás
  * kereshető
  * weben szerkeszthető

* Utoljára hallgatott állomással indul

* Web UI:

  * állomás hozzáadás / törlés / rendezés
  * SPIFFS feltöltés
  * fényerő állítás
  * reboot

* PC streaming támogatás (playlist.m3u + Python server)

* ID3 tag olvasás (PC stream)

* VU meter

* Radio-Browser API integráció

---

## 🖼️ ÚJ: Állomás logó megjelenítés (TFT)

A firmware támogatja az állomáslogók megjelenítését **TFT kijelzőn**.

⚠️ OLED ág NEM használ logókat.

### Jellemzők

* PNG logók SPIFFS-ről
* automatikus betöltés állomásváltáskor
* cache-elés (nem tölti újra feleslegesen)
* fallback: `nologo.png`
* megjelenítés: **felső középső rész**

---

## 📁 SPIFFS struktúra

```
/stations.txt
/wifi.txt
/web/...
/fonts/...
/logos/
    danubius.png
    retro.png
    nologo.png
```

---

## 📄 stations.txt formátum

```
Station Name<TAB>URL<TAB>Logo
```

### Példa:

```
DANUBIUS RÁDIÓ	https://danubiusradio.hu/live_HiFi.mp3	danubius
```

### Szabályok:

* logónév **kiterjesztés nélkül**
* mindig **kisbetűs**
* fájl: `/logos/<név>.png`
* ha nincs logó:

```
nologo
```

---

## 🖼️ Logó működés

A firmware:

1. kiolvassa a 3. mezőt (`logoName`)
2. összeállítja az útvonalat:

```
/logos/<logoName>.png
```

3. ha nem létezik:

```
/logos/nologo.png
```

4. kirajzolja TFT-re

---

## 📺 Kijelző viselkedés

### TFT (ILI9488 / ST7789 stb.)

* logó megjelenik ✔
* felső középen ✔
* állomásváltáskor frissül ✔

### OLED

* változatlan működés ✔
* logó NINCS ✔

---

## ⚙️ Build / Konfiguráció

* Arduino IDE
* ESP32
* LovyanGFX
* ESP32-audioI2S

---

## 🔧 Megjegyzések

* a logó **nem kerül újratöltésre minden frame-nél**
* csak állomásváltáskor
* UI redraw nem romlik
* meglévő funkciók érintetlenek maradnak

---

## 🇬🇧 Features

* Encoder volume control

  * short press: play/pause
  * long press: station menu

* AAC, MP3, OPUS, FLAC, OGG/VORBIS playback (~2Mbit)

* WiFi signal indicator

* Stream info display (codec, bitrate, etc.)

* Volume indicator

* Station list (max 300, searchable, editable via web)

* Starts with last station

* Web UI:

  * station management
  * SPIFFS upload
  * brightness control
  * reboot

* PC streaming support

* ID3 tag reading

* VU meter

* Radio-Browser API

---

## 🖼️ NEW: Station logo support (TFT)

Logos are supported on **TFT displays only**.

OLED is untouched.

### Features:

* PNG logos from SPIFFS
* automatic update on station change
* caching (no unnecessary reload)
* fallback: `nologo.png`
* position: **top center**

---

## 📄 stations.txt format

```
Station Name<TAB>URL<TAB>Logo
```

Example:

```
DANUBIUS RADIO	https://...	danubius
```

### Rules:

* lowercase names
* no extension
* stored in `/logos`
* fallback: `nologo`

---

## 📺 Display behavior

### TFT

* logo shown ✔
* centered top ✔
* updates on station change ✔

### OLED

* unchanged ✔
* no logo ✔

---

## 📌 Notes

* logo loads only on station change
* UI performance unaffected
* no auto-detection — explicit mapping only
