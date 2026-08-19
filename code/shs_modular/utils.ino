// ============================================================================
//  utils.ino — small shared helpers
// ============================================================================
#include "config.h"

// True only for a real, finite number (filters NaN / inf before display/MQTT).
bool isValidFloat(float v) {
  return !isnan(v) && !isinf(v);
}

// The configured reporting cadence in milliseconds. Stored in minutes because
// that is the unit the setting is meaningful in; the clamp keeps a stray value
// in NVS from disabling reporting altogether or hammering the backend.
uint32_t publishIntervalMs() {
  uint32_t minutes = settings.publishIntervalMin;
  if (minutes < MIN_PUBLISH_INTERVAL_MIN) minutes = MIN_PUBLISH_INTERVAL_MIN;
  if (minutes > MAX_PUBLISH_INTERVAL_MIN) minutes = MAX_PUBLISH_INTERVAL_MIN;
  return minutes * 60UL * 1000UL;
}
