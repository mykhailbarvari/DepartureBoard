#pragma once
#include <Arduino.h>
#include <time.h>
#include <display.h>   // om du behöver COLOR_* och drawString-signaturen
#include <layout.h>
#include <config.h>
#include <api.h>
#include <settings.h>

// SL:s "state"-fält för en avgång.
typedef enum {
  DEP_EXPECTED = 0,  // normal, väntad avgång
  DEP_ATSTOP,        // står vid hållplatsen just nu
  DEP_CANCELLED,     // inställd
  DEP_STATE_OTHER
} DepState;

// SL:s "line.transport_mode". Styr färgkodningen i UI:t.
typedef enum {
  TMODE_BUS = 0,
  TMODE_METRO,
  TMODE_TRAIN,
  TMODE_TRAM,
  TMODE_SHIP,
  TMODE_OTHER
} TransportMode;

// Struct för vår API-Array
typedef struct {
  char     line[8];
  char     destination[32];
  char     display[8];      // råsträng från SL, t.ex. "5 min" eller "14:35"
  char     depTime[6];      // HH:MM från expected, t.ex. "14:35"
  char     stopPoint[4];    // läge, t.ex. "A" (tomt på små hållplatser)
  char     groupOfLines[28];// t.ex. "Tunnelbanans gröna linje", "Blåbuss"
  time_t   depEpoch;        // faktisk avgångstid (expected)
  time_t   schedEpoch;      // tidtabellstid (scheduled)
  int8_t   delayMin;        // expected - scheduled i minuter, negativt = tidig
  uint8_t  state;           // DepState
  uint8_t  transportMode;   // TransportMode
  uint8_t  directionCode;   // 1 eller 2
  bool     hasDeviation;    // minst en post i "deviations"
} Departure;

extern Departure departures[MAX_DEPARTURES];
extern int departureCount;

// Minuter kvar till avgång, räknat NU och inte vid hämtningen. Det är detta
// som gör att nedräkningen tickar mellan hämtningarna. Negativt = redan gått.
int departure_minsUntil(const Departure* d);

// Aktuella filter (riktning + gångtid) på ett enda ställe, så att rendering
// och scroll-klampning aldrig kan glida isär.
bool departure_passesFilter(const Departure* d);

// Linjens färg enligt SL:s egen färgsättning (group_of_lines/transport_mode).
uint16_t departure_lineColor(const Departure* d);
int  departures_visibleCount(void);
const Departure* departures_at(int visibleIndex);
int  departures_rowCapacity(void);

void drawRow(int row, const Departure* departure, int yOffset = 0);
void renderMainFromArray(int startIndex);

void renderBoot(void);
void renderMainMenu(void);      // karusellen
void renderBrightness(void);
void renderColourwayMenu(void);
void renderQR(void);            // nätverksskärmen; QR-koden ritas i fas 4
void renderSystem(void);
void renderStation(int navIndex, bool walkEditing, int directionCode, int walkMinutes);
void mainMenuWrap();

// Antal poster i karusellen (definieras i logic.cpp).
extern const int kMenuItemCount;

// Globalt Deklarerade Structs
typedef enum {
  STATE_BOOT,
  STATE_DEPARTURES,        // hemskärmen
  STATE_MENU,              // karusellen
  STATE_BRIGHTNESS,
  STATE_COLOURWAY,
  STATE_STATION,           // TILLFÄLLIG — ersätts av webbportalen i fas 4
  STATE_STATION_WALKTIME,  // TILLFÄLLIG — dito
  STATE_QR,
  STATE_SYSTEM
} AppState;

typedef struct {
  AppState state;
  int selectedIndex;
  int scrollOffset;
  bool dirty;
  uint32_t bootStartMs;
  int8_t bouncePixels;
  uint32_t bounceStartMs;
  AppState returnTo;   // dit "tillbaka" leder från skärmar som nås flera vägar
} UiState;

extern UiState ui;
extern volatile bool g_fetching;
