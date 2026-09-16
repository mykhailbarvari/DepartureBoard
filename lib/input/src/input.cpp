#include <Arduino.h>
#include <input.h>
#include <config.h>

// Constants
static const uint32_t DEBOUNCE_BTN_MS = 35;
static const int ENCODER_STEPS_PER_CLICK = 1;

// Button state structure
struct ButtonState {
  bool stable = false;
  bool lastRaw = false;
  uint32_t lastChangeMs = 0;
};

static ButtonState s_onoff;
static ButtonState s_select;
static bool s_encNext = false;
static bool s_encPrev = false;

// Gestdetektering för encoderns tryckknapp
static bool       s_selPrev      = false;
static uint32_t   s_selDownMs    = 0;
static bool       s_selLongFired = false;
static PressEvent s_selEvent     = PRESS_NONE;

// Debounce a button and return its stable state
static bool debounceButton(bool raw, ButtonState &btn) {
  uint32_t now = millis();
  if (raw != btn.lastRaw) {
    btn.lastRaw = raw;
    btn.lastChangeMs = now;
  }
  if ((now - btn.lastChangeMs) >= DEBOUNCE_BTN_MS) {
    btn.stable = btn.lastRaw;
  }
  return btn.stable;
}

// Encoder state
static int s_aLast = HIGH;
static int s_encAccum = 0;

void input_init(void) {
  pinMode(INPUT1, INPUT_PULLUP);
  pinMode(INPUT2, INPUT_PULLUP);

  pinMode(INPUT3, INPUT_PULLUP); // Encoder A (GPIO6)
  pinMode(INPUT4, INPUT_PULLUP); // Encoder B (GPIO7)

  s_aLast = digitalRead(INPUT3);
  s_encAccum = 0;
}

// Update encoder state (separate function for clarity)
static void input_updateEncoder(void) {
  int aNow = digitalRead(INPUT3);

  // React on both edges of A (HIGH<->LOW)
  if (aNow != s_aLast) {
    int b = digitalRead(INPUT4);

    if (b != aNow) {
      s_encAccum++;
    } else {
      s_encAccum--;
    }

    s_aLast = aNow;
  }

  // En detent ska ge exakt ett steg. Känns det som att listan hoppar två
  // steg per klick är ENCODER_STEPS_PER_CLICK för lågt för den här encodern.
  if (s_encAccum >= ENCODER_STEPS_PER_CLICK) {
    s_encNext = true;
    s_encAccum = 0;
  }
  if (s_encAccum <= -ENCODER_STEPS_PER_CLICK) {
    s_encPrev = true;
    s_encAccum = 0;
  }
}

// Bygger PRESS_SHORT/PRESS_LONG ovanpå den debouncade knappnivån.
static void input_updateGestures(void) {
  const bool now = s_select.stable;
  const uint32_t t = millis();

  if (now && !s_selPrev) {           // nedtryck
    s_selDownMs = t;
    s_selLongFired = false;
  }

  // Långtryck rapporteras direkt när tiden passeras, inte vid släpp.
  if (now && !s_selLongFired && (t - s_selDownMs) >= LONG_PRESS_MS) {
    s_selEvent = PRESS_LONG;
    s_selLongFired = true;
  }

  // Släpp utan att långtrycket hunnit lösa ut = korttryck.
  if (!now && s_selPrev && !s_selLongFired) {
    s_selEvent = PRESS_SHORT;
  }

  s_selPrev = now;
}

void input_update(void) {
  // Buttons: LOW = active (due to INPUT_PULLUP)
  bool raw_onoff = (digitalRead(INPUT1) == LOW);
  bool raw_select = (digitalRead(INPUT2) == LOW);

  debounceButton(raw_onoff, s_onoff);
  debounceButton(raw_select, s_select);

  input_updateGestures();
  input_updateEncoder();
}

// Button level queries
bool input_onoff(void) {
  return s_onoff.stable;
}

bool input_select(void) {
  return s_select.stable;
}

PressEvent input_selectEvent(void) {
  PressEvent e = s_selEvent;
  s_selEvent = PRESS_NONE;
  return e;
}

// Encoder: consume so ControlLogicTask (20ms) doesn't miss events.
// ENCODER_INVERT vänder hela konventionen på ett enda ställe.
bool input_encoderNext(void) {
#if ENCODER_INVERT
  bool hit = s_encPrev; s_encPrev = false;
#else
  bool hit = s_encNext; s_encNext = false;
#endif
  return hit;
}

bool input_encoderPrev(void) {
#if ENCODER_INVERT
  bool hit = s_encNext; s_encNext = false;
#else
  bool hit = s_encPrev; s_encPrev = false;
#endif
  return hit;
}
