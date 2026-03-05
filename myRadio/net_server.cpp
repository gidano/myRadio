#include "net_server.h"
#include <Arduino.h>

void net_server_poll(NetServerCtx& ctx) {
  if (ctx.server) {
    ctx.server->handleClient();
  }

  // Execute delayed reboot (requested by /api/reset)
  if (ctx.restartRequested && ctx.restartAtMs) {
    if (*(ctx.restartRequested) && (int32_t)(millis() - *(ctx.restartAtMs)) >= 0) {
      ESP.restart();
    }
  }

  // Periodic wifi UI refresh
  if (ctx.lastWifiDraw && ctx.wifiDrawMs > 0) {
    uint32_t now = millis();
    if (now - *(ctx.lastWifiDraw) >= ctx.wifiDrawMs) {
      *(ctx.lastWifiDraw) = now;
      if (ctx.updateWifiIconOnly) ctx.updateWifiIconOnly();
      if (ctx.updateBufferIndicatorOnly) ctx.updateBufferIndicatorOnly();
    }
  }

  // WiFi reconnect logic
  if (ctx.handleWiFiReconnect) {
    ctx.handleWiFiReconnect();
  }
}
