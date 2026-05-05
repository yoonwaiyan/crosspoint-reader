#include "ClockConfig.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

static constexpr const char* CONFIG_PATH = "/clock-config.json";

bool ClockConfig::loadFromSd() {
  if (!Storage.exists(CONFIG_PATH)) {
    LOG_DBG("CLKCFG", "No clock-config.json found, using defaults");
    return false;
  }

  HalFile file;
  if (!Storage.openFileForRead("CLKCFG", CONFIG_PATH, file)) {
    LOG_ERR("CLKCFG", "Cannot open %s", CONFIG_PATH);
    return false;
  }

  // Read entire file into a stack buffer (max 512 bytes — small config)
  char buf[512];
  const size_t len = file.read(reinterpret_cast<uint8_t*>(buf), sizeof(buf) - 1);
  file.close();
  buf[len] = '\0';

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, buf, len);
  if (err) {
    LOG_ERR("CLKCFG", "JSON parse error: %s", err.c_str());
    return false;
  }

  if (const char* url = doc["endpoint_url"]) {
    strncpy(endpointUrl, url, sizeof(endpointUrl) - 1);
  }
  if (const char* tok = doc["auth_token"]) {
    strncpy(authToken, tok, sizeof(authToken) - 1);
  }
  if (doc["timezone_offset_minutes"].is<int>()) {
    timezoneOffsetMinutes = doc["timezone_offset_minutes"].as<int16_t>();
  }
  if (const char* ntp = doc["ntp_server"]) {
    strncpy(ntpServer, ntp, sizeof(ntpServer) - 1);
  }

  LOG_INF("CLKCFG", "Loaded: endpoint=%s tz=%d ntp=%s",
          endpointUrl[0] ? endpointUrl : "(none)", timezoneOffsetMinutes, ntpServer);
  return true;
}
