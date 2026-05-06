#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdint>

#include "../Activity.h"
#include "ClockConfig.h"

/**
 * ClockActivity — NTP-syncable HH:MM clock with sleep/wake event logging.
 *
 * Button mapping:
 *   Back / Confirm      → return to Home
 *   PageBack  (Up)      → log "wake" event to SD + HTTPS POST, then NTP-sync
 *   PageForward (Down)  → log "sleep" event to SD + HTTPS POST
 *
 * Events that fail to POST are queued in /sleep-log-pending.jsonl and
 * retried on the next WiFi-up cycle (next PageBack press).
 *
 * Configuration is read from /clock-config.json at onEnter().
 */
class ClockActivity final : public Activity {
  ClockConfig config;

  // Tick task — fires requestUpdate() at each minute boundary.
  TaskHandle_t tickTaskHandle = nullptr;
  static void tickTaskTrampoline(void* param);
  [[noreturn]] void tickTask();

  // Ghosting suppression: full refresh once per hour.
  int rendersSinceFullRefresh = 0;
  static constexpr int FULL_REFRESH_INTERVAL = 60;

  // Transient status line shown below the date.
  char statusLine[32] = {};
  uint32_t statusClearAfterMs = 0;

  void renderClock(bool fullRefresh);

  // NTP + WiFi helpers (M3)
  void onNtpSyncRequested();
  void doNtpSync();

  // Event logging + HTTPS POST (M4 / M5)
  static constexpr const char* LOG_PATH = "/sleep-log.jsonl";
  static constexpr const char* PENDING_PATH = "/sleep-log-pending.jsonl";
  void logEvent(const char* eventType);
  bool postEvent(const char* jsonLine);   // Returns true on HTTP 2xx
  void flushPending();                    // Retry queued events; called after WiFi is up

 public:
  explicit ClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Clock", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

  // Keep the system awake so the tick task fires every minute uninterrupted.
  bool preventAutoSleep() override { return true; }
};
