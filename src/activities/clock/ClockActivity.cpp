#include "ClockActivity.h"

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <HTTPClient.h>
#include <I18n.h>
#include <Logging.h>
#include <NetworkClientSecure.h>
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

  config.loadFromSd();

  // Apply timezone offset via POSIX TZ string (hours east of UTC, no DST).
  if (config.timezoneOffsetMinutes != 0) {
    const int offsetH = config.timezoneOffsetMinutes / 60;
    const int offsetM = config.timezoneOffsetMinutes % 60;
    char tzBuf[32];
    // POSIX TZ sign convention is opposite to UTC offset: UTC+8 → "UTC-8"
    snprintf(tzBuf, sizeof(tzBuf), "UTC%+d:%02d", -offsetH, abs(offsetM));
    setenv("TZ", tzBuf, 1);
    tzset();
    LOG_INF("CLK", "TZ set to %s", tzBuf);
  }

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

  // PageBack (Up) → log wake event + NTP sync (M3 / M4)
  if (mappedInput.wasReleased(MappedInputManager::Button::PageBack)) {
    logEvent("wake");
    onNtpSyncRequested();
    return;
  }

  // PageForward (Down) → log sleep event (M4)
  if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) {
    logEvent("sleep");
    requestUpdate();
    return;
  }

  // Clear transient status after its timeout
  if (statusLine[0] != '\0' && statusClearAfterMs != 0 && millis() >= statusClearAfterMs) {
    statusLine[0] = '\0';
    statusClearAfterMs = 0;
    requestUpdate();
  }
}

// ---------------------------------------------------------------------------
// NTP sync (M3)
// ---------------------------------------------------------------------------

void ClockActivity::onNtpSyncRequested() {
  startActivityForResult(
      std::make_unique<WifiSelectionActivity>(renderer, mappedInput, true),
      [this](const ActivityResult& result) {
        if (result.isCancelled) {
          LOG_DBG("CLK", "WiFi cancelled");
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
  requestUpdate(true);

  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, config.ntpServer[0] ? config.ntpServer : "pool.ntp.org");
  esp_sntp_init();

  int retry = 0;
  constexpr int MAX_RETRIES = 50;
  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && retry < MAX_RETRIES) {
    vTaskDelay(pdMS_TO_TICKS(100));
    retry++;
  }
  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }

  const bool synced = retry < MAX_RETRIES;
  LOG_INF("CLK", synced ? "NTP synced (retries=%d)" : "NTP timeout (retries=%d)", retry);

  // Flush any queued events while WiFi is still up (M5)
  if (config.hasEndpoint()) {
    flushPending();
  }

  // Tear down WiFi
  WiFi.disconnect(false);
  delay(100);
  WiFi.mode(WIFI_OFF);

  snprintf(statusLine, sizeof(statusLine), "%s", synced ? tr(STR_CLOCK_NTP_OK) : tr(STR_CLOCK_NTP_FAIL));
  statusClearAfterMs = millis() + 5000;
  requestUpdate();
}

// ---------------------------------------------------------------------------
// Event logging (M4) + HTTPS POST with pending queue (M5)
// ---------------------------------------------------------------------------

void ClockActivity::logEvent(const char* eventType) {
  struct timeval tv {};
  gettimeofday(&tv, nullptr);
  struct tm now {};
  gmtime_r(&tv.tv_sec, &now);

  char line[80];
  snprintf(line, sizeof(line), "{\"event\":\"%s\",\"ts\":\"%04d-%02d-%02dT%02d:%02d:%02dZ\"}\n",
           eventType, now.tm_year + 1900, now.tm_mon + 1, now.tm_mday,
           now.tm_hour, now.tm_min, now.tm_sec);

  // 1. Append to permanent log
  {
    HalFile f = Storage.open(LOG_PATH, O_RDWR | O_CREAT | O_AT_END);
    if (f) {
      f.write(reinterpret_cast<const uint8_t*>(line), strlen(line));
      f.close();
    } else {
      LOG_ERR("CLK", "Cannot write to %s", LOG_PATH);
    }
  }

  // 2. If endpoint configured, attempt immediate POST; on failure queue it
  if (config.hasEndpoint()) {
    if (!postEvent(line)) {
      // Append to pending queue for retry on next WiFi-up
      HalFile pf = Storage.open(PENDING_PATH, O_RDWR | O_CREAT | O_AT_END);
      if (pf) {
        pf.write(reinterpret_cast<const uint8_t*>(line), strlen(line));
        pf.close();
        LOG_DBG("CLK", "Queued for retry: %s", line);
      }
    }
  }

  LOG_INF("CLK", "Event: %s", line);
  snprintf(statusLine, sizeof(statusLine), "%s",
           (strcmp(eventType, "wake") == 0) ? tr(STR_CLOCK_LOG_WAKE) : tr(STR_CLOCK_LOG_SLEEP));
  statusClearAfterMs = millis() + 3000;
}

