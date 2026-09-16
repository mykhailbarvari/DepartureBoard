#pragma once
#include <stdint.h>
#include <stdbool.h>

// Liten tween-hjälpare för UI:ts glidande övergångar.
//
// Modellen är "förskjutning som ebbar ut": när något byts sätts en förskjutning
// i pixlar som motsvarar var bilden nyss stod, och den easas sedan mot noll.
// Renderaren ritar helt enkelt allt förskjutet med `current`.
//
// Fördelen mot en från/till-tween är att snabb snurrning blir mjuk —
// ui_animNudge() lägger till ovanpå den förskjutning som redan finns, så ett
// nytt steg mitt i en pågående rörelse fortsätter från där bilden faktiskt är
// istället för att hoppa tillbaka till start.

#define UI_ANIM_MS  120   // rörelsens längd; ett ställe att ändra hastigheten på
// Bildrutetakt MEDAN en rörelse pågår. 20 ms ger 6-7 bildrutor per rörelse,
// vilket är det minsta som läses som en glidning. Sänk till 16 om mätningen
// visar att en hel bildruta ritas snabbare än så — det ger 8 och blir mjukare.
#define UI_FRAME_MS 20

typedef struct {
  int32_t  from;      // förskjutning i px när steget togs
  int32_t  current;   // aktuell förskjutning — det renderaren läser
  uint32_t startMs;
  bool     active;
} UiAnim;

// Lägger deltaPx till den aktuella förskjutningen och startar om rörelsen.
// clampPx håller summan inom ±clampPx, så snabb snurrning inte kan dra iväg
// förskjutningen längre än en skärm.
void ui_animNudge(UiAnim* a, int deltaPx, int clampPx);

// Räknar om `current` utifrån klockan. Anropas en gång per bildruta, före
// ritningen. Sätter active = false när rörelsen är slut.
void ui_animUpdate(UiAnim* a);

// Nollställer direkt, utan animation.
void ui_animReset(UiAnim* a);

static inline bool ui_animActive(const UiAnim* a) {
  return a && a->active;
}
