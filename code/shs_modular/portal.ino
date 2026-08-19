// ============================================================================
//  portal.ino — commissioning / setup portal.
//
//  Opened when no Wi-Fi credentials are saved, when the saved network cannot be
//  reached on a fresh boot, or on a double reset (see resetdetect.ino). Serves:
//
//    /            WiFiManager home — Wi-Fi setup, settings, device identity
//    /live        live BME680 readings, refreshed every 5 s
//    /sensors.json  the data behind /live
//    /sendtest    publish one reading to the configured backend, report result
//    /finish      leave the portal and start normal operation
//    /resetwifi   forget the saved network only
//    /clearcal    discard the BSEC calibration only
//    /factory     settings back to defaults + forget the network
//
//  The portal runs non-blocking so we can enforce our own timeout, keep driving
//  BSEC while it is open (its 3 s cadence must not stall, or the calibration
//  clock effectively stops), and let a "keep awake" toggle defeat the timeout
//  for debugging.
// ============================================================================
#include "config.h"

#if USE_NETWORK
#include <WiFiManager.h>   // tzapu

static const uint32_t PORTAL_TIMEOUT_MS = 10UL * 60UL * 1000UL;  // 10 minutes

static WiFiManager wm;
static volatile bool keepAwake  = false;
static volatile bool portalDone = false;

// Custom parameters. Allocated once — the portal runs a single time per boot.
static WiFiManagerParameter *p_deviceName;
static WiFiManagerParameter *p_pubMin;
static WiFiManagerParameter *p_tempOff;
#if USE_SENSORBOARD
static WiFiManagerParameter *p_apiUrl;
#if !SHS_HAS_WORKSHOP_KEY
static WiFiManagerParameter *p_project;
static WiFiManagerParameter *p_apiKey;
#endif
#if !SHS_DERIVED_WRITE_KEY
static WiFiManagerParameter *p_writeKey;
#endif
#endif
#if USE_MQTT
static WiFiManagerParameter *p_mqttHost;
static WiFiManagerParameter *p_mqttPort;
static WiFiManagerParameter *p_mqttUser;
static WiFiManagerParameter *p_mqttPass;
static WiFiManagerParameter *p_mqttPrefix;
static WiFiManagerParameter *p_haDisc;
static WiFiManagerParameter *p_mqttMode;
static WiFiManagerParameter *p_mqttTls;
#endif

static WiFiManagerParameter *makeCheckbox(const char *id, const char *label, bool checked) {
  // WiFiManager checkbox idiom: value "T", pre-check via the "checked"
  // attribute. When submitted unchecked the browser omits the field entirely,
  // so getValue() comes back empty — that absence is the "false".
  const char *custom = checked ? "type=\"checkbox\" checked" : "type=\"checkbox\"";
  return new WiFiManagerParameter(id, label, "T", 2, custom, WFM_LABEL_AFTER);
}

static bool checkboxChecked(WiFiManagerParameter *p) {
  return strncmp(p->getValue(), "T", 1) == 0;
}

// --- Saving ------------------------------------------------------------------