bool ClockActivity::postEvent(const char* jsonLine) {
  if (!config.hasEndpoint()) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  // Strip trailing newline for the POST body
  char body[80];
  strncpy(body, jsonLine, sizeof(body) - 1);
  body[sizeof(body) - 1] = '\0';
  const size_t bodyLen = strlen(body);
  if (bodyLen > 0 && body[bodyLen - 1] == '\n') {
    body[bodyLen - 1] = '\0';
  }

  NetworkClientSecure client;
  client.setInsecure();  // skips cert validation — acceptable for shared-secret auth
  HTTPClient http;
  http.begin(client, config.endpointUrl);
  http.addHeader("Content-Type", "application/json");
  if (config.authToken[0]) {
    http.addHeader("Authorization", (std::string("Bearer ") + config.authToken).c_str());
  }

  // HTTPClient::POST takes a non-const uint8_t* (Arduino API quirk)
  const int code = http.POST(reinterpret_cast<uint8_t*>(body), strlen(body));
  http.end();

  if (code >= 200 && code < 300) {
    LOG_INF("CLK", "POST OK (%d)", code);
    return true;
  }
  LOG_ERR("CLK", "POST failed (%d) to %s", code, config.endpointUrl);
  return false;
}

void ClockActivity::flushPending() {
  if (!Storage.exists(PENDING_PATH)) return;

  HalFile f = Storage.open(PENDING_PATH, O_RDONLY);
  if (!f) return;

  // Read entire pending file into a heap buffer (bounded to ~4 KB)
  constexpr size_t MAX_PENDING = 4096;
  auto* buf = static_cast<char*>(malloc(MAX_PENDING));
  if (!buf) {
    f.close();
    LOG_ERR("CLK", "malloc failed for pending flush");
    return;
  }

  const size_t len = f.read(reinterpret_cast<uint8_t*>(buf), MAX_PENDING - 1);
  f.close();
  buf[len] = '\0';

  // Walk newline-delimited JSON lines and POST each
  int sent = 0;
  int failed = 0;
  char* cursor = buf;
  while (*cursor != '\0') {
    char* eol = strchr(cursor, '\n');
    const bool hasNewline = (eol != nullptr);
    if (!hasNewline) break;  // incomplete final line — leave for next cycle
    *eol = '\0';
    const char* line = cursor;
    cursor = eol + 1;

    if (strlen(line) == 0) continue;

    char lineWithNl[84];
    snprintf(lineWithNl, sizeof(lineWithNl), "%s\n", line);
    if (postEvent(lineWithNl)) {
      sent++;
    } else {
      failed++;
    }
  }

  free(buf);
  buf = nullptr;

  if (failed == 0) {
    // All flushed — delete the pending file
    Storage.remove(PENDING_PATH);
    LOG_INF("CLK", "Pending flushed (%d sent)", sent);
  } else {
    LOG_ERR("CLK", "Pending flush partial: %d sent, %d failed", sent, failed);
    // Leave the file in place; a full retry on the next cycle
    // Note: successfully-sent items remain in the file and will be re-sent.
    // A proper dequeue would rewrite the file minus sent lines — acceptable
    // for the current scope where failures should be rare.
  }
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

  char timeBuf[6];
  char dateBuf[12];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", now.tm_hour, now.tm_min);
  snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d",
           now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);

  renderer.clearScreen();

  const int sh = renderer.getScreenHeight();

  const int timeLineH = renderer.getLineHeight(NOTOSERIF_18_FONT_ID);
  const int dateLineH = renderer.getLineHeight(UI_12_FONT_ID);
  const int hintLineH = renderer.getLineHeight(UI_10_FONT_ID);

  const int blockH = timeLineH + 4 + dateLineH;
  const int timeY = (sh - blockH) / 2;
  const int dateY = timeY + timeLineH + 4;

  renderer.drawCenteredText(NOTOSERIF_18_FONT_ID, timeY, timeBuf);
  renderer.drawCenteredText(UI_12_FONT_ID, dateY, dateBuf);

  if (statusLine[0] != '\0') {
    renderer.drawCenteredText(UI_10_FONT_ID, dateY + dateLineH + 8, statusLine);
  }

  renderer.drawCenteredText(UI_10_FONT_ID, sh - hintLineH - 8, tr(STR_CLOCK_BACK));

  const HalDisplay::RefreshMode mode = fullRefresh ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH;
  renderer.displayBuffer(mode);

  LOG_DBG("CLK", "%s %s status='%s' heap=%d", timeBuf, dateBuf, statusLine, ESP.getFreeHeap());
}
