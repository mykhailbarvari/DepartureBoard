#include "settings.h"
#include <Preferences.h>
#include <config.h>
#include <display.h>
#include <string.h>
#include <stdio.h>

Settings g_settings;

// Namnrymd och nycklar är medvetet desamma som tidigare, så befintliga
// sparade värden överlever bytet till den här modulen.
// NVS-nycklar får vara högst 15 tecken.
static const char* NS = "board";

static void copyStr(char* dst, size_t cap, const char* src) {
  if (!cap) return;
  if (!src) { dst[0] = 0; return; }
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = 0;
}

void settings_load(void) {
  Preferences prefs;
  prefs.begin(NS, true);  // read-only

  g_settings.brightness    = prefs.getUChar ("brightness", DEFAULT_BRIGHTNESS);
  g_settings.colourway     = prefs.getUShort("colourway",  COLOR_ORANGE);
  g_settings.walkMinutes   = prefs.getUChar ("walkmin",    0);
  g_settings.directionCode = prefs.getUChar ("dircode",    0);
  g_settings.siteId        = prefs.getUInt  ("siteid",     SITE_ID);
  g_settings.transportMask = prefs.getUChar ("tmask",      TMODE_MASK_BUS);

  String name = prefs.getString("sitename", "");
  String ssid = prefs.getString("wifissid",  WIFI_SSID);
  String pass = prefs.getString("wifipass",  WIFI_PASSWORD);

  prefs.end();

  copyStr(g_settings.siteName, sizeof(g_settings.siteName), name.c_str());
  copyStr(g_settings.wifiSsid, sizeof(g_settings.wifiSsid), ssid.c_str());
  copyStr(g_settings.wifiPass, sizeof(g_settings.wifiPass), pass.c_str());

  // Skydda mot skadade värden i NVS.
  if (g_settings.directionCode > 2) g_settings.directionCode = 0;
  if (g_settings.walkMinutes  > 60) g_settings.walkMinutes   = 0;
  if (g_settings.siteId == 0)       g_settings.siteId        = SITE_ID;
}

void settings_save(void) {
  Preferences prefs;
  prefs.begin(NS, false);  // read-write

  prefs.putUChar ("brightness", g_settings.brightness);
  prefs.putUShort("colourway",  g_settings.colourway);
  prefs.putUChar ("walkmin",    g_settings.walkMinutes);
  prefs.putUChar ("dircode",    g_settings.directionCode);
  prefs.putUInt  ("siteid",     g_settings.siteId);
  prefs.putUChar ("tmask",      g_settings.transportMask);
  prefs.putString("sitename",   g_settings.siteName);
  prefs.putString("wifissid",   g_settings.wifiSsid);
  prefs.putString("wifipass",   g_settings.wifiPass);

  prefs.end();
}

bool settings_hasWifi(void) {
  return g_settings.wifiSsid[0] != 0;
}

void settings_transportParam(char* out, size_t cap) {
  if (!out || cap == 0) return;
  out[0] = 0;

  const uint8_t m = g_settings.transportMask;
  if (m == 0) return;   // alla trafikslag → utelämna parametern

  struct Entry { uint8_t bit; const char* name; };
  static const Entry kEntries[] = {
    { TMODE_MASK_BUS,   "BUS"   },
    { TMODE_MASK_METRO, "METRO" },
    { TMODE_MASK_TRAIN, "TRAIN" },
    { TMODE_MASK_TRAM,  "TRAM"  },
    { TMODE_MASK_SHIP,  "SHIP"  },
  };

  for (size_t i = 0; i < sizeof(kEntries) / sizeof(kEntries[0]); i++) {
    if (!(m & kEntries[i].bit)) continue;
    if (out[0]) strncat(out, ",", cap - strlen(out) - 1);
    strncat(out, kEntries[i].name, cap - strlen(out) - 1);
  }
}
