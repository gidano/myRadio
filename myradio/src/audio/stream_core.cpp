#include "stream_core.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stdarg.h>

#ifndef AUDIO_WATCHDOG
#define AUDIO_WATCHDOG 1
#endif

static StreamCoreConfig g_cfg;
static QueueHandle_t g_audioQ = nullptr;
static TaskHandle_t g_audioTaskHandle = nullptr;

enum AudioCmdType : uint8_t { ACMD_SET_VOL, ACMD_CONNECT_URL, ACMD_STOP };

struct AudioCmd {
  AudioCmdType type;
  uint8_t vol;
  char url[256];
};

static inline void logf(const char* fmt, ...) {
  if (!g_cfg.logf) return;
  va_list ap;
  va_start(ap, fmt);

  char buf[512];
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  g_cfg.logf("%s", buf);
}

static inline void logln(const char* s) {
  if (g_cfg.logln) g_cfg.logln(s);
}

static void streamAudioTask(void* param) {
  (void)param;
  if (!g_cfg.audio) {
    vTaskDelete(nullptr);
    return;
  }

  AudioCmd cmd;

  uint32_t lastFilledNonZeroAt = millis();
  uint32_t lastUnderrunPrintAt = 0;
  uint32_t lastRecoveryAt      = 0;
  uint32_t emptySince          = 0;
  uint32_t underrunCount       = 0;
  const uint32_t connectGraceMs = 2500;

  for (;;) {
    while (g_audioQ && xQueueReceive(g_audioQ, &cmd, 0) == pdTRUE) {
      if (cmd.type == ACMD_SET_VOL) {
        g_cfg.audio->setVolume(cmd.vol);
      } else if (cmd.type == ACMD_STOP) {
        g_cfg.audio->stopSong();
      } else if (cmd.type == ACMD_CONNECT_URL) {
        emptySince = 0;
        lastFilledNonZeroAt = millis();
        lastUnderrunPrintAt = millis();
        logf("[AUDIO] Connecting to: %s\n", cmd.url);

        if (strncmp(cmd.url, "https://", 8) == 0) logln("[AUDIO] HTTPS URL detected");
        else                                       logln("[AUDIO] HTTP URL detected");

        g_cfg.audio->connecttohost(cmd.url);
      }
    }

    g_cfg.audio->loop();

    if (g_cfg.connectRequestedAtMs && *g_cfg.connectRequestedAtMs > 0 && !g_cfg.audio->isRunning()) {
      uint32_t elapsed = millis() - *g_cfg.connectRequestedAtMs;
      if (elapsed > 3000 && elapsed < 10000) {
        static uint32_t lastPrint = 0;
        if (millis() - lastPrint > 2000) {
          lastPrint = millis();
          logf("[AUDIO] Még mindig kapcsolódik... (%lu mp)\n", elapsed / 1000);
        }
      }
    }

#if AUDIO_WATCHDOG
    if (g_cfg.audio->isRunning()) {
      const size_t filled = g_cfg.audio->inBufferFilled();

      if (filled > 0) {
        lastFilledNonZeroAt = millis();
        emptySince = 0;
      } else {
        if (emptySince == 0) emptySince = millis();

        const uint32_t now = millis();
        const bool inConnectGrace =
          (g_cfg.connectRequestedAtMs && *g_cfg.connectRequestedAtMs > 0) &&
          ((now - *g_cfg.connectRequestedAtMs) < connectGraceMs);

        if (!inConnectGrace && now - lastUnderrunPrintAt > 2000) {
          underrunCount++;
          lastUnderrunPrintAt = now;
          logf("[AUDIO] underrun: inBufferFilled=0 (count=%lu)\n", (unsigned long)underrunCount);
        }

        if ((now - emptySince) > 8000 && (now - lastRecoveryAt) > 30000 &&
            g_cfg.lastConnectUrl && g_cfg.lastConnectUrl->length()) {
          if (!g_cfg.connectRequestedAtMs || ((now - *g_cfg.connectRequestedAtMs) > 10000)) {
            lastRecoveryAt = now;
            logf("[AUDIO] watchdog recovery: reconnecting to %s\n", g_cfg.lastConnectUrl->c_str());
            g_cfg.audio->stopSong();
            vTaskDelay(pdMS_TO_TICKS(20));
            g_cfg.audio->connecttohost(g_cfg.lastConnectUrl->c_str());
            if (g_cfg.connectRequestedAtMs) *g_cfg.connectRequestedAtMs = millis();
          }
        }
      }
    } else {
      emptySince = 0;
    }
#endif

    vTaskDelay(1);
  }
}

void stream_core_begin(const StreamCoreConfig& cfg) {
  g_cfg = cfg;

  if (!g_audioQ) {
    g_audioQ = xQueueCreate(6, sizeof(AudioCmd));
  }

  if (!g_audioTaskHandle) {
    xTaskCreatePinnedToCore(
      streamAudioTask,
      "audioTask",
      g_cfg.taskStack,
      nullptr,
      g_cfg.taskPriority,
      &g_audioTaskHandle,
      g_cfg.taskCore
    );
  }
}

void stream_core_sendVolume(uint8_t v) {
  if (!g_audioQ) return;
  AudioCmd c{};
  c.type = ACMD_SET_VOL;
  c.vol = v;
  xQueueSend(g_audioQ, &c, 0);
}

void stream_core_sendStop() {
  if (!g_audioQ) return;
  AudioCmd c{};
  c.type = ACMD_STOP;
  xQueueSend(g_audioQ, &c, 0);
}

void stream_core_sendConnect(const String& url) {
  if (g_cfg.lastConnectUrl) *g_cfg.lastConnectUrl = url;
  if (g_cfg.connectRequestedAtMs) *g_cfg.connectRequestedAtMs = millis();
  if (!g_audioQ) return;

  AudioCmd c{};
  c.type = ACMD_CONNECT_URL;
  strlcpy(c.url, url.c_str(), sizeof(c.url));
  xQueueSend(g_audioQ, &c, 0);
}

void stream_core_readBuffer(size_t& filled, size_t& freeb, size_t& total, int& percent) {
  filled = 0;
  freeb = 0;
  total = 0;
  percent = 0;

  if (!g_cfg.audio) return;

  filled = g_cfg.audio->inBufferFilled();
  freeb  = g_cfg.audio->inBufferFree();
  total  = filled + freeb;

  if (total > 0) {
    percent = (int)((filled * 100) / total);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
  }
}
