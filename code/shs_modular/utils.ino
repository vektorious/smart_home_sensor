// ============================================================================
//  utils.ino — small shared helpers
// ============================================================================
#include "config.h"

// True only for a real, finite number (filters NaN / inf before display/MQTT).
bool isValidFloat(float v) {
  return !isnan(v) && !isinf(v);
}
