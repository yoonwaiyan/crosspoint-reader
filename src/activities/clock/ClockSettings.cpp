#include "ClockSettings.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>

#include "fontIds.h"

static constexpr const char* SETTINGS_PATH = "/clock-settings.json";

// ---------------------------------------------------------------------------
// Font family / size table — drives both the settings picker and rendering.
// Add entries here when new fonts are compiled into the firmware.
// ---------------------------------------------------------------------------

static constexpr ClockSettings::FontSizeEntry NOTOSERIF_SIZES[] = {
    {NOTOSERIF_12_FONT_ID, "12 pt"},
    {NOTOSERIF_14_FONT_ID, "14 pt"},
    {NOTOSERIF_16_FONT_ID, "16 pt"},
    {NOTOSERIF_18_FONT_ID, "18 pt"},
};
static constexpr ClockSettings::FontSizeEntry NOTOSANS_SIZES[] = {
    {NOTOSANS_12_FONT_ID, "12 pt"},
    {NOTOSANS_14_FONT_ID, "14 pt"},
    {NOTOSANS_16_FONT_ID, "16 pt"},
    {NOTOSANS_18_FONT_ID, "18 pt"},
};
static constexpr ClockSettings::FontSizeEntry OPENDYSLEXIC_SIZES[] = {
    {OPENDYSLEXIC_8_FONT_ID,  " 8 pt"},
    {OPENDYSLEXIC_10_FONT_ID, "10 pt"},
    {OPENDYSLEXIC_12_FONT_ID, "12 pt"},
    {OPENDYSLEXIC_14_FONT_ID, "14 pt"},
};

const ClockSettings::FontFamilyEntry ClockSettings::FONT_FAMILIES[FONT_FAMILY_COUNT] = {
    {"NotoSerif",    NOTOSERIF_SIZES,    4},
    {"NotoSans",     NOTOSANS_SIZES,     4},
    {"OpenDyslexic", OPENDYSLEXIC_SIZES, 4},
};

// ---------------------------------------------------------------------------
// Timezone table
// ---------------------------------------------------------------------------

const ClockSettings::TimezoneEntry ClockSettings::TIMEZONES[TIMEZONE_COUNT] = {
    {"UTC-12  Baker Island",          -720},
    {"UTC-11  American Samoa",        -660},
    {"UTC-10  Hawaii",                -600},
    {"UTC-9   Alaska",                -540},
    {"UTC-8   Pacific (US/Canada)",   -480},
    {"UTC-7   Mountain (US/Canada)",  -420},
    {"UTC-6   Central (US/Canada)",   -360},
    {"UTC-5   Eastern (US/Canada)",   -300},
    {"UTC-4   Atlantic / Caracas",    -240},
    {"UTC-3   Buenos Aires",          -180},
    {"UTC-2   South Georgia",         -120},
    {"UTC-1   Azores",                 -60},
    {"UTC+0   London / Lisbon",           0},
    {"UTC+1   Paris / Berlin",           60},
    {"UTC+2   Cairo / Athens",          120},
    {"UTC+3   Moscow / Nairobi",        180},
    {"UTC+4   Dubai / Baku",            240},
    {"UTC+5   Karachi / Tashkent",      300},
    {"UTC+5:30 Mumbai / Kolkata",       330},
    {"UTC+5:45 Kathmandu",              345},
    {"UTC+6   Dhaka / Almaty",          360},
    {"UTC+7   Bangkok / Jakarta",       420},
    {"UTC+8   Beijing / KL / Singapore",480},
    {"UTC+9   Tokyo / Seoul",           540},
    {"UTC+9:30 Adelaide",               570},
    {"UTC+10  Sydney / Brisbane",       600},
    {"UTC+11  Solomon Islands",         660},
    {"UTC+12  Auckland / Fiji",         720},
};

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

bool ClockSettings::loadFromSd() {
  if (!Storage.exists(SETTINGS_PATH)) return false;

  HalFile file;
  if (!Storage.openFileForRead("CLKSET", SETTINGS_PATH, file)) {
    LOG_ERR("CLKSET", "Cannot open %s", SETTINGS_PATH);
    return false;
  }
  char buf[256];
  const size_t len = file.read(reinterpret_cast<uint8_t*>(buf), sizeof(buf) - 1);
  file.close();
  buf[len] = '\0';

  JsonDocument doc;
  if (deserializeJson(doc, buf, len)) return false;

  if (doc["timezoneIndex"].is<int>())   timezoneIndex   = doc["timezoneIndex"].as<uint8_t>();
  if (doc["orientation"].is<int>())     orientation     = doc["orientation"].as<uint8_t>();
  if (doc["fontFamilyIndex"].is<int>()) fontFamilyIndex = doc["fontFamilyIndex"].as<uint8_t>();
  if (doc["fontSizeIndex"].is<int>())   fontSizeIndex   = doc["fontSizeIndex"].as<uint8_t>();
  if (doc["fontWeight"].is<int>())      fontWeight      = doc["fontWeight"].as<uint8_t>();
  if (doc["timeFormat"].is<int>())      timeFormat      = doc["timeFormat"].as<uint8_t>();
  if (doc["dateFormat"].is<int>())      dateFormat      = doc["dateFormat"].as<uint8_t>();

  // Clamp indices to valid ranges
  if (timezoneIndex   >= TIMEZONE_COUNT)    timezoneIndex   = 22;
  if (orientation     >= ORIENTATION_COUNT) orientation     = 0;
  if (fontFamilyIndex >= FONT_FAMILY_COUNT) fontFamilyIndex = 0;
  const uint8_t maxSize = FONT_FAMILIES[fontFamilyIndex].sizeCount;
  if (fontSizeIndex   >= maxSize)           fontSizeIndex   = maxSize - 1;
  if (fontWeight      >= FONT_WEIGHT_COUNT) fontWeight      = 0;
  if (timeFormat      >= TIME_FORMAT_COUNT) timeFormat      = 0;
  if (dateFormat      >= DATE_FORMAT_COUNT) dateFormat      = 0;

  LOG_INF("CLKSET", "Loaded: tz=%d ori=%d fam=%d sz=%d wt=%d tf=%d df=%d",
          timezoneIndex, orientation, fontFamilyIndex, fontSizeIndex,
          fontWeight, timeFormat, dateFormat);
  return true;
}

