#pragma once
#include <Arduino.h>
#include <WebServer.h>

struct NetServerCtx {
  WebServer* server;

  // delayed reboot
  volatile bool*     restartRequested;
  volatile uint32_t* restartAtMs;

  // periodic wifi UI refresh (callbacks)
  uint32_t* lastWifiDraw;
  uint32_t  wifiDrawMs;
  void (*updateWifiIconOnly)();
  void (*updateBufferIndicatorOnly)();

  // reconnect handler
  void (*handleWiFiReconnect)();
};

void net_server_poll(NetServerCtx& ctx);
