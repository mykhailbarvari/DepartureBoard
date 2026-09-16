#include "portal.h"
#include "portal_page.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <settings.h>
#include <api.h>
#include <string.h>

// Arduino-corens inbyggda WebServer räcker för en ren konfigurationsportal och
// slipper ESPAsyncWebServer som beroende. Den blockerar, så den körs i en egen
// låg-prioriterad task och konkurrerar aldrig med DisplayTask.
static WebServer  server(80);
static DNSServer  dns;

static bool s_apMode = false;
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
  server.send(code, "application/json", out);
}

static void handleRoot(void) {
  server.send_P(200, "text/html", PORTAL_PAGE);
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

  server.send(200, "application/json", "{\"ok\":true}");
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
  server.send(200, "application/json", "{\"ok\":true}");
  delay(200);

  if (s_apMode) {
    dns.stop();
    WiFi.softAPdisconnect(true);
    s_apMode = false;
    WiFi.mode(WIFI_STA);
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

void portal_begin(bool forceAp) {
  if (forceAp || !settings_hasWifi()) {
    buildApSsid();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(s_apSsid);           // öppet nät: håller join-QR:en kort
    dns.start(53, "*", WiFi.softAPIP());
    s_apMode = true;
  }

  server.on("/",              HTTP_GET,  handleRoot);
  server.on("/api/settings",  HTTP_GET,  handleGetSettings);
  server.on("/api/settings",  HTTP_POST, handlePostSettings);
  server.on("/api/wifi/scan", HTTP_GET,  handleWifiScan);
  server.on("/api/wifi",      HTTP_POST, handlePostWifi);
  server.on("/api/status",    HTTP_GET,  handleStatus);
  server.onNotFound(handleNotFound);

  server.begin();
}

void portal_task(void* pv) {
  (void)pv;
  for (;;) {
    if (s_apMode) dns.processNextRequest();
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
