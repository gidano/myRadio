#pragma once

namespace lang {

// ---------------------------------------------------------------------
// Phase 1: initial language infrastructure (not wired into runtime yet)
// The strings will be adopted gradually in later phases.
// ---------------------------------------------------------------------

static constexpr const char* boot_starting_radio       = "Starting radio...";
static constexpr const char* boot_wifi_connecting      = "Connecting WiFi";
static constexpr const char* boot_wifi_connected       = "WiFi connected";
static constexpr const char* boot_searching_network    = "Searching network";
static constexpr const char* boot_loading_playlist     = "Loading playlist";
static constexpr const char* boot_restoring_station    = "Restoring last station";
static constexpr const char* boot_ap_mode              = "WiFi setup mode";
static constexpr const char* boot_ssid_attempt_prefix   = "SSID ";
static constexpr const char* boot_ssid_prefix           = "SSID: ";
static constexpr const char* boot_wifi_stabilizing_log  = "[WiFi] Waiting for stabilization...";

static constexpr const char* ui_paused                 = "PAUSED";
static constexpr const char* ui_stream_prefix          = "Stream: ";
static constexpr const char* ui_no_list                = "No playlist";
static constexpr const char* ui_no_list_stations       = "No playlist (stations.txt)";
static constexpr const char* ui_select_station         = "Select station:";
static constexpr const char* ui_ok_exit_hint           = "OK: press | Exit: long press";
static constexpr const char* ui_ip_prefix              = "IP: ";
static constexpr const char* ui_no_wifi_ip             = "IP: - (no WiFi)";
static constexpr const char* ui_buffering              = "Buffering...";
static constexpr const char* ui_connecting             = "Connecting...";
static constexpr const char* ui_volume_prefix          = "Volume: ";

static constexpr const char* wifi_setup_about_title    = "About WiFi setup:";
static constexpr const char* wifi_setup_title          = "WiFi setup";
static constexpr const char* wifi_setup_step1          = "1) Connect to WiFi:";
static constexpr const char* wifi_setup_step2          = "2) In browser:";
static constexpr const char* wifi_setup_step3          = "3) Enter SSID and password";
static constexpr const char* wifi_setup_save_hint      = "Press Save,";
static constexpr const char* wifi_setup_restart_hint   = "Radio restart";
static constexpr const char* wifi_setup_ap_prefix      = "Network name:";
static constexpr const char* wifi_setup_browser_hint   = "Browser address:";

static constexpr const char* portal_page_title         = "WiFi setup";
static constexpr const char* portal_heading            = "WiFi configuration";
static constexpr const char* portal_ssid_label         = "WiFi network (SSID)";
static constexpr const char* portal_password_label     = "Password";
static constexpr const char* portal_save_button        = "Save";
static constexpr const char* portal_restart_notice     = "The device will restart after saving.";
static constexpr const char* portal_missing_ssid       = "Missing SSID.";
static constexpr const char* portal_save_ok            = "Saved. Restarting...";
static constexpr const char* portal_save_failed        = "Save failed.";

static constexpr const char* web_title                 = "myRadio web interface";
static constexpr const char* web_upload_title          = "File upload";
static constexpr const char* web_upload_path_label     = "Target path";
static constexpr const char* web_upload_button         = "Upload";
static constexpr const char* web_filelist_json         = "File list (JSON)";
static constexpr const char* web_back_to_main          = "Back to main page";
static constexpr const char* web_ok                    = "OK";
static constexpr const char* web_error                 = "Error";
static constexpr const char* web_not_found             = "Not found";
static constexpr const char* web_interface_not_found   = "Web interface not found";
static constexpr const char* web_fs_open_failed        = "Failed to open file";

static constexpr const char* search_back_to_main       = "Back to main page";
static constexpr const char* search_station_name       = "Station name";
static constexpr const char* search_station_placeholder= "e.g. Rock";
static constexpr const char* search_country            = "Country";
static constexpr const char* search_country_placeholder= "e.g. Hungary";
static constexpr const char* search_tag                = "Genre (tag)";
static constexpr const char* search_tag_placeholder    = "e.g. jazz";
static constexpr const char* search_button             = "Search";
static constexpr const char* search_clear_button       = "Clear";
static constexpr const char* search_player_preview     = "Radio player (preview)";
static constexpr const char* search_results            = "Results";
static constexpr const char* search_no_results         = "No results.";
static constexpr const char* search_module_credit      = "Search module:";
static constexpr const char* search_stream_prefix      = "Stream: ";


static constexpr const char* web_upload_html_title     = "SPIFFS Upload";
static constexpr const char* web_upload_intro          = "If the web UI is not uploaded yet, upload this first:";
static constexpr const char* web_upload_file_label     = "File";
static constexpr const char* web_back_to_radio         = "Back to radio";
static constexpr const char* web_search_not_found      = "Search page not found";
static constexpr const char* web_search_open_failed    = "Failed to open search page";
static constexpr const char* api_missing_index         = "Missing index";
static constexpr const char* api_invalid_index         = "Invalid index";
static constexpr const char* api_missing_name_or_url   = "Missing name or URL";
static constexpr const char* api_station_list_full     = "Station list is full";
static constexpr const char* api_empty_name            = "Empty name";
static constexpr const char* api_empty_url             = "Empty URL";
static constexpr const char* api_save_failed_spiffs    = "Save failed (SPIFFS)";
static constexpr const char* api_saved                 = "Saved";
static constexpr const char* api_missing_from_to       = "Missing from/to";
static constexpr const char* api_invalid_from_to       = "Invalid from/to";
static constexpr const char* api_no_change             = "No change";
static constexpr const char* api_order_saved           = "Order saved";
static constexpr const char* api_missing_volume        = "Missing volume";
static constexpr const char* api_invalid_volume        = "Invalid volume";
static constexpr const char* api_not_playlist_station  = "Not a playlist station";
static constexpr const char* api_advance_playlist_failed = "Failed to advance playlist";
static constexpr const char* api_rewind_playlist_failed  = "Failed to rewind playlist";
static constexpr const char* api_no_stations           = "No stations";

} // namespace lang
