/*
 * Sensor-board identity diagnostics
 *
 * Standalone sketch: it does not include or depend on the Smart Home Sensor
 * project. Flash it to one board at a time and record the complete serial
 * report. It never joins Wi-Fi, writes NVS, or prints credentials.
 *
 * Tested target: ESP32-C6 with Arduino-ESP32 core 3.x.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <esp_chip_info.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <mbedtls/md.h>

static constexpr char DEVICE_ID_PREFIX[] = "shs";
static constexpr char DEVICE_ID_DOMAIN[] = "shs-device-id";

static void printMac(const char *label, const uint8_t mac[6]) {
  Serial.printf("%s=%02x:%02x:%02x:%02x:%02x:%02x\n",
                label, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void printMacOrUnavailable(const char *label, esp_mac_type_t type) {
  uint8_t mac[6] = {};
  esp_err_t result = esp_read_mac(mac, type);
  if (result == ESP_OK) {
    printMac(label, mac);
  } else {
    Serial.printf("%s=<unavailable:%s>\n", label, esp_err_to_name(result));
  }
}

static void printEfuseMac(uint64_t efuseMac) {
  // This is the byte order used by the production firmware's hash input:
  // macToBytes() takes the least-significant byte first.
  uint8_t hashInput[6];
  for (int i = 0; i < 6; ++i) hashInput[i] = (uint8_t)(efuseMac >> (8 * i));
  printMac("efuse_mac_hash_input", hashInput);

  // This is the conventional human-readable representation of the same value.
  Serial.printf("efuse_mac_u64=0x%012llx\n",
                (unsigned long long)(efuseMac & 0xFFFFFFFFFFFFULL));
  Serial.printf("efuse_mac_human=%02x:%02x:%02x:%02x:%02x:%02x\n",
                hashInput[5], hashInput[4], hashInput[3],
                hashInput[2], hashInput[1], hashInput[0]);
}

static bool deriveProductionDeviceId(uint64_t efuseMac, char *out, size_t outLen) {
  if (outLen < sizeof(DEVICE_ID_PREFIX) + 1 + 8) return false;

  uint8_t mac[6];
  for (int i = 0; i < 6; ++i) mac[i] = (uint8_t)(efuseMac >> (8 * i));

  uint8_t digest[32] = {};
  const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_context_t context;
  mbedtls_md_init(&context);
  bool ok = md != nullptr &&
            mbedtls_md_setup(&context, md, 0) == 0 &&
            mbedtls_md_starts(&context) == 0 &&
            mbedtls_md_update(&context,
                              (const uint8_t *)DEVICE_ID_DOMAIN,
                              strlen(DEVICE_ID_DOMAIN)) == 0 &&
            mbedtls_md_update(&context, mac, sizeof(mac)) == 0 &&
            mbedtls_md_finish(&context, digest) == 0;
  mbedtls_md_free(&context);

  if (!ok) return false;
  snprintf(out, outLen, "%s-%02x%02x%02x%02x", DEVICE_ID_PREFIX,
           digest[0], digest[1], digest[2], digest[3]);
  return true;
}

static void printResetReason() {
  Serial.printf("reset_reason_cpu0=%d\n", (int)esp_reset_reason());
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3 || \
    CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 || \
    CONFIG_IDF_TARGET_ESP32H2
  Serial.printf("reset_reason_cpu1=%d\n", (int)esp_reset_reason_cpu(1));
#endif
}

static void printReport() {
  const uint64_t efuseMac = ESP.getEfuseMac();
  esp_chip_info_t chipInfo;
  esp_chip_info(&chipInfo);

  Serial.println();
  Serial.println("=== SENSOR_BOARD_IDENTITY_REPORT_BEGIN ===");
  Serial.printf("report_version=1\n");
  Serial.printf("millis=%lu\n", (unsigned long)millis());
  Serial.printf("arduino_core=%s\n", ESP.getSdkVersion());
  Serial.printf("chip_model=%s\n", ESP.getChipModel());
  Serial.printf("chip_revision=%d\n", ESP.getChipRevision());
  Serial.printf("chip_cores=%d\n", chipInfo.cores);
  Serial.printf("cpu_mhz=%u\n", ESP.getCpuFreqMHz());
  Serial.printf("flash_size=%lu\n", (unsigned long)ESP.getFlashChipSize());
  Serial.printf("free_heap=%lu\n", (unsigned long)ESP.getFreeHeap());
  printResetReason();

  printEfuseMac(efuseMac);
  printMacOrUnavailable("wifi_sta_mac", ESP_MAC_WIFI_STA);
  printMacOrUnavailable("wifi_ap_mac", ESP_MAC_WIFI_SOFTAP);
  printMacOrUnavailable("bt_mac", ESP_MAC_BT);
  printMacOrUnavailable("ieee802154_mac", ESP_MAC_IEEE802154);

  // WiFi.macAddress() is shown separately because it is the value exposed by
  // the Arduino API used by the production diagnostics. No connection is made.
  WiFi.mode(WIFI_STA);
  Serial.printf("arduino_wifi_sta_mac=%s\n", WiFi.macAddress().c_str());
  WiFi.mode(WIFI_OFF);

  char deviceId[32] = {};
  if (deriveProductionDeviceId(efuseMac, deviceId, sizeof(deviceId))) {
    Serial.printf("production_derived_device_id=%s\n", deviceId);
    Serial.printf("production_identity_hash=sha256(domain||efuse_mac_hash_input)\n");
  } else {
    Serial.printf("production_derived_device_id=<hash-failed>\n");
    Serial.printf("production_identity_hash=<failed>\n");
  }
  Serial.println("=== SENSOR_BOARD_IDENTITY_REPORT_END ===");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(1200); // allow USB CDC and the serial monitor to attach
  Serial.println("sensor-board identity diagnostics starting");
  printReport();
  Serial.println("Leave this board connected for 5 seconds, then record the next report if needed.");
}

void loop() {
  static unsigned long nextReport = 0;
  if (millis() >= nextReport) {
    nextReport = millis() + 5000;
    printReport();
  }
  delay(20);
}