static void saveParamsCallback() {
  if (p_deviceName->getValue()[0]) {
    strlcpy(settings.deviceName, p_deviceName->getValue(), sizeof(settings.deviceName));
  }
  uint32_t pub = strtoul(p_pubMin->getValue(), nullptr, 10);
  if (pub >= MIN_PUBLISH_INTERVAL_MIN && pub <= MAX_PUBLISH_INTERVAL_MIN) {
    settings.publishIntervalMin = pub;
  }
  settings.tempOffsetC = atof(p_tempOff->getValue());

#if USE_SENSORBOARD
  if (p_apiUrl->getValue()[0]) {
    strlcpy(settings.apiUrl, p_apiUrl->getValue(), sizeof(settings.apiUrl));
  }
#if !SHS_HAS_WORKSHOP_KEY
  strlcpy(settings.project, p_project->getValue(), sizeof(settings.project));
  strlcpy(settings.apiKey, p_apiKey->getValue(), sizeof(settings.apiKey));
#endif
#if !SHS_DERIVED_WRITE_KEY
  // Adopting a device ID claimed by an earlier build: pasting the key that owns
  // it is the only way back in, since the server has no recovery path. Blank
  // means "keep the current one" — otherwise saving the form would wipe it.
  if (p_writeKey->getValue()[0]) {
    strlcpy(settings.writeKey, p_writeKey->getValue(), sizeof(settings.writeKey));
  }
#endif
#endif

#if USE_MQTT
  strlcpy(settings.mqttHost,     p_mqttHost->getValue(),   sizeof(settings.mqttHost));
  strlcpy(settings.mqttUser,     p_mqttUser->getValue(),   sizeof(settings.mqttUser));
  strlcpy(settings.mqttPass,     p_mqttPass->getValue(),   sizeof(settings.mqttPass));
  if (p_mqttPrefix->getValue()[0]) {
    strlcpy(settings.mqttPrefix, p_mqttPrefix->getValue(), sizeof(settings.mqttPrefix));
  }
  if (p_haDisc->getValue()[0]) {
    strlcpy(settings.haDiscPrefix, p_haDisc->getValue(),   sizeof(settings.haDiscPrefix));
  }
  uint32_t port = strtoul(p_mqttPort->getValue(), nullptr, 10);
  if (port > 0 && port < 65536) settings.mqttPort = (uint16_t)port;
  uint32_t mode = strtoul(p_mqttMode->getValue(), nullptr, 10);
  if (mode <= MQTT_MODE_BOTH) settings.mqttMode = (uint8_t)mode;
  settings.mqttTls = checkboxChecked(p_mqttTls);
#endif

  saveSettings();
  Serial.println("Portal: settings saved");
}

// --- Live readings -----------------------------------------------------------

static void appendFloat(String &j, const char *key, float v, uint8_t decimals) {
  j += "\"";
  j += key;
  j += "\":";
  j += isValidFloat(v) ? String((double)v, (unsigned int)decimals) : "null";
  j += ",";
}

static String buildSensorJson() {
  SensorPacket d = sensorLatest();

  String j = "{";
  appendFloat(j, "iaq",         d.iaq,         0);
  appendFloat(j, "co2",         d.co2,         0);
  appendFloat(j, "voc",         d.voc,         2);
  appendFloat(j, "temperature", d.temperature, 2);
  appendFloat(j, "humidity",    d.humidity,    1);
  appendFloat(j, "pressure",    d.pressure,    1);
  j += "\"iaq_accuracy\":" + String(d.iaqAccuracy) + ",";

  bool connected = WiFi.status() == WL_CONNECTED;
  j += "\"wifi_connected\":" + String(connected ? "true" : "false") + ",";
  j += "\"ssid\":\"" + WiFi.SSID() + "\",";
  j += "\"ip\":\"" + (connected ? WiFi.localIP().toString() : String("")) + "\",";
  j += "\"rssi\":" + String(connected ? WiFi.RSSI() : 0) + ",";
  j += "\"keep_awake\":" + String(keepAwake ? "true" : "false");
  j += "}";
  return j;
}

