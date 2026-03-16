#include "http_api.h"

#include <Arduino.h>
#include <WiFi.h>

void http_api_register_routes(WebServer& server, const HttpApiHandlers& h) {
  server.on("/", h.handleRoot);
  // Radio search page (served from SPIFFS)
  server.on("/search", HTTP_GET, h.handleSearch);
  server.on("/search.html", HTTP_GET, h.handleSearch);
  server.on("/api/stations", HTTP_GET, h.handleGetStations);
  server.on("/api/stations/add", HTTP_POST, h.handleAddStation);
  server.on("/api/stations/delete", HTTP_POST, h.handleDeleteStation);
  server.on("/api/stations/update", HTTP_POST, h.handleUpdateStation);
  server.on("/api/stations/move", HTTP_POST, h.handleMoveStation);
  // Accept common reorder endpoints from different web UIs (drag-and-drop)
  server.on("/api/stations/reorder", HTTP_POST, h.handleMoveStation);
  server.on("/api/stations/order", HTTP_POST, h.handleMoveStation);
  server.on("/api/stations/sort", HTTP_POST, h.handleMoveStation);
  server.on("/api/stations/moveStation", HTTP_POST, h.handleMoveStation);

  server.on("/api/station", HTTP_POST, h.handleSetStation);
  server.on("/api/volume", HTTP_POST, h.handleSetVolume);
  server.on("/api/brightness", HTTP_GET, h.handleGetBrightness);
  server.on("/api/brightness", HTTP_POST, h.handleSetBrightness);
  server.on("/api/toggle", HTTP_POST, h.handleTogglePause);
  server.on("/api/status", HTTP_GET, h.handleGetStatus);
  server.on("/api/next", HTTP_POST, h.handleNextStation);
  server.on("/api/track_next", HTTP_POST, h.handleTrackNext);
  server.on("/api/track_prev", HTTP_POST, h.handleTrackPrev);
  server.on("/api/prev", HTTP_POST, h.handlePrevStation);
  server.on("/api/buffer", HTTP_GET, h.handleGetBuffer);
  server.on("/api/reset", HTTP_POST, h.handleReset);

  server.on("/upload", HTTP_GET, h.handleUploadPage);
  server.on("/upload", HTTP_POST, h.handleUploadDone, h.handleFileUpload);
  server.on("/api/fs/list", HTTP_GET, h.handleFsList);

  server.begin();
  Serial.println("Web server started");
  Serial.print("Connect to http://");
  Serial.println(WiFi.localIP());
}
