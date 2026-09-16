#include "settings.h"
#include <Preferences.h>
#include <config.h>
#include <display.h>

Settings g_settings;

// Namnrymd och nycklar är medvetet desamma som tidigare, så befintliga
// sparade värden överlever bytet till den här modulen.
static const char* NS = "board";

void settings_load(void) {
  Preferences prefs;
  prefs.begin(NS, true);  // read-only

  g_settings.brightness    = prefs.getUChar ("brightness", DEFAULT_BRIGHTNESS);
  g_settings.colourway     = prefs.getUShort("colourway",  COLOR_ORANGE);
  g_settings.walkMinutes   = prefs.getUChar ("walkmin",    0);
  g_settings.directionCode = prefs.getUChar ("dircode",    0);
  g_settings.siteId        = prefs.getUInt  ("siteid",     SITE_ID);

  prefs.end();

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

  prefs.end();
}
