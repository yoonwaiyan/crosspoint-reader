#pragma once

#include "../Activity.h"

/**
 * ClockActivity — M1 stub: renders "Hello" and exits on any button press.
 * Future milestones will replace this with an RTC-driven clock face.
 */
class ClockActivity final : public Activity {
 public:
  explicit ClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Clock", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
