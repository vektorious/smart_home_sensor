// ============================================================================
//  bme680.ino — BME680 read path via Bosch BSEC2
//  Initialises the sensor, applies the 3.3 V / LP tuning blob, restores and
//  persists the calibration state in NVS, and on every new sample fills a
//  SensorPacket and hands it to the display and MQTT modules.
//
//  NOTE: bsec2 ships no esp32c6 precompiled blob. The C6 is soft-float RISC-V
//  (rv32imac), ABI-compatible with the C3 blob, so src/esp32c6/libalgobsec.a
//  must be created as a copy of src/esp32c3/. A library update removes that
//  folder — recreate it if linking fails. See build_instructions.md §4.
// ============================================================================
#include "config.h"
#include <Wire.h>
#include <Preferences.h>
#include <bsec2.h>
#include "bsec_config_33v_3s_4d.h"  // BSEC tuning for 3.3 V supply, LP (3 s)

static Bsec2       envSensor;
static Preferences prefs;
static bool        bmeReady = false;

static uint8_t  bsecState[BSEC_MAX_STATE_BLOB_SIZE];
static uint32_t lastStateSave = 0;
static int      lastSavedAccuracy = -1;

// Virtual sensors we want BSEC to compute and hand back via the callback.
static bsecSensor sensorList[] = {
  BSEC_OUTPUT_IAQ,
  BSEC_OUTPUT_CO2_EQUIVALENT,
  BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
  BSEC_OUTPUT_RAW_PRESSURE,
  BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
  BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
};

static void checkBsecStatus() {
  char buf[24];

  if (envSensor.sensor.status < BME68X_OK) {
    // I2C/sensor-level failure (bad wiring, wrong address, no chip ID).
    Serial.printf("BME68x error %d\n", envSensor.sensor.status);
    snprintf(buf, sizeof(buf), "BME68x err %d", envSensor.sensor.status);
    displayStatus(buf, COLOR_ERR);
  } else if (envSensor.sensor.status > BME68X_OK) {
    Serial.printf("BME68x warning %d\n", envSensor.sensor.status);
  }

  if (envSensor.status < BSEC_OK) {
    // BSEC-library-level failure (init/version/subscription).
    Serial.printf("BSEC error %d\n", envSensor.status);
    snprintf(buf, sizeof(buf), "BSEC err %d", envSensor.status);
    displayStatus(buf, COLOR_ERR);
  } else if (envSensor.status > BSEC_OK) {
    Serial.printf("BSEC warning %d\n", envSensor.status);
  }
}

static void loadState() {
  prefs.begin("bsec", /* readOnly */ true);
  size_t len = prefs.getBytesLength("state");
  if (len == BSEC_MAX_STATE_BLOB_SIZE) {
    prefs.getBytes("state", bsecState, BSEC_MAX_STATE_BLOB_SIZE);
    prefs.end();
    if (envSensor.setState(bsecState)) {
      Serial.println("BSEC state restored from NVS");
    } else {
      checkBsecStatus();
    }
  } else {
    prefs.end();
    Serial.println("No saved BSEC state — starting fresh calibration");
  }
}

static void saveState() {
  if (!envSensor.getState(bsecState)) {
    checkBsecStatus();
    return;
  }
  prefs.begin("bsec", /* readOnly */ false);
  prefs.putBytes("state", bsecState, BSEC_MAX_STATE_BLOB_SIZE);
  prefs.end();
  lastStateSave = millis();
  Serial.println("BSEC state saved to NVS");
}

