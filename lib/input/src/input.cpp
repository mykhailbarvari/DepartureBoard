#include <Arduino.h>
#include <input.h>
#include <config.h>

static bool s_onoff, s_nav1, s_nav2, s_select;

// Debounce state per knapp
static bool s_onoff_stable = false, s_select_stable = false;
static bool s_onoff_lastRaw = false, s_select_lastRaw = false;
static uint32_t s_onoff_lastChangeMs = 0, s_select_lastChangeMs = 0;

static bool debounceButton(bool raw, bool &stable, bool &lastRaw, uint32_t &lastChangeMs, uint32_t debounceMs) {
  uint32_t now = millis();
  if (raw != lastRaw) { lastRaw = raw; lastChangeMs = now; }
  if ((now - lastChangeMs) >= debounceMs) stable = lastRaw;
  return stable;
}

// --------- ENCODER (polling + latch/consume) ----------
static int s_aLast = HIGH;
static int s_encAccum = 0;
// -----------------------------------------------------

void input_init(void) {
  pinMode(INPUT1, INPUT_PULLUP);
  pinMode(INPUT2, INPUT_PULLUP);

  pinMode(INPUT3, INPUT_PULLUP); // Encoder A (GPIO6)
  pinMode(INPUT4, INPUT_PULLUP); // Encoder B (GPIO7)

  s_aLast = digitalRead(INPUT3);
  s_encAccum = 0;
}

void input_update(void) {
  const uint32_t DEBOUNCE_BTN_MS = 35;

  // Knappar: LOW = aktiv (pga INPUT_PULLUP)
  bool raw_onoff  = (digitalRead(INPUT1) == LOW);
  bool raw_select = (digitalRead(INPUT2) == LOW);

  s_onoff  = debounceButton(raw_onoff,  s_onoff_stable,  s_onoff_lastRaw,  s_onoff_lastChangeMs,  DEBOUNCE_BTN_MS);
  s_select = debounceButton(raw_select, s_select_stable, s_select_lastRaw, s_select_lastChangeMs, DEBOUNCE_BTN_MS);

  // --------- ENCODER: 1 fysiskt klick => 1 event ----------
  int aNow = digitalRead(INPUT3);

  // reagera på BÅDA flanker på A (HIGH<->LOW)
  if (aNow != s_aLast) {
    int b = digitalRead(INPUT4);

    // riktning: om fel håll, byt ++/-- här
    if (b != aNow) s_encAccum++;
    else           s_encAccum--;

    s_aLast = aNow;
  }

  // EC11E12-15P30: 30 detents/varv men 15 PPR => 2 A-flanker per klick
  const int STEPS_PER_CLICK = 1;

  if (s_encAccum >= STEPS_PER_CLICK)  { s_nav1 = true; s_encAccum = 0; }  // UP (latched)
  if (s_encAccum <= -STEPS_PER_CLICK) { s_nav2 = true; s_encAccum = 0; }  // DOWN (latched)
  // --------------------------------------------------------
}

// Knappnivåer (som innan)
bool input_onoff(void)  { return s_onoff;  }
bool input_select(void) { return s_select; }

// Encoder: consume så ControlLogicTask (20ms) inte kan missa event
bool input_nav1(void) { bool t = s_nav1; s_nav1 = false; return t; } // UP (1 gång)
bool input_nav2(void) { bool t = s_nav2; s_nav2 = false; return t; } // DOWN (1 gång)
