#pragma once
#include <Arduino.h>
#include <IPAddress.h>

using WifiManagerPortalHelpFn = void (*)(const char* apSsid, const IPAddress& ip);
using WifiManagerRestoredFn   = void (*)();
using WifiManagerAttemptFn    = void (*)(const char* ssid, int index, int total);

void wifi_manager_init(WifiManagerPortalHelpFn portalHelpFn,
                       WifiManagerRestoredFn restoredFn,
                       WifiManagerAttemptFn attemptFn = nullptr);

bool wifi_manager_begin_or_portal();
void wifi_manager_handle_reconnect();
uint32_t* wifi_manager_connected_at_ptr();
const char* wifi_manager_current_attempt_ssid();
const char* wifi_manager_active_ssid();
int wifi_manager_active_rssi_dbm();
