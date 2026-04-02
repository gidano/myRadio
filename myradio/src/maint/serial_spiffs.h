#pragma once

#include <Arduino.h>

void serial_spiffs_begin(Stream& primarySerial, Stream* secondarySerial = nullptr);
void serial_spiffs_poll();
bool serial_spiffs_is_active();
