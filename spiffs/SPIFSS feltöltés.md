## SPIFFS mappa szerkezet

Az alábbi ábra a **SPIFFS-ben található `data` mappa** szerkezetét mutatja.

### SPIFFS könyvtár struktúra

```
data
├── audio_icons/
├── conf/
├── fonts/
│   ├── test_20.vlw
│   ├── test_24.vlw
│   ├── test_28.vlw
│   ├── test_sb_20.vlw
│   ├── test_sb_24.vlw
│   └── test_sb_28.vlw
├── web/
│   ├── index.html
│   ├── search.html
│   ├── style.css
│   └── theme.css
├── stations.txt
└── wifi.txt
```

---

# Fájl feltöltés (SPIFFS)

A web UI segítségével további fájlokat is fel tudsz tölteni.

### Példa: `stations.txt` feltöltése

1. **Fájl menü → Tallózás**
2. válaszd ki a `stations.txt` fájlt
3. az útvonal mezőbe írd:

```
/stations.txt
```

4. **Feltöltés**
5. a rendszer jelzi, ha a feltöltés sikeres
6. visszaléphetsz az előző oldalra

---

### Példa: `wifi.txt` feltöltése

A `wifi.txt` is az SPIFFS **gyökérkönyvtárába kerül**.

Útvonal:

```
/wifi.txt
```

Lépések ugyanazok mint a `stations.txt` feltöltésénél.

---

# wifi.txt formátuma

A `wifi.txt` fájl két soros tömböket tartalmaz, max. 5db SSID/jelszó párost adhatsz meg!

Példa:

```
MyWifiNetwork
titkosjelszo123
SPACE
SSID
jelszó
SPACE
SSID
jelszó
SPACE
SSID
jelszó
```

Mentéskor ajánlott:

```
UTF-8 kódolás
```

Szövegszerkesztőnek pl. a **Notepad++** ajánlott, amiben egyszerűen beállíthatod a mentett fájl kódolását.

---

# stations.txt formátuma

A `stations.txt` fájlban minden sor egy rádióállomást tartalmaz.

A formátum:

```
Állomásnév[TAB]stream URL
```

Példa:

```
Retro Rádió	https://icast.connectmedia.hu/5001/live.mp3
Petőfi Rádió	http://mr-stream.mediaconnect.hu/4737/mr2.aac
Dance Wave!	https://dancewave.online/dance.mp3
Radio Paradise Main Mix (EU) 320k AAC	http://stream-uk1.radioparadise.com/aac-320
```

Fontos:

- az **állomásnév és a link között TAB karakter legyen**
- a fájlt **UTF-8 kódolással mentsd**

---

# Megjegyzés

Más fájlok feltöltésére általában **nincs szükség**.

Az állomások:

- hozzáadása
- törlése
- rendezése


kényelmesen elvégezhető a **webes felületen**.

Amit még a WiFi kapcsolatról tudni kell:<br><br>
Alaphelyzetben a rádió indulás után 120mp ideig próbál csatlakozni a megadott adatokkal.<br>
Ha ez az idő eltelik, újraindul és ismét felkínálja új WiFi adatok bevitelét,<br>
gondolva az esetleg megváltozott hálózati körülmények adaptálására.<br>

---


