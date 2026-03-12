# myRadio Music Server

Egy könnyen használható Windows alkalmazás, amely a számítógépedet **zenei streaming szerverré** alakítja **ESP32 alapú myRadio webrádiók** számára.

Az alkalmazás automatikusan **lejátszási listákat generál** a zenei mappádból, majd HTTP szerveren keresztül elérhetővé teszi őket, így a rádió közvetlenül a PC-ről tud zenét streamelni.

**Python telepítés nem szükséges**, az alkalmazás egyetlen EXE fájlként fut.

---

## Képernyőkép

![myRadio Music Server](screenshot.png)

---

# Főbb funkciók

- Egyszerű **Windows grafikus felület**
- **Egyetlen EXE fájl** – Python nélkül fut
- **Magyar / Angol nyelv**
- Automatikus **playlist generálás**
- Albumonkénti lejátszási listák
- Automatikus **IP felismerés**
- Beépített **HTTP zenei szerver**
- Kompatibilis az **ESP32 myRadio firmware-rel**
- Windows **Dark / Light theme felismerés**
- Playlist URL-ek másolása egy kattintással
- Valós idejű szerver napló

---

# Generált lejátszási listák

A szerver indulásakor a program az alábbi fájlokat hozza létre a zenei mappában:


playlist.m3u
playlist_shuffle.m3u
album_playlists/*.m3u


Példa könyvtárszerkezet:


Music/
├── playlist.m3u
├── playlist_shuffle.m3u
├── album_playlists/
│ ├── Pink_Floyd.m3u
│ ├── Metallica.m3u
│ └── Daft_Punk.m3u


Ha a zenei könyvtár változik, a playlist-ek automatikusan frissülnek.

---

# Működés

Az alkalmazás egy kis HTTP szervert indít a számítógépen:


http://IP_CÍM:PORT/


Példa playlist URL:


http://192.168.31.101:8000/playlist.m3u


Ezt az URL-t kell megadni a **myRadio ESP32 webrádióban**, amely így közvetlenül a számítógépről játssza le a zenét.

---

# Használat

1. Indítsd el a **myRadio Music Server** alkalmazást
2. Válaszd ki a **zenei mappát**
3. Kattints az **Indítás** gombra
4. Másold ki a **playlist URL-t**
5. Add meg ezt a címet a **myRadio rádióban**

Ennyi.

A rádió ezután közvetlenül a számítógépedről fog zenét streamelni.

---

# Letöltés

A legfrissebb verzió innen tölthető le:

⬇ **[Letöltés – myRadio Music Server](https://github.com/gidano/myRadio/releases/download/1.0/myRadio_Music_Server.zip)**

---

# Technikai részletek

HTTP szerver:


http.server.SimpleHTTPRequestHandler
ThreadingTCPServer


Playlist encoding:


UTF-8 BOM


A fájl URL-ek automatikusan kódolásra kerülnek, így a speciális karaktereket tartalmazó fájlnevek is működnek.

---

# Fordítás forrásból

Szükséges:


Python 3.x
PyInstaller


Fordítás PowerShell alatt:


py -m PyInstaller --onefile --windowed --name "myRadio Music Server" --icon=music_server.ico --clean --noconfirm --add-data 'music_server.ico;.' --add-data 'music_server.png;.' --version-file version_info.txt pc_music_server_gui_v1_1_1.py


Az elkészült program itt található:


dist/myRadio Music Server.exe


---

# Szerző

gidano