
<h2>Ajánlott hardver konfiguráció</h2>

<p>
  <strong>Hardver:</strong> ESP32-S3 N16R8 44pin, ILI9488 (480x320px), 16MB flash, 8MB PSRAM, nincs érintőképernyő, 1 rotary encoder, DAC
</p>

<h3>PIN kiosztás</h3>

<pre><code>#define TFT_DC          9
#define TFT_CS          10
#define TFT_RST         -1  // EN/RST PIN-re kötve
#define BRIGHTNESS_PIN  7

#define PIN_MOSI 11
#define PIN_SCLK 12
#define PIN_MISO -1   // tartsd -1-en 3-vezetékes SPI esetén (ha nincs olvasás)

#define I2S_DOUT  5
#define I2S_BCLK  4
#define I2S_LRC   6
// #define I2S_MCLK 15  // ezt csak akkor engedélyezd, ha CS4344 vagy más MCLK jelet használó DAC-od van, PCM5102-nél nem kell

#define ENC_A     3  // EC11-nél S1
#define ENC_B     1  // EC11-nél S2
#define ENC_BTN   2  // enkóder nyomógomb (PULLUP)</code></pre>

<h3>Flash / törlés</h3>

<p>A flash teljes törlése <code>0x0</code> értékkel elvégezhető például az alábbi eszközökkel:</p>

<ul>
  <li><strong>Flash Download Tool</strong><br>
    https://dl.espressif.com/public/flash_download_tool.zip
  </li>
  <li><strong>ESPConnect</strong><br>
    https://thelastoutpostworkshop.github.io/ESPConnect/<br>
    Egyszerűbb online megoldás, külön driver nélkül.
  </li>
  <li><strong>Online ESP32 Web Flash</strong><br>
    https://www.espboards.dev/tools/program/
  </li>
</ul>

<h3>Firmware letöltés</h3>

<p>
  A bináris fájlok a <strong>Releases</strong> menüpont alatt érhetők el.
</p>

<p>
  👉 <a href="https://github.com/gidano/myRadio/releases/latest"><strong>Firmware letöltés</strong></a>
</p>
