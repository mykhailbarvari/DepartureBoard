#pragma once
#include <stdint.h>

// All beständig konfiguration på ett ställe.
//
// Tidigare öppnades och stängdes Preferences på sju olika ställen i main.cpp
// plus ett i display.cpp, var och en med sin egen nyckelsträng. Nu finns
// nycklarna bara här, och webbportalen i fas 4 skriver till samma struct.
//
// Fälten läses direkt av flera tasks. De är små skalärer, så en läsning är
// atomär på ESP32 och behöver ingen mutex.
typedef struct {
  uint8_t  brightness;     // 0-255
  uint16_t colourway;      // RGB565, en av COLOR_*
  uint8_t  walkMinutes;    // 0 = av; filtrerar bort avgångar man ändå missar
  uint8_t  directionCode;  // 0 = båda riktningarna
  uint32_t siteId;         // SL:s hållplats-id
} Settings;

extern Settings g_settings;

// Läser NVS till g_settings. Saknade nycklar får sina defaults.
// Måste anropas före display_init(), som läser ljusstyrkan härifrån.
void settings_load(void);

// Skriver hela g_settings till NVS.
void settings_save(void);
