#include "ClockSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ClockSettingsActivity::onEnter() {
  Activity::onEnter();
  state      = State::LIST;
  listIndex  = 0;
  requestUpdate();
}

void ClockSettingsActivity::onExit() { Activity::onExit(); }

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void ClockSettingsActivity::loop() {
  if (state == State::LIST) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      settings->saveToSd();
      finish();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      enterPicker(static_cast<Setting>(listIndex));
      return;
    }
    nav.onNextRelease([this] {
      listIndex = ButtonNavigator::nextIndex(listIndex, SETTING_COUNT);
      requestUpdate();
    });
    nav.onPreviousRelease([this] {
      listIndex = ButtonNavigator::previousIndex(listIndex, SETTING_COUNT);
      requestUpdate();
    });

  } else {  // PICKER
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      state = State::LIST;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      confirmPicker();
      return;
    }
    const int total = getPickerItemCount();
    nav.onNextRelease([this, total] {
      pickerIndex = ButtonNavigator::nextIndex(pickerIndex, total);
      requestUpdate();
    });
    nav.onPreviousRelease([this, total] {
      pickerIndex = ButtonNavigator::previousIndex(pickerIndex, total);
      requestUpdate();
    });
  }
}

// ---------------------------------------------------------------------------
// State transitions
// ---------------------------------------------------------------------------

void ClockSettingsActivity::enterPicker(Setting s) {
  currentSetting = s;
  pickerIndex    = getCurrentSettingValue();
  state          = State::PICKER;
  requestUpdate();
}

void ClockSettingsActivity::confirmPicker() {
  applyPickerSelection(pickerIndex);
  state = State::LIST;
  requestUpdate();
}

// ---------------------------------------------------------------------------
// Data helpers
// ---------------------------------------------------------------------------

int ClockSettingsActivity::getPickerItemCount() const {
  switch (currentSetting) {
    case Setting::TIMEZONE:     return ClockSettings::TIMEZONE_COUNT;
    case Setting::ORIENTATION:  return ClockSettings::ORIENTATION_COUNT;
    case Setting::FONT_FAMILY:  return ClockSettings::FONT_FAMILY_COUNT;
    case Setting::FONT_SIZE:    return ClockSettings::FONT_FAMILIES[settings->fontFamilyIndex].sizeCount;
    case Setting::FONT_WEIGHT:  return ClockSettings::FONT_WEIGHT_COUNT;
    case Setting::TIME_FORMAT:  return ClockSettings::TIME_FORMAT_COUNT;
    case Setting::DATE_FORMAT:  return ClockSettings::DATE_FORMAT_COUNT;
  }
  return 0;
}

int ClockSettingsActivity::getCurrentSettingValue() const {
  switch (currentSetting) {
    case Setting::TIMEZONE:    return settings->timezoneIndex;
    case Setting::ORIENTATION: return settings->orientation;
    case Setting::FONT_FAMILY: return settings->fontFamilyIndex;
    case Setting::FONT_SIZE:   return settings->fontSizeIndex;
    case Setting::FONT_WEIGHT: return settings->fontWeight;
    case Setting::TIME_FORMAT: return settings->timeFormat;
    case Setting::DATE_FORMAT: return settings->dateFormat;
  }
  return 0;
}

void ClockSettingsActivity::applyPickerSelection(int index) {
  switch (currentSetting) {
    case Setting::TIMEZONE:    settings->timezoneIndex   = static_cast<uint8_t>(index); break;
    case Setting::ORIENTATION: settings->orientation     = static_cast<uint8_t>(index); break;
    case Setting::FONT_FAMILY:
      settings->fontFamilyIndex = static_cast<uint8_t>(index);
      // Clamp size index to new family's available sizes
      if (settings->fontSizeIndex >= ClockSettings::FONT_FAMILIES[index].sizeCount) {
        settings->fontSizeIndex = ClockSettings::FONT_FAMILIES[index].sizeCount - 1;
      }
      break;
    case Setting::FONT_SIZE:   settings->fontSizeIndex = static_cast<uint8_t>(index); break;
    case Setting::FONT_WEIGHT: settings->fontWeight    = static_cast<uint8_t>(index); break;
    case Setting::TIME_FORMAT: settings->timeFormat    = static_cast<uint8_t>(index); break;
    case Setting::DATE_FORMAT: settings->dateFormat    = static_cast<uint8_t>(index); break;
  }
}

