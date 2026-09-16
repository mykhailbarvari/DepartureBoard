#include "portal.h"
#include "portal_page.h"
#include "portal_logo.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <settings.h>
#include <api.h>
#include <logic.h>
#include <string.h>

// Arduino-corens inbyggda WebServer räcker för en ren konfigurationsportal och
// slipper ESPAsyncWebServer som beroende. Den blockerar, så den körs i en egen
// låg-prioriterad task och konkurrerar aldrig med DisplayTask.
static WebServer  server(80);
static DNSServer  dns;

static bool s_apMode = false;
static bool s_serverStarted = false;
static char s_apSsid[24] = {0};

// Kort SSID med flit: WiFi-join-QR:en blir "WIFI:T:nopass;S:<ssid>;;" och
// måste rymmas i QR version 2 (32 byte) för att kunna ritas med 2 px per
// modul på 64 px höjd.
static void buildApSsid(void) {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(s_apSsid, sizeof(s_apSsid), "DepBoard-%02X%02X", mac[4], mac[5]);
}

bool        portal_isAp(void)   { return s_apMode; }
const char* portal_apSsid(void) { return s_apSsid; }

// ------------------------------------------------------------------ HANDLERS

static void sendJson(int code, const JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json; charset=utf-8", out);
}

static void handleRoot(void) {
  server.send_P(200, "text/html; charset=utf-8", PORTAL_PAGE);
}

// Logotypen serveras som egen resurs i stallet for att badda in den i HTML:en.
// Det haller sidan liten och later webblasaren cacha logotypen for sig.
static void handleLogo(void) {
  server.sendHeader("Cache-Control", "public, max-age=604800");
  server.send_P(200, "image/svg+xml", PORTAL_LOGO_SVG);
}

static void handleGetSettings(void) {
  JsonDocument d;
  d["brightness"]    = g_settings.brightness;
  d["colourway"]     = g_settings.colourway;
  d["walkMinutes"]   = g_settings.walkMinutes;
  d["directionCode"] = g_settings.directionCode;
  d["siteId"]        = g_settings.siteId;
  d["siteName"]      = g_settings.siteName;
  d["transportMask"] = g_settings.transportMask;

  // Temana kommer från enheten i stället för att dupliceras i sidan. En kopia
  // av RGB565-värdena skulle glida isär förr eller senare.
  JsonArray themes = d["themes"].to<JsonArray>();
  for (int i = 0; i < kThemeCount; i++) {
    JsonObject o = themes.add<JsonObject>();
    o["v"] = kThemes[i].colour;
    o["n"] = kThemes[i].name;
  }

  sendJson(200, d);
}

static void handlePostSettings(void) {
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain"))) {
    server.send(400, "text/plain", "bad json");
    return;
  }

  // Klampa allt som kommer utifrån — portalen är öppen för alla på nätet.
  const uint32_t site = d["siteId"] | g_settings.siteId;
  if (site > 0) g_settings.siteId = site;

  uint8_t dir = d["directionCode"] | g_settings.directionCode;
  g_settings.directionCode = (dir > 2) ? 0 : dir;

  uint16_t walk = d["walkMinutes"] | g_settings.walkMinutes;
  g_settings.walkMinutes = (walk > 60) ? 60 : (uint8_t)walk;

  uint16_t bri = d["brightness"] | g_settings.brightness;
  g_settings.brightness = (bri > 255) ? 255 : (uint8_t)bri;

  g_settings.colourway     = d["colourway"]     | g_settings.colourway;
  g_settings.transportMask = d["transportMask"] | g_settings.transportMask;

  const char* name = d["siteName"] | "";
  if (name[0]) {
    strncpy(g_settings.siteName, name, sizeof(g_settings.siteName) - 1);
    g_settings.siteName[sizeof(g_settings.siteName) - 1] = 0;
  }

  settings_save();
  requestFetch();   // panelen ska visa den nya hållplatsen direkt

  server.send(200, "application/json; charset=utf-8", "{\"ok\":true}");
}

