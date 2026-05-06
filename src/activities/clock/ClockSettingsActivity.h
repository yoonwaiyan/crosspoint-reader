#pragma once

#include <string>

#include "../Activity.h"
#include "ClockSettings.h"
#include "util/ButtonNavigator.h"

/**
 * ClockSettingsActivity — two-state UI for editing ClockSettings.
 *
 * LIST state  : scrollable list of all settings with current values on the right.
 * PICKER state: scrollable list of options for the selected setting.
 *
 * Changes are written to SD when the user presses Back from the LIST state.
 */
class ClockSettingsActivity final : public Activity {
 public:
  // Which setting row is being edited in PICKER state.
  enum class Setting { TIMEZONE, ORIENTATION, FONT_FAMILY, FONT_SIZE, FONT_WEIGHT, TIME_FORMAT, DATE_FORMAT };
  static constexpr int SETTING_COUNT = 7;

 private:
  ClockSettings* settings;  // owned by ClockActivity — valid while on stack
  ButtonNavigator nav;

  enum class State { LIST, PICKER } state = State::LIST;

  int listIndex   = 0;
  int pickerIndex = 0;
  Setting currentSetting = Setting::TIMEZONE;

  // --- helpers ---
  int  getPickerItemCount() const;
  int  getCurrentSettingValue() const;
  void applyPickerSelection(int index);
  std::string getSettingCurrentValue(int settingIndex) const;
  std::string getPickerItemLabel(int index) const;
  const char* getSettingName(int settingIndex) const;

  void enterPicker(Setting s);
  void confirmPicker();
  void renderList();
  void renderPicker();

 public:
  explicit ClockSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, ClockSettings* settings)
      : Activity("ClockSettings", renderer, mappedInput), settings(settings) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
