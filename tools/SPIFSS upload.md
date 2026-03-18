## SPIFFS folder structure
The diagram below shows the structure of the **`data` folder stored in SPIFFS**.

### SPIFFS directory structure
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

# File upload (SPIFFS)
You can upload additional files using the web UI.

### Example: uploading `stations.txt`
1. **File menu → Browse**
2. select the `stations.txt` file
3. enter the following path in the path field:
```
/stations.txt
```
4. **Upload**
5. the system will confirm when the upload is successful
6. you can navigate back to the previous page

---

### Example: uploading `wifi.txt`
The `wifi.txt` file is also placed in the **root directory** of SPIFFS.

Path:
```
/wifi.txt
```
The steps are the same as for uploading `stations.txt`.

---

# wifi.txt format
The `wifi.txt` file contains two-line blocks — you can specify up to 5 SSID/password pairs.

Example:
```
MyWifiNetwork
secretpassword123
SPACE
SSID
password
SPACE
SSID
password
SPACE
SSID
password
```

Recommended encoding when saving:
```
UTF-8
```
A text editor such as **Notepad++** is recommended, as it lets you easily set the encoding of the saved file.

---

# stations.txt format
Each line in the `stations.txt` file represents one radio station.

Format:
```
Station name[TAB]stream URL
```

Example:
```
Retro Rádió	https://icast.connectmedia.hu/5001/live.mp3
Petőfi Rádió	http://mr-stream.mediaconnect.hu/4737/mr2.aac
Dance Wave!	https://dancewave.online/dance.mp3
Radio Paradise Main Mix (EU) 320k AAC	http://stream-uk1.radioparadise.com/aac-320
```

Important:
- there must be a **TAB character between the station name and the URL**
- save the file with **UTF-8 encoding**

---

# Notes
Uploading other files is generally **not necessary**.

Adding, deleting and reordering stations can be done conveniently through the **web interface**.

A few things to know about the WiFi connection:<br><br>
By default, after startup the radio will attempt to connect using the provided credentials for 120 seconds.<br>
If this time elapses without a successful connection, the device restarts and offers the option to enter new WiFi credentials again,<br>
allowing it to adapt to potentially changed network conditions.<br>
