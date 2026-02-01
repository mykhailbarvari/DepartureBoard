#include <Arduino.h>
#include <input.h>
#include "C:\Users\misha\Documents\PlatformIO\Projects\DepartureBoard\include\config.h"

static bool s_onoff, s_nav1, s_nav2, s_select;

// Debounce state per knapp
static bool s_onoff_stable = false, s_nav1_stable = false, s_nav2_stable = false, s_select_stable = false;
static bool s_onoff_lastRaw = false, s_nav1_lastRaw = false, s_nav2_lastRaw = false, s_select_lastRaw = false;
static uint32_t s_onoff_lastChangeMs = 0, s_nav1_lastChangeMs = 0, s_nav2_lastChangeMs = 0, s_select_lastChangeMs = 0;

static bool debounceButton(bool raw, bool &stable, bool &lastRaw, uint32_t &lastChangeMs, uint32_t debounceMs) {
  uint32_t now = millis();

  // Om rå signal ändras -> starta om timer
  if (raw != lastRaw) {
    lastRaw = raw;
    lastChangeMs = now;
  }
  // Om rå signal varit oförändrad tillräckligt länge -> acceptera som stabil
  if ((now - lastChangeMs) >= debounceMs) {
    stable = lastRaw;
  }
  return stable;
}

void input_init(void) {
  pinMode(INPUT1, INPUT_PULLUP);
  pinMode(INPUT2, INPUT_PULLUP);
  pinMode(INPUT3, INPUT_PULLUP);
  pinMode(INPUT4, INPUT_PULLUP);
}

void input_update(void) {
  const uint32_t DEBOUNCE_MS = 35; // prova 20–35ms

  // LOW = tryckt (pga INPUT_PULLUP)
  bool raw_onoff  = (digitalRead(INPUT1) == LOW);
  bool raw_nav1   = (digitalRead(INPUT2) == LOW);
  bool raw_nav2   = (digitalRead(INPUT3) == LOW);
  bool raw_select = (digitalRead(INPUT4) == LOW);

  s_onoff  = debounceButton(raw_onoff,  s_onoff_stable,  s_onoff_lastRaw,  s_onoff_lastChangeMs,  DEBOUNCE_MS);
  s_nav1   = debounceButton(raw_nav1,   s_nav1_stable,   s_nav1_lastRaw,   s_nav1_lastChangeMs,   DEBOUNCE_MS);
  s_nav2   = debounceButton(raw_nav2,   s_nav2_stable,   s_nav2_lastRaw,   s_nav2_lastChangeMs,   DEBOUNCE_MS);
  s_select = debounceButton(raw_select, s_select_stable, s_select_lastRaw, s_select_lastChangeMs, DEBOUNCE_MS);
}

bool input_onoff(void)  { return s_onoff;  }
bool input_nav1(void)   { return s_nav1;   }
bool input_nav2(void)   { return s_nav2;   }
bool input_select(void) { return s_select; }