static void handleWifiScan(void) {
  const int n = WiFi.scanNetworks();

  JsonDocument d;
  JsonArray arr = d.to<JsonArray>();
  for (int i = 0; i < n && i < 20; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
  }
  WiFi.scanDelete();

  sendJson(200, d);
}

static void handlePostWifi(void) {
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain"))) {
    server.send(400, "text/plain", "bad json");
    return;
  }

  const char* ssid = d["ssid"] | "";
  const char* pass = d["pass"] | "";
  if (!ssid[0]) {
    server.send(400, "text/plain", "no ssid");
    return;
  }

  strncpy(g_settings.wifiSsid, ssid, sizeof(g_settings.wifiSsid) - 1);
  g_settings.wifiSsid[sizeof(g_settings.wifiSsid) - 1] = 0;
  strncpy(g_settings.wifiPass, pass, sizeof(g_settings.wifiPass) - 1);
  g_settings.wifiPass[sizeof(g_settings.wifiPass) - 1] = 0;
  settings_save();

  // Svara FÖRE anslutningsförsöket: i AP-läge rycks telefonens anslutning
  // undan när vi byter läge, och då kommer svaret aldrig fram.
  server.send(200, "application/json; charset=utf-8", "{\"ok\":true}");
  delay(200);

  if (s_apMode) {
    dns.stop();
    WiFi.softAPdisconnect(true);
    s_apMode = false;
    WiFi.mode(WIFI_STA);

    // Sockeln hörde till AP-gränssnittet som just försvann. Låt
    // startServerOnce() binda om när STA-anslutningen är uppe.
    server.stop();
    s_serverStarted = false;
  }
  WiFi.begin(g_settings.wifiSsid, g_settings.wifiPass);
  requestFetch();
}

static void handleStatus(void) {
  JsonDocument d;
  d["ap"]        = s_apMode;
  d["connected"] = (WiFi.status() == WL_CONNECTED);
  d["ssid"]      = s_apMode ? s_apSsid : WiFi.SSID().c_str();
  d["ip"]        = s_apMode ? WiFi.softAPIP().toString()
                            : WiFi.localIP().toString();
  sendJson(200, d);
}

// Captive portal: allt okänt leds till startsidan, så telefonens
// "logga in på nätverket"-ruta öppnar konfigurationssidan av sig själv.
static void handleNotFound(void) {
  if (s_apMode) {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
    return;
  }
  server.send(404, "text/plain", "not found");
}

// -------------------------------------------------------------------- UPPSTART

// server.begin() skapar en lyssnande socket och assertar inne i lwIP
// ("tcpip_send_msg_wait_sem ... Invalid mbox") om den anropas innan
// nätverksstacken finns. I STA-läge rörs radion först när ApiTask kör sin
// första hämtning, alltså långt efter setup(). Därför startas servern lazy,
// när det faktiskt finns ett gränssnitt att lyssna på.
static void startServerOnce(void) {
  if (s_serverStarted) return;
  if (!s_apMode && WiFi.status() != WL_CONNECTED) return;

  server.begin();
  s_serverStarted = true;
}

void portal_begin(bool forceAp) {
  if (forceAp || !settings_hasWifi()) {
    buildApSsid();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(s_apSsid);           // öppet nät: håller join-QR:en kort
    dns.start(53, "*", WiFi.softAPIP());
    s_apMode = true;
  }

  server.on("/",              HTTP_GET,  handleRoot);
  server.on("/logo.svg",      HTTP_GET,  handleLogo);
  server.on("/api/settings",  HTTP_GET,  handleGetSettings);
  server.on("/api/settings",  HTTP_POST, handlePostSettings);
  server.on("/api/wifi/scan", HTTP_GET,  handleWifiScan);
  server.on("/api/wifi",      HTTP_POST, handlePostWifi);
  server.on("/api/status",    HTTP_GET,  handleStatus);
  server.onNotFound(handleNotFound);

  // Ingen server.begin() här — se startServerOnce().
}

void portal_task(void* pv) {
  (void)pv;
  for (;;) {
    startServerOnce();

    if (s_serverStarted) {
      if (s_apMode) dns.processNextRequest();
      server.handleClient();
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
