#pragma once

#include <EpdFontFamily.h>
#include <cstdint>
#include <ctime>

/**
 * ClockSettings — display preferences for ClockActivity.
 * Persisted to /clock-settings.json on the SD card.
 * Separate from ClockConfig (network/endpoint settings).
 */
struct ClockSettings {
  // Timezone: index into TIMEZONES[]
  uint8_t timezoneIndex = 22;  // default UTC+8 KL / Singapore

  // Display orientation: 0=Portrait, 1=Landscape
  uint8_t orientation = 0;

  // Font family: index into FONT_FAMILIES[]
  uint8_t fontFamilyIndex = 0;  // NotoSerif

  // Font size: index into FONT_FAMILIES[fontFamilyIndex].sizes[]
  uint8_t fontSizeIndex = 2;  // 16pt for NotoSerif (small=0→12, medium=1→14, large=2→16, xl=3→18)

  // Font weight: 0=Regular, 1=Bold, 2=Italic, 3=Bold-Italic
  uint8_t fontWeight = 0;

  // Time format: 0=24h, 1=12h AM/PM
  uint8_t timeFormat = 0;

  // Date format: see DATE_FORMAT_LABELS[]
  uint8_t dateFormat = 0;

  // -----------------------------------------------------------------------
  // Persistence
  // -----------------------------------------------------------------------
  bool loadFromSd();
  bool saveToSd() const;

  // -----------------------------------------------------------------------
  // Rendering helpers
  // -----------------------------------------------------------------------

  // Font ID for the large time string, from the selected family + size.
  int getTimeFontId() const;

  // EpdFontFamily::Style for the selected weight.
  EpdFontFamily::Style getFontWeight() const;

  // POSIX TZ string for the selected timezone (e.g. "UTC-8").
  const char* getPosixTz() const;

  // Format time into buf (max 9 chars "12:34 PM\0"); returns buf.
  const char* formatTime(char* buf, size_t bufLen, const struct tm& t) const;

  // Format date into buf (max 20 chars); returns buf.
  const char* formatDate(char* buf, size_t bufLen, const struct tm& t) const;

  // -----------------------------------------------------------------------
  // Static option tables (used by ClockSettingsActivity)
  // -----------------------------------------------------------------------

  struct FontSizeEntry {
    int fontId;
    const char* label;
  };

  struct FontFamilyEntry {
    const char* name;
    const FontSizeEntry* sizes;
    uint8_t sizeCount;
  };

  static const FontFamilyEntry FONT_FAMILIES[];
  static constexpr int FONT_FAMILY_COUNT = 3;

  struct TimezoneEntry {
    const char* label;
    int16_t offsetMinutes;
  };
  static const TimezoneEntry TIMEZONES[];
  static constexpr int TIMEZONE_COUNT = 28;

  static constexpr const char* ORIENTATION_LABELS[] = {"Portrait", "Landscape"};
  static constexpr int ORIENTATION_COUNT = 2;

  static constexpr const char* FONT_WEIGHT_LABELS[] = {"Regular", "Bold", "Italic", "Bold-Italic"};
  static constexpr int FONT_WEIGHT_COUNT = 4;

  static constexpr const char* TIME_FORMAT_LABELS[] = {"24h  (14:30)", "12h  (2:30 PM)"};
  static constexpr int TIME_FORMAT_COUNT = 2;

  static constexpr const char* DATE_FORMAT_LABELS[] = {
      "2026-05-06",
      "06/05/2026",
      "05/06/2026",
      "Wed, 06 May 2026",
      "06 May 2026",
      "May 6, 2026",
  };
  static constexpr int DATE_FORMAT_COUNT = 6;
};
