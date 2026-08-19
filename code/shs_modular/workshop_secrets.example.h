// ============================================================================
//  workshop_secrets.example.h — copy to workshop_secrets.h and fill in.
//
//  ONLY the short-workshop (SENSORBOARD) build reads this file. It bakes an
//  instructor-provided API key and project into the image so students can flash
//  and publish with zero configuration.
//
//  workshop_secrets.h is gitignored. Without it the build still compiles: the
//  device then falls back to an anonymous, temporary device (no API key, random
//  write key in NVS) — fine for testing, but the 48 h idle expiry applies.
//
//  ⚠ The workshop image is a TEMPORARY artefact. The key and the salt below end
//  up inside a binary that is served publicly by the web flasher. Rotate both
//  per workshop and delete the image from gh-pages afterwards. See
//  web-flasher/README.md § "Workshop image".
// ============================================================================
#pragma once

// API key issued by the diy-sensor.org operator. Makes every student device
// persistent (exempt from the idle sweep) and raises its rate limits.
#define WORKSHOP_API_KEY   "replace-me-with-the-workshop-api-key"

// Dashboard grouping. All devices flashed with this image land in one project
// view at diy-sensor.org/dashboard/project/<slug>. Keep to [a-z0-9-].
#define WORKSHOP_PROJECT   "shs-workshop-2026"

// Salt for the per-device write key: writeKey = HMAC-SHA256(salt, efuse MAC),
// truncated to 32 hex chars. Never stored in NVS — recomputed at every boot, so
// it survives a factory reset AND a full flash erase, which is what keeps a
// student from permanently orphaning their device ID.
//
// Generate a fresh one per workshop:  openssl rand -hex 32
#define WORKSHOP_KEY_SALT  "replace-me-with-32-random-bytes-in-hex"
