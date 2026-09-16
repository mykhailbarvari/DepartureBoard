#pragma once
#include <Arduino.h>
#include <stdbool.h>
#include <config.h>

// Enda knappen (encoderns tryck) bär två gester, eftersom hårdvaran saknar
// en dedikerad back-knapp:
//   PRESS_SHORT = välj / bekräfta
//   PRESS_LONG  = tillbaka ett steg (på hemskärmen: reserverad för QR-koden)
// PRESS_LONG skickas i samma stund som tiden passeras, inte vid släpp, så
// gesten känns av direkt i handen.
typedef enum {
  PRESS_NONE = 0,
  PRESS_SHORT,
  PRESS_LONG
} PressEvent;

void input_init(void);
void input_update(void);

bool input_onoff(void);
bool input_select(void);   // rå nivå, används inte för navigation

// Konsumerar och returnerar nästa knapphändelse (eller PRESS_NONE).
PressEvent input_selectEvent(void);

// Encodern beskrivs i listriktning, inte i rotationsriktning: NEXT flyttar
// nedåt i en lista, PREV uppåt. Alla skärmar använder samma konvention.
// Känns det bakvänt på hårdvaran: sätt ENCODER_INVERT till 1 i config.h.
bool input_encoderNext(void);
bool input_encoderPrev(void);