std::string ClockSettingsActivity::getSettingCurrentValue(int idx) const {
  switch (static_cast<Setting>(idx)) {
    case Setting::TIMEZONE:    return ClockSettings::TIMEZONES[settings->timezoneIndex].label;
    case Setting::ORIENTATION: return ClockSettings::ORIENTATION_LABELS[settings->orientation];
    case Setting::FONT_FAMILY: return ClockSettings::FONT_FAMILIES[settings->fontFamilyIndex].name;
    case Setting::FONT_SIZE:   return ClockSettings::FONT_FAMILIES[settings->fontFamilyIndex]
                                          .sizes[settings->fontSizeIndex].label;
    case Setting::FONT_WEIGHT: return ClockSettings::FONT_WEIGHT_LABELS[settings->fontWeight];
    case Setting::TIME_FORMAT: return ClockSettings::TIME_FORMAT_LABELS[settings->timeFormat];
    case Setting::DATE_FORMAT: return ClockSettings::DATE_FORMAT_LABELS[settings->dateFormat];
  }
  return "";
}

std::string ClockSettingsActivity::getPickerItemLabel(int index) const {
  switch (currentSetting) {
    case Setting::TIMEZONE:    return ClockSettings::TIMEZONES[index].label;
    case Setting::ORIENTATION: return ClockSettings::ORIENTATION_LABELS[index];
    case Setting::FONT_FAMILY: return ClockSettings::FONT_FAMILIES[index].name;
    case Setting::FONT_SIZE:   return ClockSettings::FONT_FAMILIES[settings->fontFamilyIndex].sizes[index].label;
    case Setting::FONT_WEIGHT: return ClockSettings::FONT_WEIGHT_LABELS[index];
    case Setting::TIME_FORMAT: return ClockSettings::TIME_FORMAT_LABELS[index];
    case Setting::DATE_FORMAT: return ClockSettings::DATE_FORMAT_LABELS[index];
  }
  return "";
}

const char* ClockSettingsActivity::getSettingName(int idx) const {
  switch (static_cast<Setting>(idx)) {
    case Setting::TIMEZONE:    return tr(STR_CLOCK_TIMEZONE);
    case Setting::ORIENTATION: return tr(STR_CLOCK_DISPLAY);
    case Setting::FONT_FAMILY: return tr(STR_CLOCK_FONT_FAMILY);
    case Setting::FONT_SIZE:   return tr(STR_CLOCK_FONT_SIZE);
    case Setting::FONT_WEIGHT: return tr(STR_CLOCK_FONT_WEIGHT);
    case Setting::TIME_FORMAT: return tr(STR_CLOCK_TIME_FORMAT);
    case Setting::DATE_FORMAT: return tr(STR_CLOCK_DATE_FORMAT);
  }
  return "";
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void ClockSettingsActivity::render(RenderLock&&) {
  if (state == State::LIST) {
    renderList();
  } else {
    renderPicker();
  }
}

void ClockSettingsActivity::renderList() {
  renderer.clearScreen();
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();
  const auto& m = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, m.topPadding, sw, m.headerHeight}, tr(STR_CLOCK_SETTINGS));

  const int contentTop = m.topPadding + m.headerHeight + m.verticalSpacing;
  const int contentH   = sh - contentTop - m.buttonHintsHeight - m.verticalSpacing;

  GUI.drawList(
      renderer, Rect{0, contentTop, sw, contentH},
      SETTING_COUNT, listIndex,
      [this](int i) -> std::string { return getSettingName(i); },
      nullptr, nullptr,
      [this](int i) -> std::string { return getSettingCurrentValue(i); });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

void ClockSettingsActivity::renderPicker() {
  renderer.clearScreen();
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();
  const auto& m = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, m.topPadding, sw, m.headerHeight}, getSettingName(static_cast<int>(currentSetting)));

  const int contentTop = m.topPadding + m.headerHeight + m.verticalSpacing;
  const int contentH   = sh - contentTop - m.buttonHintsHeight - m.verticalSpacing;
  const int total      = getPickerItemCount();
  const int current    = getCurrentSettingValue();

  GUI.drawList(
      renderer, Rect{0, contentTop, sw, contentH},
      total, pickerIndex,
      [this](int i) -> std::string { return getPickerItemLabel(i); },
      nullptr, nullptr,
      [current](int i) -> std::string { return i == current ? tr(STR_SELECTED) : ""; });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
