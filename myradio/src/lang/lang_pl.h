#pragma once

namespace lang {

static constexpr const char* boot_starting_radio       = "Uruchamianie radia...";
static constexpr const char* boot_wifi_connecting      = "Łączenie z WiFi";
static constexpr const char* boot_wifi_connected       = "Połączono z WiFi";
static constexpr const char* boot_searching_network    = "Wyszukiwanie sieci";
static constexpr const char* boot_loading_playlist     = "Wczytywanie listy";
static constexpr const char* boot_restoring_station    = "Przywracanie ostatniej stacji";
static constexpr const char* boot_ap_mode              = "Tryb konfiguracji WiFi";
static constexpr const char* boot_ssid_attempt_prefix  = "SSID ";
static constexpr const char* boot_ssid_prefix          = "SSID: ";
static constexpr const char* boot_wifi_stabilizing_log = "[WiFi] Oczekiwanie na stabilizację...";

static constexpr const char* ui_paused                 = "PAUZA";
static constexpr const char* ui_stream_prefix          = "Strumień: ";
static constexpr const char* ui_no_list                = "Brak listy";
static constexpr const char* ui_no_list_stations       = "Brak listy (stations.txt)";
static constexpr const char* ui_select_station         = "Wybierz stację:";
static constexpr const char* ui_ok_exit_hint           = "OK: naciśnij | Wyjście: przytrzymaj";
static constexpr const char* ui_ip_prefix              = "IP: ";
static constexpr const char* ui_no_wifi_ip             = "IP: - (brak WiFi)";
static constexpr const char* ui_buffering              = "Buforowanie...";
static constexpr const char* ui_connecting             = "Łączenie...";
static constexpr const char* ui_volume_prefix          = "Głośność: ";

static constexpr const char* wifi_setup_about_title    = "O konfiguracji WiFi:";
static constexpr const char* wifi_setup_title          = "Konfiguracja WiFi";
static constexpr const char* wifi_setup_step1          = "1) WiFi połącz:";
static constexpr const char* wifi_setup_step2          = "2) W przeglądarce:";
static constexpr const char* wifi_setup_step3          = "3) Wpisz SSID i hasło";
static constexpr const char* wifi_setup_save_hint      = "Naciśnij Zapisz,";
static constexpr const char* wifi_setup_restart_hint   = "Radio uruchomi...";
static constexpr const char* wifi_setup_ap_prefix      = "Nazwa sieci:";
static constexpr const char* wifi_setup_browser_hint   = "Adres w przeglądarce:";

static constexpr const char* portal_page_title         = "Konfiguracja WiFi";
static constexpr const char* portal_heading            = "Konfiguracja sieci WiFi";
static constexpr const char* portal_ssid_label         = "Sieć WiFi (SSID)";
static constexpr const char* portal_password_label     = "Hasło";
static constexpr const char* portal_save_button        = "Zapisz";
static constexpr const char* portal_restart_notice     = "Urządzenie uruchomi się ponownie po zapisaniu.";
static constexpr const char* portal_missing_ssid       = "Brak SSID.";
static constexpr const char* portal_save_ok            = "Zapisano. Ponowne uruchamianie...";
static constexpr const char* portal_save_failed        = "Nie udało się zapisać.";

static constexpr const char* web_title                 = "Panel myRadio";
static constexpr const char* web_upload_title          = "Przesyłanie pliku";
static constexpr const char* web_upload_path_label     = "Ścieżka docelowa";
static constexpr const char* web_upload_button         = "Prześlij";
static constexpr const char* web_filelist_json         = "Lista plików (JSON)";
static constexpr const char* web_back_to_main          = "Powrót do strony głównej";
static constexpr const char* web_ok                    = "OK";
static constexpr const char* web_error                 = "Błąd";
static constexpr const char* web_not_found             = "Nie znaleziono";
static constexpr const char* web_interface_not_found   = "Nie znaleziono interfejsu WWW";
static constexpr const char* web_fs_open_failed        = "Nie udało się otworzyć pliku";

static constexpr const char* search_back_to_main       = "Powrót do strony głównej";
static constexpr const char* search_station_name       = "Nazwa stacji";
static constexpr const char* search_station_placeholder= "np. Rock";
static constexpr const char* search_country            = "Kraj";
static constexpr const char* search_country_placeholder= "np. Polska";
static constexpr const char* search_tag                = "Gatunek (tag)";
static constexpr const char* search_tag_placeholder    = "np. jazz";
static constexpr const char* search_button             = "Szukaj";
static constexpr const char* search_clear_button       = "Wyczyść";
static constexpr const char* search_player_preview     = "Odtwarzacz radia (podgląd)";
static constexpr const char* search_results            = "Wyniki";
static constexpr const char* search_no_results         = "Brak wyników.";
static constexpr const char* search_module_credit      = "Moduł wyszukiwania:";
static constexpr const char* search_stream_prefix      = "Strumień: ";

static constexpr const char* web_upload_html_title     = "Wysyłanie SPIFFS";
static constexpr const char* web_upload_intro          = "Jeśli interfejs WWW nie został jeszcze wgrany, najpierw wyślij ten plik:";
static constexpr const char* web_upload_file_label     = "Plik";
static constexpr const char* web_back_to_radio         = "Powrót do radia";
static constexpr const char* web_search_not_found      = "Nie znaleziono strony wyszukiwania";
static constexpr const char* web_search_open_failed    = "Nie udało się otworzyć strony wyszukiwania";
static constexpr const char* api_missing_index         = "Brak indeksu";
static constexpr const char* api_invalid_index         = "Nieprawidłowy indeks";
static constexpr const char* api_missing_name_or_url   = "Brak nazwy lub URL";
static constexpr const char* api_station_list_full     = "Lista stacji jest pełna";
static constexpr const char* api_empty_name            = "Pusta nazwa";
static constexpr const char* api_empty_url             = "Pusty URL";
static constexpr const char* api_save_failed_spiffs    = "Nie udało się zapisać (SPIFFS)";
static constexpr const char* api_saved                 = "Zapisano";
static constexpr const char* api_missing_from_to       = "Brak from/to";
static constexpr const char* api_invalid_from_to       = "Nieprawidłowe from/to";
static constexpr const char* api_no_change             = "Brak zmian";
static constexpr const char* api_order_saved           = "Kolejność zapisana";
static constexpr const char* api_missing_volume        = "Brak głośności";
static constexpr const char* api_invalid_volume        = "Nieprawidłowa głośność";
static constexpr const char* api_not_playlist_station  = "To nie jest stacja z listy odtwarzania";
static constexpr const char* api_advance_playlist_failed = "Nie udało się przełączyć listy do przodu";
static constexpr const char* api_rewind_playlist_failed  = "Nie udało się cofnąć listy odtwarzania";
static constexpr const char* api_no_stations           = "Brak stacji";

} // namespace lang