bool ClockSettings::saveToSd() const {
  char buf[256];
  snprintf(buf, sizeof(buf),
           "{\"timezoneIndex\":%d,\"orientation\":%d,\"fontFamilyIndex\":%d,"
           "\"fontSizeIndex\":%d,\"fontWeight\":%d,\"timeFormat\":%d,\"dateFormat\":%d}\n",
           timezoneIndex, orientation, fontFamilyIndex, fontSizeIndex,
           fontWeight, timeFormat, dateFormat);

  HalFile file;
  if (!Storage.openFileForWrite("CLKSET", SETTINGS_PATH, file)) {
    LOG_ERR("CLKSET", "Cannot write %s", SETTINGS_PATH);
    return false;
  }
  file.write(reinterpret_cast<const uint8_t*>(buf), strlen(buf));
  file.close();
  LOG_INF("CLKSET", "Saved to %s", SETTINGS_PATH);
  return true;
}

// ---------------------------------------------------------------------------
// Rendering helpers
// ---------------------------------------------------------------------------

int ClockSettings::getTimeFontId() const {
  const uint8_t fi = fontFamilyIndex < FONT_FAMILY_COUNT ? fontFamilyIndex : 0;
  const auto& family = FONT_FAMILIES[fi];
  const uint8_t si = fontSizeIndex < family.sizeCount ? fontSizeIndex : family.sizeCount - 1;
  return family.sizes[si].fontId;
}

EpdFontFamily::Style ClockSettings::getFontWeight() const {
  switch (fontWeight) {
    case 1: return EpdFontFamily::BOLD;
    case 2: return EpdFontFamily::ITALIC;
    case 3: return EpdFontFamily::BOLD_ITALIC;
    default: return EpdFontFamily::REGULAR;
  }
}

const char* ClockSettings::getPosixTz() const {
  static char tz[16];
  const int16_t off = TIMEZONES[timezoneIndex < TIMEZONE_COUNT ? timezoneIndex : 22].offsetMinutes;
  const int h = off / 60;
  const int m = abs(off % 60);
  // POSIX sign is opposite to UTC offset: UTC+8 → "UTC-8"
  if (m == 0) {
    snprintf(tz, sizeof(tz), "UTC%+d", -h);
  } else {
    snprintf(tz, sizeof(tz), "UTC%+d:%02d", -h, m);
  }
  return tz;
}

const char* ClockSettings::formatTime(char* buf, size_t bufLen, const struct tm& t) const {
  if (timeFormat == 1) {
    // 12h AM/PM
    const int h12 = t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12;
    const char* ampm = t.tm_hour < 12 ? "AM" : "PM";
    snprintf(buf, bufLen, "%d:%02d %s", h12, t.tm_min, ampm);
  } else {
    snprintf(buf, bufLen, "%02d:%02d", t.tm_hour, t.tm_min);
  }
  return buf;
}

const char* ClockSettings::formatDate(char* buf, size_t bufLen, const struct tm& t) const {
  static constexpr const char* MONTHS[] = {
      "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  static constexpr const char* DAYS[] = {
      "Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

  const int y = t.tm_year + 1900;
  const int mo = t.tm_mon + 1;
  const int d = t.tm_mday;

  switch (dateFormat) {
    case 1: snprintf(buf, bufLen, "%02d/%02d/%04d", d, mo, y); break;
    case 2: snprintf(buf, bufLen, "%02d/%02d/%04d", mo, d, y); break;
    case 3: snprintf(buf, bufLen, "%s, %02d %s %04d", DAYS[t.tm_wday], d, MONTHS[t.tm_mon], y); break;
    case 4: snprintf(buf, bufLen, "%02d %s %04d", d, MONTHS[t.tm_mon], y); break;
    case 5: snprintf(buf, bufLen, "%s %d, %04d", MONTHS[t.tm_mon], d, y); break;
    default: snprintf(buf, bufLen, "%04d-%02d-%02d", y, mo, d); break;
  }
  return buf;
}
