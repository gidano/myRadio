#pragma once

namespace lang {

// ---------------------------------------------------------------------
// 1. fázis: induló nyelvi alapréteg (még nincs bekötve a működő kódba)
// A szövegek későbbi fázisokban fokozatosan kerülnek majd használatba.
// ---------------------------------------------------------------------

static constexpr const char* boot_starting_radio       = "Indul a rádió...";
static constexpr const char* boot_wifi_connecting      = "WiFi csatlakozás";
static constexpr const char* boot_wifi_connected       = "WiFi csatlakozva";
static constexpr const char* boot_searching_network    = "Hálózat keresése";
static constexpr const char* boot_loading_playlist     = "Lejátszási lista betöltése";
static constexpr const char* boot_restoring_station    = "Utolsó állomás visszaállítása";
static constexpr const char* boot_ap_mode              = "WiFi beállítás mód";
static constexpr const char* boot_ssid_attempt_prefix   = "SSID ";
static constexpr const char* boot_ssid_prefix           = "SSID: ";
static constexpr const char* boot_wifi_stabilizing_log  = "[WiFi] Stabilizáció várakozás...";

static constexpr const char* ui_paused                 = "SZÜNET";
static constexpr const char* ui_stream_prefix          = "Stream: ";
static constexpr const char* ui_no_list                = "Nincs lista";
static constexpr const char* ui_no_list_stations       = "Nincs lista (stations.txt)";
static constexpr const char* ui_select_station         = "Válassz állomást:";
static constexpr const char* ui_ok_exit_hint           = "OK: nyom | Kilép: hosszan nyom";
static constexpr const char* ui_ip_prefix              = "IP: ";
static constexpr const char* ui_no_wifi_ip             = "IP: - (nincs WiFi)";
static constexpr const char* ui_buffering              = "Pufferelés...";
static constexpr const char* ui_connecting             = "Csatlakozás...";
static constexpr const char* ui_volume_prefix          = "Hangerő: ";

static constexpr const char* wifi_setup_about_title    = "A WiFi beállításáról:";
static constexpr const char* wifi_setup_title          = "WiFi beállítás";
static constexpr const char* wifi_setup_step1          = "1) Csatlakozz a WiFi hálózathoz:";
static constexpr const char* wifi_setup_step2          = "2) Böngészőben nyisd meg:";
static constexpr const char* wifi_setup_step3          = "3) Add meg az SSID és jelszó párost";
static constexpr const char* wifi_setup_save_hint      = "Majd nyomj a Mentés gombra";
static constexpr const char* wifi_setup_restart_hint   = "A rádió újraindul";
static constexpr const char* wifi_setup_ap_prefix      = "Hálózat neve:";
static constexpr const char* wifi_setup_browser_hint   = "Böngésző cím:";

static constexpr const char* portal_page_title         = "WiFi beállítás";
static constexpr const char* portal_heading            = "WiFi konfiguráció";
static constexpr const char* portal_ssid_label         = "WiFi hálózat (SSID)";
static constexpr const char* portal_password_label     = "Jelszó";
static constexpr const char* portal_save_button        = "Mentés";
static constexpr const char* portal_restart_notice     = "Mentés után az eszköz újraindul.";
static constexpr const char* portal_missing_ssid       = "Hiányzó SSID.";
static constexpr const char* portal_save_ok            = "Mentve. Újraindítás...";
static constexpr const char* portal_save_failed        = "Nem sikerült menteni.";

static constexpr const char* web_title                 = "myRadio kezelőfelület";
static constexpr const char* web_upload_title          = "Fájl feltöltés";
static constexpr const char* web_upload_path_label     = "Cél útvonal";
static constexpr const char* web_upload_button         = "Feltöltés";
static constexpr const char* web_filelist_json         = "Fájllista (JSON)";
static constexpr const char* web_back_to_main          = "Vissza a főoldalra";
static constexpr const char* web_ok                    = "OK";
static constexpr const char* web_error                 = "Hiba";
static constexpr const char* web_not_found             = "Nem található";
static constexpr const char* web_interface_not_found   = "Web felület nem található";
static constexpr const char* web_fs_open_failed        = "Fájl megnyitása sikertelen";

static constexpr const char* search_back_to_main       = "Vissza a főoldalra";
static constexpr const char* search_station_name       = "Állomás név";
static constexpr const char* search_station_placeholder= "pl. Rock";
static constexpr const char* search_country            = "Ország";
static constexpr const char* search_country_placeholder= "pl. Hungary";
static constexpr const char* search_tag                = "Műfaj (tag)";
static constexpr const char* search_tag_placeholder    = "pl. jazz";
static constexpr const char* search_button             = "Keresés";
static constexpr const char* search_clear_button       = "Törlés";
static constexpr const char* search_player_preview     = "Rádió lejátszó (előnézet)";
static constexpr const char* search_results            = "Találatok";
static constexpr const char* search_no_results         = "Nincs találat.";
static constexpr const char* search_module_credit      = "Kereső modul:";
static constexpr const char* search_stream_prefix      = "Stream: ";


static constexpr const char* web_upload_html_title     = "SPIFFS feltöltés";
static constexpr const char* web_upload_intro          = "Ha még nincs web UI feltöltve, először töltsd fel ide:";
static constexpr const char* web_upload_file_label     = "Fájl";
static constexpr const char* web_back_to_radio         = "Vissza a rádióhoz";
static constexpr const char* web_search_not_found      = "Keresőoldal nem található";
static constexpr const char* web_search_open_failed    = "A keresőoldal megnyitása sikertelen";
static constexpr const char* api_missing_index         = "Hiányzó index";
static constexpr const char* api_invalid_index         = "Érvénytelen index";
static constexpr const char* api_missing_name_or_url   = "Hiányzó név vagy URL";
static constexpr const char* api_station_list_full     = "Az állomáslista megtelt";
static constexpr const char* api_empty_name            = "Üres név";
static constexpr const char* api_empty_url             = "Üres URL";
static constexpr const char* api_save_failed_spiffs    = "Mentés sikertelen (SPIFFS)";
static constexpr const char* api_saved                 = "Mentve";
static constexpr const char* api_missing_from_to       = "Hiányzó from/to";
static constexpr const char* api_invalid_from_to       = "Érvénytelen from/to";
static constexpr const char* api_no_change             = "Nincs változás";
static constexpr const char* api_order_saved           = "Sorrend mentve";
static constexpr const char* api_missing_volume        = "Hiányzó hangerő";
static constexpr const char* api_invalid_volume        = "Érvénytelen hangerő";
static constexpr const char* api_not_playlist_station  = "Ez nem lejátszási listás állomás";
static constexpr const char* api_advance_playlist_failed = "Nem sikerült a lejátszási listát előreléptetni";
static constexpr const char* api_rewind_playlist_failed  = "Nem sikerült a lejátszási listát visszaléptetni";
static constexpr const char* api_no_stations           = "Nincs állomás";

} // namespace lang
