#pragma once
#include <Arduino.h>
#include <display.h>   // om du behöver COLOR_* och drawString-signaturen
#include <layout.h>
#include <config.h>

// Struct för vår API-Array
typedef struct {
  char line[8];
  char destination[32];
  char display[8];
} Departure;

extern Departure departures[MAX_DEPARTURES];
extern int departureCount;

void drawRow(int row, const Departure* departure);
void renderMainFromArray(int startIndex);

void mainTask(void);
void renderBoot(void);
void renderMainMenu(void);
void mainMenuWrap();
void listClamp(int count, int visible);

// Globalt Deklarerade Structs
typedef enum {
  STATE_BOOT,
  STATE_INIT,  // Nån Initializing loop kanske, nån UI grejja, Animering av startup
  STATE_MENU,
  STATE_DEPARTURES,
  STATE_BRIGHTNESS,
  STATE_STATION,
  STATE_SYSTEM_SETTINGS
} AppState;

typedef struct {
  AppState state;
  int selectedIndex;
  int scrollOffset;
  bool dirty;
  uint32_t bootStartMs;

} UiState;

extern UiState ui;