// Called by BSEC each time a new set of processed outputs is ready (~3 s in LP).
static void newDataCallback(const bme68xData data, const bsecOutputs outputs,
                            const Bsec2 bsec) {
  if (!outputs.nOutputs) return;

  SensorPacket p;
  for (uint8_t i = 0; i < outputs.nOutputs; i++) {
    const bsecData out = outputs.output[i];
    switch (out.sensor_id) {
      case BSEC_OUTPUT_IAQ:
        p.iaq = out.signal;
        p.iaqAccuracy = out.accuracy;
        break;
      case BSEC_OUTPUT_CO2_EQUIVALENT:                  p.co2  = out.signal; break;
      case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:           p.voc  = out.signal; break;
      // BSEC reports raw pressure already in hPa-scale here (~1014), so no
      // Pa->hPa division is needed — dividing gave a 100x-too-small value.
      case BSEC_OUTPUT_RAW_PRESSURE:                    p.pressure = out.signal; break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE: p.temperature = out.signal; break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:    p.humidity    = out.signal; break;
    }
  }

  displayUpdate(p);
  mqttPublish(p);   // no-op / throttled internally when USE_MQTT handles it

  Serial.printf("IAQ=%.0f(a%u)  CO2=%.0fppm  VOC=%.2fppm  T=%.2fC  H=%.2f%%  P=%.2fhPa\n",
                p.iaq, p.iaqAccuracy, p.co2, p.voc, p.temperature, p.humidity, p.pressure);

  // Persist calibration once it's trustworthy, then at most every few hours,
  // or whenever the accuracy level improves.
  if (p.iaqAccuracy >= 3 &&
      ((int)p.iaqAccuracy != lastSavedAccuracy ||
       millis() - lastStateSave >= STATE_SAVE_PERIOD_MS)) {
    saveState();
    lastSavedAccuracy = p.iaqAccuracy;
  }
}

// ---- Public API -----------------------------------------------------------

bool bme680Init() {
  Wire.begin(PIN_BME_SDA, PIN_BME_SCL);

  Serial.println("Probing BME680 at 0x76...");
  displayStatus("1: I2C probe", COLOR_INFO);
  bool ok = envSensor.begin(BME68X_I2C_ADDR_LOW, Wire);
  if (!ok) {
    Serial.printf("  0x76 failed (sensor.status=%d, bsec.status=%d)\n",
                  envSensor.sensor.status, envSensor.status);
    Serial.println("Probing BME680 at 0x77...");
    ok = envSensor.begin(BME68X_I2C_ADDR_HIGH, Wire);
  }
  if (!ok) {
    Serial.printf("  begin() failed (sensor.status=%d, bsec.status=%d)\n",
                  envSensor.sensor.status, envSensor.status);
    checkBsecStatus();
    return false;
  }
  Serial.printf("begin() OK — BSEC v%d.%d.%d.%d\n",
                envSensor.version.major, envSensor.version.minor,
                envSensor.version.major_bugfix, envSensor.version.minor_bugfix);

  // Apply the 3.3 V / LP tuning before restoring state and subscribing.
  // (Recommended order: init -> config -> state -> subscription.)
  envSensor.setConfig(bsec_config_iaq);
  if (envSensor.status < BSEC_OK) {
    Serial.printf("setConfig error %d\n", envSensor.status);
    checkBsecStatus();
    return false;
  }
  Serial.println("3.3 V LP config applied");

  // Compensate for external board/LCD self-heating (see TEMP_OFFSET_C).
  envSensor.setTemperatureOffset(TEMP_OFFSET_C);

  loadState();

  // Note: updateSubscription() returns false on any non-OK status, including
  // benign positive warnings (e.g. 14 = SAMPLERATEMISMATCH, raised when no
  // matching config blob is loaded). Only a negative status is a real error.
  envSensor.updateSubscription(sensorList,
                               sizeof(sensorList) / sizeof(sensorList[0]),
                               BSEC_SAMPLE_RATE_LP);
  if (envSensor.status < BSEC_OK) {
    Serial.printf("updateSubscription error %d\n", envSensor.status);
    checkBsecStatus();
    return false;
  }
  if (envSensor.status > BSEC_OK)
    Serial.printf("updateSubscription warning %d (continuing)\n", envSensor.status);

  envSensor.attachCallback(newDataCallback);
  bmeReady = true;
  return true;
}

// Drive BSEC. Call frequently from loop(); it samples when due (~3 s in LP)
// and fires newDataCallback. Returns false only on a real (negative) error.
void bme680Run() {
  if (!bmeReady) return;

  // run() returns false on any non-OK status — including benign positive
  // warnings — so only react to a negative status.
  if (!envSensor.run() &&
      (envSensor.status < BSEC_OK || envSensor.sensor.status < BME68X_OK)) {
    checkBsecStatus();
    delay(1000);
  }
}
