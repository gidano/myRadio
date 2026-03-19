## SPIFFS image feltöltése

A projekt mellé előre elkészített `SPIFFS_xxx` fájlok is tartoznak, így nem szükséges a `./data/` mappa tartalmát kézzel, egyesével feltölteni.

### Mire valók ezek?

A `SPIFFS_xxx` fájl a rádió webes felületéhez és működéséhez szükséges fájlokat tartalmazza előre csomagolva, például:

- webes kezelőfelület
- betűkészletek
- egyéb, a `data` mappában található állományok

### Elérhető változatok

Két külön SPIFFS image készült:

- **4 MB flash mérethez**
- **16 MB flash mérethez**

Mindig azt a változatot kell használni, amelyik a kiválasztott partíciós sémához és az adott ESP32 modul flash méretéhez illeszkedik.

### Feltöltés

A `SPIFFS_xxx` fájl feltölthető például:

- [esptool](https://github.com/espressif/esptool)
- [ESP32 Flash Download Tool](https://www.espressif.com/en/support/download/other-tools)

A feltöltésnél ügyelni kell arra, hogy a fájl a partíciós táblának megfelelő **SPIFFS partíció címére** kerüljön.

### Fontos

- A **4 MB-os** SPIFFS image csak a hozzá tartozó **4 MB-os partíciós kiosztással** használható.
- A **16 MB-os** SPIFFS image csak a hozzá tartozó **16 MB-os partíciós kiosztással** használható.
- Nem megfelelő partícióméret vagy címzés esetén a webes fájlok nem lesznek elérhetők, vagy a rendszer hibásan működhet.

### Ha nem SPIFFS_xxx fájlt használsz

Természetesen továbbra is lehetőség van a `./data/` mappa tartalmát kézzel feltölteni, de az előre elkészített `SPIFFS_xxx` használata gyorsabb és egyszerűbb megoldás.

### Javaslat

Ha most telepíted a firmware-t, érdemes a megfelelő `SPIFFS_xxx` fájlt is rögtön feltölteni, így a rendszer azonnal használható lesz.

-------------------------------------------------------

## Uploading the SPIFFS image

The project includes pre-built `SPIFFS_xxx` files, so you don’t need to manually upload the contents of the `./data/` folder one by one.

### What are these for?

The `SPIFFS_xxx` file contains pre-packaged files necessary for the radio’s web interface and operation, such as:

- web interface
- fonts
- other files found in the `data` folder

### Available versions

Two separate SPIFFS images have been created:

- **for 4 MB flash size**
- **for 16 MB flash size**

You must always use the version that matches the selected partition scheme and the flash size of the given ESP32 module.

### Uploading

The `SPIFFS_xxx` file can be uploaded using, for example:

- [esptool](https://github.com/espressif/esptool)
- [ESP32 Flash Download Tool](https://www.espressif.com/en/support/download/other-tools)

When uploading, make sure the file is placed at the **SPIFFS partition address** corresponding to the partition table.

### Important

- The **4 MB** SPIFFS image can only be used with the corresponding **4 MB partition layout**.
- The **16 MB** SPIFFS image can only be used with the corresponding **16 MB partition layout**.
- If the partition size or address is incorrect, web files will not be accessible, or the system may malfunction.

### If you are not using an SPIFFS_xxx file

Of course, you can still manually upload the contents of the `./data/` folder, but using the pre-prepared `SPIFFS_xxx` file is a faster and simpler solution.

### Recommendation

If you are installing the firmware now, it is a good idea to upload the appropriate `SPIFFS_xxx` file right away, so the system will be ready to use immediately.