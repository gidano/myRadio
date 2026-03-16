#pragma once

#include <WebServer.h>

struct HttpApiHandlers {
  void (*handleRoot)();
  void (*handleSearch)();
  void (*handleGetStations)();
  void (*handleAddStation)();
  void (*handleDeleteStation)();
  void (*handleUpdateStation)();
  void (*handleMoveStation)();
  void (*handleSetStation)();
  void (*handleSetVolume)();
  void (*handleGetBrightness)();
  void (*handleSetBrightness)();
  void (*handleTogglePause)();
  void (*handleGetStatus)();
  void (*handleNextStation)();
  void (*handleTrackNext)();
  void (*handleTrackPrev)();
  void (*handlePrevStation)();
  void (*handleGetBuffer)();
  void (*handleReset)();
  void (*handleUploadPage)();
  void (*handleUploadDone)();
  void (*handleFileUpload)();
  void (*handleFsList)();
};

void http_api_register_routes(WebServer& server, const HttpApiHandlers& handlers);
