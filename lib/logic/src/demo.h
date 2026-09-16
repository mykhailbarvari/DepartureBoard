#pragma once
#include <stdbool.h>
#include <input.h>   // PressEvent

// Demoläge: en automatisk rundtur genom gränssnittet, avsedd att filmas.
//
// Demot ritar inga egna skärmar. Det syntetiserar samma händelser som
// encodern ger — ett steg framåt, ett bakåt, ett kort tryck, ett långt — och
// ControlLogicTask behandlar dem precis som riktig inmatning. Därför spelas
// varje övergång och varje animation exakt som den ser ut i normal användning,
// och det finns ingen andra uppsättning skärmlogik som kan glida isär.
//
// Turen börjar i MB Labs-logotypen och loopar tills den avbryts.

void demo_start(void);
void demo_stop(void);
bool demo_active(void);

// Fyller i vad "encodern" gör just nu enligt tidslinjen. Anropas av
// ControlLogicTask i stället för att läsa den riktiga inmatningen.
void demo_input(PressEvent* press, bool* next, bool* prev);
