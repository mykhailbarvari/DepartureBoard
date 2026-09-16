#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// All beständig konfiguration på ett ställe.
//
// Tidigare öppnades och stängdes Preferences på åtta olika ställen, var och en
// med sin egen nyckelsträng. Nu finns nycklarna bara här, och webbportalen
// skriver till samma struct.
//
// Fälten läses direkt av flera tasks. Skalärerna är små, så en läsning är
// atomär på ESP32. Strängarna skrivs bara från portalen och läses vid
// anslutning, alltså aldrig samtidigt i praktiken.

// Trafikslag som bitmask. 0 = allt som går från hållplatsen.
#define TMODE_MASK_BUS   0x01
#define TMODE_MASK_METRO 0x02
#define TMODE_MASK_TRAIN 0x04
#define TMODE_MASK_TRAM  0x08
#define TMODE_MASK_SHIP  0x10

typedef struct {
  uint8_t  brightness;     // 0-255
  uint16_t colourway;      // RGB565, en av COLOR_*
  uint8_t  walkMinutes;    // 0 = av; filtrerar bort avgångar man ändå missar
  uint8_t  directionCode;  // 0 = båda riktningarna
  uint32_t siteId;         // SL:s hållplats-id
  uint8_t  transportMask;  // se TMODE_MASK_*; 0 = alla
  char     siteName[32];   // visningsnamn, enbart för UI:t
  char     wifiSsid[33];   // 32 tecken + NUL
  char     wifiPass[65];   // 64 tecken + NUL
} Settings;

extern Settings g_settings;

// Läser NVS till g_settings. Saknade nycklar får sina defaults, där WiFi
// faller tillbaka på secrets.h så en redan flashad tavla fortsätter fungera.
// Måste anropas före display_init(), som läser ljusstyrkan härifrån.
void settings_load(void);

// Skriver hela g_settings till NVS.
void settings_save(void);

// true när vi har något att ansluta till. Är den false startar enheten
// SoftAP-provisionering istället.
bool settings_hasWifi(void);

// Bygger SL:s transport-parameter, t.ex. "BUS,METRO". Tom sträng = alla
// trafikslag, och då ska parametern utelämnas helt ur URL:en.
void settings_transportParam(char* out, size_t cap);
