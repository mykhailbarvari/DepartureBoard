#include "ui_anim.h"
#include <Arduino.h>

void ui_animReset(UiAnim* a) {
  if (!a) return;
  a->from    = 0;
  a->current = 0;
  a->startMs = 0;
  a->active  = false;
}

void ui_animNudge(UiAnim* a, int deltaPx, int clampPx) {
  if (!a) return;

  // Ackumulera ovanpå det som redan pågår, så snabb snurrning inte hackar.
  int32_t v = a->current + deltaPx;
  if (clampPx > 0) {
    if (v >  clampPx) v =  clampPx;
    if (v < -clampPx) v = -clampPx;
  }

  a->from    = v;
  a->current = v;
  a->startMs = millis();
  a->active  = (v != 0);
}

void ui_animUpdate(UiAnim* a) {
  if (!a || !a->active) return;

  const uint32_t elapsed = millis() - a->startMs;

  if (elapsed >= UI_ANIM_MS) {
    a->current = 0;
    a->active  = false;
    return;
  }

  // Ease-out KVADRATISK: current = from * (1 - p)^2.
  //
  // Kubisk vore mjukare med gott om bildrutor, men en rörelse ryms bara i 6-8
  // stycken här och då blir den för framtung: första bildrutan flyttade 54 av
  // 128 px och innehållet hann passera skärmen med bara ett par pixlar synliga.
  // Kvadratisk fördelar stegen jämnare (40, 32, 24, 18, 11, 3) och håller
  // alltid något läsbart på skärmen.
  //
  // Float undviks medvetet — det här körs per bildruta på en panel som redan
  // spenderar det mesta av sin tid på att måla om DMA-bufferten.
  const int32_t p = (int32_t)(elapsed * 1000UL / UI_ANIM_MS);  // 0..999
  const int32_t q = 1000 - p;
  const int32_t f = q * q / 1000;                              // (1-p)^2, 0..1000

  a->current = a->from * f / 1000;
}
