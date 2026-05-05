#include "ClockActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "fontIds.h"

void ClockActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void ClockActivity::loop() {
  mappedInput.update();
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    finish();
  }
}

void ClockActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const int cx = renderer.getScreenWidth() / 2;
  const int lineH = renderer.getLineHeight(NOTOSERIF_18_FONT_ID);
  const int y = (renderer.getScreenHeight() - lineH) / 2;

  renderer.drawCenteredText(NOTOSERIF_18_FONT_ID, y, "Hello");

  const int hintY = renderer.getScreenHeight() - renderer.getLineHeight(UI_10_FONT_ID) - 8;
  renderer.drawCenteredText(UI_10_FONT_ID, hintY, tr(STR_CLOCK_BACK));

  renderer.displayBuffer();
}
