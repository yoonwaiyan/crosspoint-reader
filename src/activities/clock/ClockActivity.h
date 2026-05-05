#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdint>

#include "../Activity.h"

/**
 * ClockActivity — NTP-syncable HH:MM clock with sleep/wake event logging.
 *
 * Button mapping:
 *   Back / Confirm      → return to Home
 *   PageBack  (Up)      → log "wake" event to SD, then NTP-sync time (M3/M4)
 *   PageForward (Down)  → log "sleep" event to SD (M4)
 *
 * A background tick task wakes at each minute boundary and triggers a
 * partial re-render. A full panel refresh fires every 60 renders (~1 h).
 */
class ClockActivity final : public Activity {
  // Tick task — fires requestUpdate() at each minute boundary.
  TaskHandle_t tickTaskHandle = nullptr;
  static void tickTaskTrampoline(void* param);
  [[noreturn]] void tickTask();

  // Ghosting suppression: full refresh once per hour.
  int rendersSinceFullRefresh = 0;
  static constexpr int FULL_REFRESH_INTERVAL = 60;

  // Status line shown below the date (e.g. "Time synced", "Sync failed").
  char statusLine[32] = {};
  uint32_t statusClearAfterMs = 0;  // millis() value at which to clear

  void renderClock(bool fullRefresh);

  // NTP + WiFi helpers (M3)
  void onNtpSyncRequested();
  void doNtpSync();

  // Sleep/wake event logging (M4)
  static constexpr const char* LOG_PATH = "/sleep-log.jsonl";
  void logEvent(const char* eventType);

 public:
  explicit ClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Clock", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
