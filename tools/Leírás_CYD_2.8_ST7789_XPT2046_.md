## Ajánlott hardver konfiguráció

**Hardver:** CYD 2.8" ESP32 ST7789XPT2046 (320x240px), 4MB flash, 4MB hozzáadott PSRAM, érintőképernyő, 5102A DAC

### PIN kiosztás

```cpp
#define TFT_DC          2
#define TFT_CS          15
#define TFT_RST         -1  // -1 érték legyen, ha EN/RST PIN-re kötöd más ESP32S3 modulon
#define BRIGHTNESS_PIN  21

#define PIN_MOSI 11
#define PIN_SCLK 12
#define PIN_MISO -1   // tartsd -1-en 3-vezetékes SPI esetén (ha nincs olvasás)

/*  TOUCHSCREEN  */
#define TS_MODEL        TS_MODEL_XPT2046
#define TS_SPIPINS      25, 39, 32    /* SCK, MISO, MOSI */
#define TS_CS			33

/* I2S DAC 5102 */
#define I2S_DOUT  27
#define I2S_BCLK  4
#define I2S_LRC   22
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



