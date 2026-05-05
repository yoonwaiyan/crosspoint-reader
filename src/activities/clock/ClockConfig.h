#pragma once

#include <cstdint>

/**
 * ClockConfig — parsed from /clock-config.json on the SD card.
 * All fields have safe defaults so the clock works without the file.
 */
struct ClockConfig {
  char endpointUrl[256] = {};   // HTTPS POST target (empty = disabled)
  char authToken[128] = {};     // Sent as "Authorization: Bearer <token>"
  int16_t timezoneOffsetMinutes = 0;  // Applied via posix TZ string at boot
  char ntpServer[64] = "pool.ntp.org";

  // Returns true if event posting is configured.
  bool hasEndpoint() const { return endpointUrl[0] != '\0'; }

  // Load from /clock-config.json; returns true on success.
  // On failure leaves the struct at its zero-initialised defaults.
  bool loadFromSd();
};
