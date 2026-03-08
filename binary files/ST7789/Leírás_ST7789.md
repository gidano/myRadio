## Ajánlott hardver konfiguráció

**Hardver:** ESP32-S3 Supermini, ST7789 (320x240px), 4MB flash, 2MB PSRAM, nincs érintőképernyő, 1 rotary encoder, DAC

### PIN kiosztás

```cpp
#define TFT_DC          9
#define TFT_CS          10
#define TFT_RST         8  // -1 érték legyen, ha EN/RST PIN-re kötöd más ESP32S3 modulon
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
#define ENC_BTN   2  // enkóder nyomógomb (PULLUP)
```

### Flash törlés

A flash teljes törlése `0x0` értékkel elvégezhető például az alábbi eszközökkel:

- **Flash Download Tool**  
  https://dl.espressif.com/public/flash_download_tool.zip

- **ESPConnect (online)**  
  https://thelastoutpostworkshop.github.io/ESPConnect/  
  Egyszerűbb online megoldás, külön driver nélkül.

- **Online ESP32 Web Flash**  
  https://www.espboards.dev/tools/program/

### Firmware letöltés

👉 **[Firmware letöltés](https://github.com/gidano/myRadio/releases/latest)**