static const char LIVE_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Live Readings</title>
<style>
 body{font-family:system-ui,sans-serif;margin:0;background:#0f1115;color:#e8eaed}
 header{padding:16px 20px;background:#1a1d24;font-size:1.2rem;font-weight:600;
        display:flex;align-items:center;gap:12px;flex-wrap:wrap}
 .grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:12px;padding:16px 20px}
 .card{background:#1a1d24;border:1px solid #2a2e37;border-radius:10px;padding:14px}
 .label{font-size:.72rem;text-transform:uppercase;letter-spacing:.05em;color:#9aa0aa}
 .value{font-size:1.5rem;font-weight:600;margin-top:6px}
 .unit{font-size:.85rem;color:#9aa0aa;margin-left:3px}
 .bad{color:#ff6b6b}
 .foot{padding:12px 20px;color:#9aa0aa;font-size:.85rem;display:flex;
       align-items:center;gap:10px;flex-wrap:wrap}
 a{color:#7aa2ff}
 .send{padding:4px 20px 16px;display:flex;align-items:center;gap:12px;flex-wrap:wrap}
 button{background:#2f855a;color:#fff;border:0;border-radius:8px;padding:10px 16px;
        font-size:.95rem;font-weight:600;cursor:pointer}
 button:disabled{opacity:.6;cursor:default}
 #sendresult{font-weight:600}
 #sendresult.ok{color:#4ade80}
 #sendresult.err{color:#ff6b6b}
 .wbadge{font-size:.75rem;font-weight:600;padding:4px 12px;border-radius:20px}
 .wbadge.ok{background:#14361f;color:#4ade80}
 .wbadge.err{background:#3a1414;color:#ff6b6b}
 .note{margin:0 20px 16px;padding:12px 14px;border-radius:10px;font-size:.85rem;
       background:#2a2410;border:1px solid #5c4d1a;color:#e8d9a8}
</style></head><body>
<header>Live Readings <span id="wifi" class="wbadge err">WiFi: …</span></header>
<div class="grid" id="grid"></div>
<div class="note" id="acc"></div>
<div class="send">
 <button id="sendbtn">Send a test reading</button>
 <span id="sendresult"></span>
</div>
<div class="foot">
 <label><input type="checkbox" id="ka"> Keep portal open (disables the 10 min timeout)</label>
 <span id="status">refreshing every 5 s…</span>
 <a href="/">&larr; Back to setup</a>
 <form action="/finish" method="post" style="margin:0"><button>Finish setup</button></form>
</div>
<script>
const cards=[["iaq","IAQ","",0],["co2","CO2 equiv.","ppm",0],["voc","VOC equiv.","ppm",2],
 ["temperature","Temperature","°C",2],["humidity","Humidity","%",1],
 ["pressure","Pressure","hPa",1],["rssi","WiFi RSSI","dBm",0]];
// BSEC self-calibrates against the cleanest air it has seen. Until it reaches
// accuracy 3 the IAQ number is a placeholder, and students otherwise read the
// first value they see as gospel.
const ACC=["0 — stabilizing. IAQ is not meaningful yet; this is normal for the first minutes.",
 "1 — calibrating against a short history. Treat IAQ as indicative only.",
 "2 — calibrating. IAQ is roughly right but still drifting.",
 "3 — fully calibrated. IAQ is trustworthy."];
function render(d){
 let h="";
 for(const [k,lab,unit,dec] of cards){
  if(!(k in d))continue;
  const v=d[k];
  const disp=(v===null||v===undefined)?"—":(typeof v==="number"?v.toFixed(dec):v);
  const bad=(v===null||v===undefined)?" bad":"";
  h+=`<div class="card"><div class="label">${lab}</div><div class="value${bad}">${disp}<span class="unit">${unit}</span></div></div>`;
 }
 document.getElementById("grid").innerHTML=h;
 document.getElementById("acc").textContent="IAQ accuracy "+(ACC[d.iaq_accuracy]||d.iaq_accuracy);
 document.getElementById("ka").checked=!!d.keep_awake;
 const w=document.getElementById("wifi");
 if(d.wifi_connected){w.className="wbadge ok";w.textContent="WiFi: "+(d.ssid||"connected")+" · "+d.ip;}
 else{w.className="wbadge err";w.textContent="WiFi: not connected";}
}
async function tick(){
 try{const r=await fetch("/sensors.json",{cache:"no-store"});render(await r.json());
  document.getElementById("status").textContent="updated "+new Date().toLocaleTimeString();
 }catch(e){document.getElementById("status").textContent="read error";}
}
document.getElementById("ka").addEventListener("change",async e=>{
 await fetch("/keepawake?v="+(e.target.checked?1:0));
});
const b=document.getElementById("sendbtn"),res=document.getElementById("sendresult");
b.addEventListener("click",async()=>{
 b.disabled=true;res.className="";res.textContent="Sending…";
 try{
  const d=await (await fetch("/sendtest",{method:"POST"})).json();
  res.className=d.ok?"ok":"err";
  res.textContent=(d.ok?"✓ ":"✗ ")+d.msg;
 }catch(e){res.className="err";res.textContent="✗ Request failed";}
 b.disabled=false;
});
tick();setInterval(tick,5000);
</script></body></html>
)HTML";

// Injected into the <head> of every WiFiManager page. On the "credentials
// saved" page it replaces the default "reconnect to the AP to try again"
// message with useful links: the portal stays up in AP+STA mode, so the
// student can go straight back instead of hunting for the AP again.
static const char SAVED_PAGE_HEAD[] =
    "<script>addEventListener('load',function(){"
    "if(location.pathname.indexOf('wifisave')<0)return;"
    "var m=document.querySelector('.msg');if(!m)return;"
    "m.innerHTML=\"Credentials saved. The device is connecting to your network."
    "<br><br><a href='/'>Return to setup</a>"
    "<br><br><a href='/live'>Check the connection &amp; live readings</a>\";"
    "});</script>";

// --- Backend test ------------------------------------------------------------

// Publish one reading now and describe the outcome in the student's terms.
static String sendTestJson() {
  SensorPacket d = sensorLatest();
  bool ok = false;
  String msg;

  if (WiFi.status() != WL_CONNECTED) {
    msg = "Not connected to WiFi — set up the network first";
  } else {
#if USE_SENSORBOARD
    int code = sensorboardSend(d);
    ok  = (code == 200 || code == 201);
    if (ok) msg = String(code) + " — reading stored, check the dashboard";
    else if (code == 403) msg = "403 — this device ID belongs to a different write key";
    else if (code == 401) msg = "401 — the API key was missing or rejected";
    else if (code == 429) msg = "429 — rate limited, try again in a moment";
    else if (code < 0)    msg = "could not reach " + String(settings.apiUrl);
    else                  msg = "server returned " + String(code);
#elif USE_MQTT
    if (settings.mqttHost[0] == '\0') {
      msg = "No broker address configured yet";
    } else {
      ok  = mqttSendTest(d) == 0;
      msg = ok ? "published to " + String(settings.mqttHost)
               : "could not connect to the broker at " + String(settings.mqttHost);
    }
#else
    msg = "This build has no backend configured";
#endif
  }

  return String("{\"ok\":") + (ok ? "true" : "false") +
         ",\"msg\":\"" + msg + "\"}";
}

// --- Routes ------------------------------------------------------------------

static void sendSimplePage(const char *title, const char *body) {
  String html = "<!doctype html><meta charset=utf-8>"
                "<meta name=viewport content='width=device-width,initial-scale=1'>"
                "<body style='font-family:system-ui;padding:24px;max-width:34rem'><h3>";
  html += title;
  html += "</h3><p>";
  html += body;
  html += "</p>";
  wm.server->send(200, "text/html", html);
}

static void bindCustomRoutes() {
  wm.server->on("/sensors.json", []() {
    wm.server->sendHeader("Cache-Control", "no-store");
    wm.server->send(200, "application/json", buildSensorJson());
  });
  wm.server->on("/live", []() {
    wm.server->send_P(200, "text/html", LIVE_PAGE);
  });
  wm.server->on("/keepawake", []() {
    keepAwake = wm.server->arg("v") == "1";
    Serial.println(keepAwake ? "Portal: keep-awake ON" : "Portal: keep-awake OFF");
    wm.server->send(200, "application/json",
                    String("{\"keep_awake\":") + (keepAwake ? "true" : "false") + "}");
  });
  wm.server->on("/sendtest", HTTP_POST, []() {
    wm.server->send(200, "application/json", sendTestJson());
  });
  wm.server->on("/finish", HTTP_POST, []() {
    sendSimplePage("Setup finished",
                   "The device is starting normal operation. You can close this page.");
    portalDone = true;
  });

  // --- The three resets. Each is named for what it destroys; none of them
  // touches the device identity, which is derived from the chip and cannot be
  // lost (see settings.ino).
  wm.server->on("/resetwifi", HTTP_POST, []() {
    sendSimplePage("Wi-Fi forgotten",
                   "Settings and calibration are untouched. Rebooting into setup…");
    clearWiFiCredentials();
    delay(800);
    ESP.restart();
  });
  wm.server->on("/clearcal", HTTP_POST, []() {
    clearBsecState();
    sendSimplePage("IAQ calibration cleared",
                   "The gas sensor starts learning from scratch: hours to reach "
                   "accuracy 3, up to four days to fully converge. Rebooting…");
    delay(800);
    ESP.restart();
  });
  wm.server->on("/factory", HTTP_POST, []() {
    resetSettingsToDefaults();
    saveSettings();
    clearWiFiCredentials();
    sendSimplePage("Factory reset done",
                   "Settings are back to defaults and the network is forgotten. "
                   "The device ID, its write key and the IAQ calibration are kept. "
                   "Rebooting…");
    delay(800);
    ESP.restart();
  });
}

// --- Menu --------------------------------------------------------------------

static String buildMenuHtml() {
  String h;

  // The device ID is what a student looks for on a shared dashboard, so it gets
  // the most prominent spot on the page.
  h += "<div style='margin:16px 0;padding:14px;border:1px solid #ccc;border-radius:8px;text-align:center'>"
       "<div style='font-size:.75rem;text-transform:uppercase;letter-spacing:.05em;color:#666'>Device ID</div>"
       "<div style='font-size:1.5rem;font-weight:700;font-family:monospace;margin-top:4px;word-break:break-all'>"
       + String(settings.deviceId) + "</div>";
#if USE_SENSORBOARD
  h += "<div style='font-size:.75rem;color:#666;margin-top:6px;word-break:break-all'>"
       DASHBOARD_URL_PREFIX + String(settings.deviceId) + "</div>";
#endif
  h += "</div>";

#if USE_SENSORBOARD
  h += "<div style='margin:16px 0;padding:12px;border:1px solid #f0d98c;background:#fff8e6;border-radius:8px'>"
       "<div style='font-size:.75rem;text-transform:uppercase;color:#7a6520'>Write key — note it down</div>"
       "<div style='font-family:monospace;word-break:break-all;margin-top:4px'>"
       + String(settings.writeKey) + "</div>"
       "<div style='font-size:.75rem;color:#7a6520;margin-top:6px'>";
#if SHS_DERIVED_WRITE_KEY
  // Reproducible from the board — but only by the image holding the salt. Once
  // the workshop firmware is withdrawn, this page is the last copy of it.
  h += "This key proves you own the device ID. This build recreates it from the "
       "board itself, so a reset cannot lose it — but only this firmware can "
       "recreate it. Write it down if you want to keep the same device ID after "
       "the workshop image is withdrawn.";
#else
  h += "This key proves you own the device ID. It cannot be recovered — if it is "
       "lost, the ID stays claimed until it expires (48 h after the last reading).";
#endif
  h += "</div></div>";
#endif

  h += "<form action='/live' method='get' style='margin:16px 0'>"
       "<button class='D'>Live readings &amp; connection test</button></form>"
       "<form action='/finish' method='post' style='margin:16px 0'>"
       "<button style='background:#2f855a'>Finish setup &amp; start monitoring</button></form>"
       "<hr><p style='font-size:.8rem;color:#666'>Each reset below affects only what it names.</p>"
       "<form action='/resetwifi' method='post' style='margin:10px 0'>"
       "<button>Forget Wi-Fi network</button></form>"
       "<form action='/clearcal' method='post' style='margin:10px 0' "
       "onsubmit='return confirm(\"Discard the IAQ calibration? It takes hours to rebuild.\")'>"
       "<button>Clear IAQ calibration</button></form>"
       "<form action='/factory' method='post' style='margin:10px 0' "
       "onsubmit='return confirm(\"Reset all settings to defaults and forget Wi-Fi?\")'>"
       "<button style='background:#a12222'>Factory reset (settings + Wi-Fi)</button></form>";
  return h;
}

static void buildParameters() {
  static char pubBuf[12], tempBuf[12];
  snprintf(pubBuf,  sizeof(pubBuf),  "%u", settings.publishIntervalMin);
  snprintf(tempBuf, sizeof(tempBuf), "%.1f", settings.tempOffsetC);

  p_deviceName = new WiFiManagerParameter("dname", "Device name (shown on the dashboard)",
                                          settings.deviceName, sizeof(settings.deviceName) - 1);
  p_pubMin     = new WiFiManagerParameter("pub", "Publish interval (minutes)", pubBuf, 11);
  p_tempOff    = new WiFiManagerParameter("toff", "Temperature offset (°C)", tempBuf, 11);
  wm.addParameter(p_deviceName);
  wm.addParameter(p_pubMin);
  wm.addParameter(p_tempOff);

#if USE_SENSORBOARD
  p_apiUrl  = new WiFiManagerParameter("aurl", "API URL",
                                       settings.apiUrl, sizeof(settings.apiUrl) - 1);
  wm.addParameter(p_apiUrl);
#if !SHS_HAS_WORKSHOP_KEY
  p_project = new WiFiManagerParameter("proj", "Project (dashboard group)",
                                       settings.project, sizeof(settings.project) - 1);
  p_apiKey  = new WiFiManagerParameter("akey", "API key (optional)",
                                       settings.apiKey, sizeof(settings.apiKey) - 1);
  wm.addParameter(p_project);
  wm.addParameter(p_apiKey);
#endif
#if !SHS_DERIVED_WRITE_KEY
  p_writeKey = new WiFiManagerParameter("wkey",
      "Write key — blank keeps the current one; paste one to adopt an existing device ID",
      "", sizeof(settings.writeKey) - 1);
  wm.addParameter(p_writeKey);
#endif
#endif

#if USE_MQTT
  static char portBuf[8], modeBuf[4];
  snprintf(portBuf, sizeof(portBuf), "%u", settings.mqttPort);
  snprintf(modeBuf, sizeof(modeBuf), "%u", settings.mqttMode);

  p_mqttHost   = new WiFiManagerParameter("mhost", "MQTT broker host",
                                          settings.mqttHost, sizeof(settings.mqttHost) - 1);
  p_mqttPort   = new WiFiManagerParameter("mport", "MQTT port", portBuf, 7);
  p_mqttUser   = new WiFiManagerParameter("muser", "MQTT username",
                                          settings.mqttUser, sizeof(settings.mqttUser) - 1);
  p_mqttPass   = new WiFiManagerParameter("mpass", "MQTT password",
                                          settings.mqttPass, sizeof(settings.mqttPass) - 1);
  p_mqttPrefix = new WiFiManagerParameter("mpre", "Topic prefix",
                                          settings.mqttPrefix, sizeof(settings.mqttPrefix) - 1);
  p_haDisc     = new WiFiManagerParameter("mdisc", "HA discovery prefix",
                                          settings.haDiscPrefix, sizeof(settings.haDiscPrefix) - 1);
  p_mqttMode   = new WiFiManagerParameter("mmode",
      "Mode: 0 = HA auto-discovery, 1 = plain per-metric topics, 2 = both", modeBuf, 3);
  p_mqttTls    = makeCheckbox("mtls", "Use TLS (MQTTS)", settings.mqttTls);

  wm.addParameter(p_mqttHost);
  wm.addParameter(p_mqttPort);
  wm.addParameter(p_mqttUser);
  wm.addParameter(p_mqttPass);
  wm.addParameter(p_mqttPrefix);
  wm.addParameter(p_haDisc);
  wm.addParameter(p_mqttMode);
  wm.addParameter(p_mqttTls);
#endif
}

// ---- Public API -------------------------------------------------------------

void runCommissioningPortal() {
  buildParameters();

  wm.setSaveParamsCallback(saveParamsCallback);
  wm.setWebServerCallback(bindCustomRoutes);
  wm.setCustomHeadElement(SAVED_PAGE_HEAD);
  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(0);       // we enforce our own timeout below
  wm.setBreakAfterConfig(true);
  // Don't let WiFiManager tear the portal down itself once it connects: it
  // frees `server`, and since we also shut down below, the second shutdown
  // would dereference the freed pointer. We own the portal lifecycle.
  wm.setDisableConfigPortal(false);

  std::vector<const char *> menu = {"wifi", "param", "custom", "sep", "info", "restart", "exit"};
  wm.setMenu(menu);
  // WiFiManager stores the pointer rather than copying, so this must outlive
  // the portal — hence static, not a local.
  static String menuHtml;
  menuHtml = buildMenuHtml();
  wm.setCustomMenuHTML(menuHtml.c_str());

  String apName = String(settings.deviceName) + "-Setup";
  Serial.println("Portal: starting AP '" + apName + "'");
  displayPortal(apName.c_str(), settings.deviceId);
  wm.startConfigPortal(apName.c_str());

  // Stay open (AP+STA) after Wi-Fi connects rather than exiting immediately, so
  // the student can watch live readings and run the send test over the real
  // connection. Exit on an explicit Finish, or on timeout.
  uint32_t start = millis();
  while (true) {
    wm.process();
    bme680Run();          // keep BSEC sampling while the portal is open

    if (portalDone) {
      Serial.println("Portal: finished by user");
      break;
    }
    if (!keepAwake && (millis() - start > PORTAL_TIMEOUT_MS)) {
      Serial.println("Portal: timeout reached");
      break;
    }
    delay(5);
  }

  if (wm.getConfigPortalActive()) wm.stopConfigPortal();
}

#else  // ---- no networking in this variant ---------------------------------

void runCommissioningPortal() {}

#endif
