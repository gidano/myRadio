#pragma once

namespace lang {

static constexpr const char* boot_starting_radio       = "Radio wird gestartet...";
static constexpr const char* boot_wifi_connecting      = "WLAN-Verbindung";
static constexpr const char* boot_wifi_connected       = "WLAN verbunden";
static constexpr const char* boot_searching_network    = "Netzwerk wird gesucht";
static constexpr const char* boot_loading_playlist     = "Playlist wird geladen";
static constexpr const char* boot_restoring_station    = "Letzter Sender wird wiederhergestellt";
static constexpr const char* boot_ap_mode              = "WLAN-Einrichtungsmodus";
static constexpr const char* boot_ssid_attempt_prefix  = "SSID ";
static constexpr const char* boot_ssid_prefix          = "SSID: ";
static constexpr const char* boot_wifi_stabilizing_log = "[WiFi] Warte auf Stabilisierung...";

static constexpr const char* ui_paused                 = "PAUSE";
static constexpr const char* ui_stream_prefix          = "Stream: ";
static constexpr const char* ui_no_list                = "Keine Liste";
static constexpr const char* ui_no_list_stations       = "Keine Liste (stations.txt)";
static constexpr const char* ui_select_station         = "Sender wählen:";
static constexpr const char* ui_ok_exit_hint           = "OK: drücken | Beenden: lang drücken";
static constexpr const char* ui_ip_prefix              = "IP: ";
static constexpr const char* ui_no_wifi_ip             = "IP: - (kein WLAN)";
static constexpr const char* ui_buffering              = "Puffern...";
static constexpr const char* ui_connecting             = "Verbinden...";
static constexpr const char* ui_volume_prefix          = "Lautstärke: ";

static constexpr const char* wifi_setup_about_title    = "Zur WLAN-Einrichtung:";
static constexpr const char* wifi_setup_title          = "WLAN-Einrichtung";
static constexpr const char* wifi_setup_step1          = "1) Verbinde dich WLAN:";
static constexpr const char* wifi_setup_step2          = "2) Im Browser:";
static constexpr const char* wifi_setup_step3          = "3) SSID und Passwort eingeben";
static constexpr const char* wifi_setup_save_hint      = "Speichern drücken,";
static constexpr const char* wifi_setup_restart_hint   = "Das Radio restartet";
static constexpr const char* wifi_setup_ap_prefix      = "Netzwerkname:";
static constexpr const char* wifi_setup_browser_hint   = "Browser-Adresse:";

static constexpr const char* portal_page_title         = "WLAN-Einrichtung";
static constexpr const char* portal_heading            = "WLAN-Konfiguration";
static constexpr const char* portal_ssid_label         = "WLAN-Netzwerk (SSID)";
static constexpr const char* portal_password_label     = "Passwort";
static constexpr const char* portal_save_button        = "Speichern";
static constexpr const char* portal_restart_notice     = "Das Gerät startet nach dem Speichern neu.";
static constexpr const char* portal_missing_ssid       = "SSID fehlt.";
static constexpr const char* portal_save_ok            = "Gespeichert. Neustart...";
static constexpr const char* portal_save_failed        = "Speichern fehlgeschlagen.";

static constexpr const char* web_title                 = "myRadio Weboberfläche";
static constexpr const char* web_upload_title          = "Datei hochladen";
static constexpr const char* web_upload_path_label     = "Zielpfad";
static constexpr const char* web_upload_button         = "Hochladen";
static constexpr const char* web_filelist_json         = "Dateiliste (JSON)";
static constexpr const char* web_back_to_main          = "Zurück zur Hauptseite";
static constexpr const char* web_ok                    = "OK";
static constexpr const char* web_error                 = "Fehler";
static constexpr const char* web_not_found             = "Nicht gefunden";
static constexpr const char* web_interface_not_found   = "Weboberfläche nicht gefunden";
static constexpr const char* web_fs_open_failed        = "Datei konnte nicht geöffnet werden";

static constexpr const char* search_back_to_main       = "Zurück zur Hauptseite";
static constexpr const char* search_station_name       = "Sendername";
static constexpr const char* search_station_placeholder= "z. B. Rock";
static constexpr const char* search_country            = "Land";
static constexpr const char* search_country_placeholder= "z. B. Deutschland";
static constexpr const char* search_tag                = "Genre (Tag)";
static constexpr const char* search_tag_placeholder    = "z. B. Jazz";
static constexpr const char* search_button             = "Suchen";
static constexpr const char* search_clear_button       = "Löschen";
static constexpr const char* search_player_preview     = "Radioplayer (Vorschau)";
static constexpr const char* search_results            = "Ergebnisse";
static constexpr const char* search_no_results         = "Keine Ergebnisse.";
static constexpr const char* search_module_credit      = "Suchmodul:";
static constexpr const char* search_stream_prefix      = "Stream: ";

static constexpr const char* web_upload_html_title     = "SPIFFS Upload";
static constexpr const char* web_upload_intro          = "Wenn das Web-UI noch nicht hochgeladen wurde, zuerst diese Datei hochladen:";
static constexpr const char* web_upload_file_label     = "Datei";
static constexpr const char* web_back_to_radio         = "Zurück zum Radio";
static constexpr const char* web_search_not_found      = "Suchseite nicht gefunden";
static constexpr const char* web_search_open_failed    = "Suchseite konnte nicht geöffnet werden";
static constexpr const char* api_missing_index         = "Fehlender Index";
static constexpr const char* api_invalid_index         = "Ungültiger Index";
static constexpr const char* api_missing_name_or_url   = "Name oder URL fehlt";
static constexpr const char* api_station_list_full     = "Die Senderliste ist voll";
static constexpr const char* api_empty_name            = "Leerer Name";
static constexpr const char* api_empty_url             = "Leere URL";
static constexpr const char* api_save_failed_spiffs    = "Speichern fehlgeschlagen (SPIFFS)";
static constexpr const char* api_saved                 = "Gespeichert";
static constexpr const char* api_missing_from_to       = "Fehlendes from/to";
static constexpr const char* api_invalid_from_to       = "Ungültiges from/to";
static constexpr const char* api_no_change             = "Keine Änderung";
static constexpr const char* api_order_saved           = "Reihenfolge gespeichert";
static constexpr const char* api_missing_volume        = "Fehlende Lautstärke";
static constexpr const char* api_invalid_volume        = "Ungültige Lautstärke";
static constexpr const char* api_not_playlist_station  = "Kein Playlist-Sender";
static constexpr const char* api_advance_playlist_failed = "Playlist konnte nicht weitergeschaltet werden";
static constexpr const char* api_rewind_playlist_failed  = "Playlist konnte nicht zurückgeschaltet werden";
static constexpr const char* api_no_stations           = "Keine Sender";

} // namespace lang
