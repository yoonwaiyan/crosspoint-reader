#include "ClockActivity.h"

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>
#include <esp_sntp.h>

#include <cstring>
#include <ctime>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "fontIds.h"

// ---------------------------------------------------------------------------
// Tick task — fires at each wall-clock minute boundary.
// ---------------------------------------------------------------------------

void ClockActivity::tickTaskTrampoline(void* param) {
  static_cast<ClockActivity*>(param)->tickTask();
}

[[noreturn]] void ClockActivity::tickTask() {
  while (true) {
    struct timeval tv {};
    gettimeofday(&tv, nullptr);
    const int secsUntilNext = 60 - static_cast<int>(tv.tv_sec % 60);
    vTaskDelay(pdMS_TO_TICKS(secsUntilNext * 1000UL));
    requestUpdate();
  }
}

// ---------------------------------------------------------------------------
// Activity lifecycle
// ---------------------------------------------------------------------------

void ClockActivity::onEnter() {
  Activity::onEnter();

  xTaskCreate(tickTaskTrampoline, "ClockTick", 2048, this, 1, &tickTaskHandle);

  requestUpdate();
}

void ClockActivity::onExit() {
  if (tickTaskHandle) {
    vTaskDelete(tickTaskHandle);
    tickTaskHandle = nullptr;
  }
  Activity::onExit();
}

void ClockActivity::loop() {
  mappedInput.update();

  // Exit clock mode
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    finish();
    return;
  }

  // PageBack (Up) → log wake + NTP sync
  if (mappedInput.wasReleased(MappedInputManager::Button::PageBack)) {
    logEvent("wake");
    onNtpSyncRequested();
    return;
  }

  // PageForward (Down) → log sleep
  if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) {
    logEvent("sleep");
    snprintf(statusLine, sizeof(statusLine), "%s", tr(STR_CLOCK_LOG_SLEEP));
    statusClearAfterMs = millis() + 3000;
    requestUpdate();
    return;
  }

  // Clear transient status line after its timeout
  if (statusLine[0] != '\0' && millis() >= statusClearAfterMs) {
    statusLine[0] = '\0';
    requestUpdate();
  }
}

// ---------------------------------------------------------------------------
// NTP sync (M3)
// ---------------------------------------------------------------------------

void ClockActivity::onNtpSyncRequested() {
  // Reuse the existing WifiSelectionActivity; autoConnect=true attempts the
  // last-saved network silently before showing the network list.
  startActivityForResult(
      std::make_unique<WifiSelectionActivity>(renderer, mappedInput, true),
      [this](const ActivityResult& result) {
        if (result.isCancelled) {
          LOG_DBG("CLK", "WiFi cancelled, skipping NTP");
          snprintf(statusLine, sizeof(statusLine), "%s", tr(STR_CLOCK_NTP_FAIL));
          statusClearAfterMs = millis() + 3000;
          requestUpdate();
          return;
        }
        doNtpSync();
      });
}

void ClockActivity::doNtpSync() {
  snprintf(statusLine, sizeof(statusLine), "%s", tr(STR_CLOCK_NTP_SYNCING));
  statusClearAfterMs = 0;
  requestUpdate(true);  // show "Syncing..." immediately

  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "pool.ntp.org");
  esp_sntp_init();

  int retry = 0;
  constexpr int MAX_RETRIES = 50;  // 5 s max
  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && retry < MAX_RETRIES) {
    vTaskDelay(pdMS_TO_TICKS(100));
    retry++;
  }
  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }

  // Tear WiFi down to save power
  WiFi.disconnect(false);
  delay(100);
  WiFi.mode(WIFI_OFF);

  if (retry < MAX_RETRIES) {
    LOG_INF("CLK", "NTP synced (retries=%d)", retry);
    snprintf(statusLine, sizeof(statusLine), "%s", tr(STR_CLOCK_NTP_OK));
  } else {
    LOG_ERR("CLK", "NTP sync timeout");
    snprintf(statusLine, sizeof(statusLine), "%s", tr(STR_CLOCK_NTP_FAIL));
  }
  statusClearAfterMs = millis() + 5000;
  requestUpdate();
}

// ---------------------------------------------------------------------------
// Sleep/wake event logging (M4)
// ---------------------------------------------------------------------------

void ClockActivity::logEvent(const char* eventType) {
  struct timeval tv {};
  gettimeofday(&tv, nullptr);
  struct tm now {};
  gmtime_r(&tv.tv_sec, &now);

  // ISO-8601 UTC timestamp + newline-terminated JSON line
  char line[64];
  snprintf(line, sizeof(line), "{\"event\":\"%s\",\"ts\":\"%04d-%02d-%02dT%02d:%02d:%02dZ\"}\n",
           eventType, now.tm_year + 1900, now.tm_mon + 1, now.tm_mday,
           now.tm_hour, now.tm_min, now.tm_sec);

  // Append to /sleep-log.jsonl; Storage.open() passes flags directly to SdFat.
  HalFile file = Storage.open(LOG_PATH, O_RDWR | O_CREAT | O_AT_END);
  if (!file) {
    LOG_ERR("CLK", "Cannot open %s for append", LOG_PATH);
    return;
  }
  file.write(reinterpret_cast<const uint8_t*>(line), strlen(line));
  file.close();

  LOG_INF("CLK", "Logged: %s", line);
  snprintf(statusLine, sizeof(statusLine), "%s",
           (strcmp(eventType, "wake") == 0) ? tr(STR_CLOCK_LOG_WAKE) : tr(STR_CLOCK_LOG_SLEEP));
  statusClearAfterMs = millis() + 3000;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void ClockActivity::render(RenderLock&&) {
  rendersSinceFullRefresh++;
  const bool doFull = (rendersSinceFullRefresh >= FULL_REFRESH_INTERVAL);
  if (doFull) rendersSinceFullRefresh = 0;
  renderClock(doFull);
}

void ClockActivity::renderClock(bool fullRefresh) {
  struct timeval tv {};
  gettimeofday(&tv, nullptr);
  struct tm now {};
  localtime_r(&tv.tv_sec, &now);

  char timeBuf[6];   // "HH:MM\0"
  char dateBuf[12];  // "YYYY-MM-DD\0"
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", now.tm_hour, now.tm_min);
  snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d",
           now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);

  renderer.clearScreen();

  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();

  const int timeLineH = renderer.getLineHeight(NOTOSERIF_18_FONT_ID);
  const int dateLineH = renderer.getLineHeight(UI_12_FONT_ID);
  const int hintLineH = renderer.getLineHeight(UI_10_FONT_ID);

  // Centre the time, date below it
  const int blockH = timeLineH + dateLineH + 4;
  const int timeY = (sh - blockH) / 2;
  const int dateY = timeY + timeLineH + 4;

  renderer.drawCenteredText(NOTOSERIF_18_FONT_ID, timeY, timeBuf);
  renderer.drawCenteredText(UI_12_FONT_ID, dateY, dateBuf);

  // Transient status line (NTP result, log confirmation, etc.)
  if (statusLine[0] != '\0') {
    const int statusY = dateY + dateLineH + 8;
    renderer.drawCenteredText(UI_10_FONT_ID, statusY, statusLine);
  }

  // Button hints at bottom
  const int hintY = sh - hintLineH - 8;
  renderer.drawCenteredText(UI_10_FONT_ID, hintY, tr(STR_CLOCK_BACK));

  const HalDisplay::RefreshMode mode = fullRefresh ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH;
  renderer.displayBuffer(mode);

  LOG_DBG("CLK", "%s %s status='%s' heap=%d", timeBuf, dateBuf, statusLine, ESP.getFreeHeap());
}
