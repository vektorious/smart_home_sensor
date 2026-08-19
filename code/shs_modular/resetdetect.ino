// ============================================================================
//  resetdetect.ino — double-reset detection.
//
//  Detects two resets in quick succession and uses that to force the setup
//  portal open even when Wi-Fi credentials are already saved. On a device that
//  never sleeps and has no buttons other than RESET and BOOT, this is the only
//  way back into configuration short of reflashing — so it is what a student
//  uses to fix a mistyped broker address or move the device to a new network.
//
//  The "armed" flag lives in NVS rather than RTC RAM: pressing RESET pulls EN
//  low, and RTC RAM is not reliably retained across an EN reset on all
//  ESP32-C6 boards, whereas NVS always survives.
//
//  Cost: every boot blocks for the length of the detection window. On an
//  always-on device that boots rarely, three seconds once is a fair price, and
//  the window is shown on the display so it doubles as an invitation.
// ============================================================================
#include "config.h"
#include <Preferences.h>

static const uint32_t DRD_WINDOW_MS = 3000;

static void drdSetArmed(bool armed) {
  Preferences p;
  p.begin("drd", /* readOnly */ false);
  p.putBool("armed", armed);
  p.end();
}

bool detectDoubleReset() {
  Preferences p;
  p.begin("drd", /* readOnly */ true);
  bool armed = p.getBool("armed", false);
  p.end();

  if (armed) {
    drdSetArmed(false);   // consume it
    Serial.println("Double reset detected — opening setup portal");
    return true;
  }

  // First reset: arm the flag and hold the window open for a second press.
  drdSetArmed(true);
  Serial.println("Press RESET again within 3 s to open the setup portal...");
  displayStatus("RESET x2 = setup", COLOR_INFO);
  delay(DRD_WINDOW_MS);
  drdSetArmed(false);     // window elapsed with no second press
  return false;
}
