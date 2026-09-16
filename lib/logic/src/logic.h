#pragma once
#include <Arduino.h>
#include <display.h>   // om du behöver COLOR_* och drawString-signaturen
#include <layout.h>
#include <config.h>

// Struct för vår API-Array
typedef struct {
  char line[8];
  char destination[32];
  char display[8];     // råsträng från SL, t.ex. "5 min" eller "14:35"
  char depTime[6];     // HH:MM från expected, t.ex. "14:35"
  uint16_t minsUntil;  // beräknade minuter tills avgång (via NTP + expected)
} Departure;

extern Departure departures[MAX_DEPARTURES];
extern int departureCount;

void drawRow(int row, const Departure* departure, int yOffset = 0);
void renderMainFromArray(int startIndex);

void mainTask(void);
void renderBoot(void);
void renderMainMenu(void);
void renderDisplayMenu(void);
void renderBrightness(void);
void renderColourwayMenu(void);
void renderSystemSettings(void);
void renderStation(int navIndex, bool walkEditing, int directionCode, int walkMinutes);
void mainMenuWrap();
void listClamp(int count, int visible);

// Globalt Deklarerade Structs
typedef enum {
  STATE_BOOT,
  STATE_INIT,
  STATE_MENU,
  STATE_DEPARTURES,
  STATE_DISPLAY_MENU,
  STATE_BRIGHTNESS,
  STATE_COLOURWAY,
  STATE_STATION,
  STATE_STATION_WALKTIME,
  STATE_SYSTEM_SETTINGS
} AppState;

typedef struct {
  AppState state;
  int selectedIndex;
  int scrollOffset;
  bool dirty;
  uint32_t bootStartMs;
  int8_t bouncePixels;
  uint32_t bounceStartMs;
} UiState;

extern UiState ui;
extern volatile uint16_t g_colourway;
extern volatile bool g_fetching;
