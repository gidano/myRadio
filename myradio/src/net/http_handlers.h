#pragma once

#include <Arduino.h>

extern String g_uploadPath;


void handleRoot();
void handleSearch();
void handleGetStations();
void handleSetStation();
void handleAddStation();
void handleDeleteStation();
void handleUpdateStation();
void handleMoveStation();
void handleUploadPage();
void handleUploadDone();
void handleFileUpload();
void handleFsList();

void handleGetBrightness();
void handleSetBrightness();
void handleSetVolume();
void handleTogglePause();
void handleGetStatus();
void handleTrackNext();
void handleTrackPrev();
void handleNextStation();
void handlePrevStation();
void handleReset();
void handleGetBuffer();
