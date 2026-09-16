#include "demo.h"
#include <logic.h>
#include <Arduino.h>

// Vad ett steg gör. Åtgärden utförs en gång när steget börjar, sedan ligger
// bilden still i hold-tiden så att den hinner filmas.
typedef enum {
  D_WAIT = 0,   // gör inget, bara stanna kvar
  D_NEXT,       // ett encodersteg framåt
  D_PREV,       // ett bakåt
  D_PRESS,      // kort tryck (välj)
  D_LONG,       // långt tryck (tillbaka / ångra)
  D_LOOP        // börja om från logotypen
} DemoOp;

typedef struct {
  uint8_t  op;
  uint16_t hold;   // ms att ligga kvar efter åtgärden
} DemoStep;

// Turen. Menyindex följer kMenuItems: 0 Avgångar, 1 Ljusstyrka, 2 Färgtema,
// 3 Hållplats, 4 Nätverk, 5 System.
//
// Två skärmar lämnas medvetet med LÅNGT tryck i stället för kort: temamenyn
// sparar färgen vid kort tryck, och demot ska inte ändra inställningar hos den
// som filmar. Ljusstyrkan besöks utan att vridas, så att den sparar tillbaka
// samma värde den kom in med.
static const DemoStep kScript[] = {
  { D_WAIT,  3400 },   // logotypen — bootsekvensen går själv vidare till avgångarna
  { D_WAIT,  1800 },   // avgångslistan i vila

  { D_NEXT,   850 },   // scroll nedåt: visar radernas glidning
  { D_NEXT,   850 },
  { D_NEXT,   850 },
  { D_PREV,   850 },   // och tillbaka upp
  { D_PREV,  1100 },

  { D_PRESS, 1300 },   // -> karusellen, står på Avgångar

  { D_NEXT,  1000 },   // Ljusstyrka
  { D_NEXT,  1000 },   // Färgtema
  { D_NEXT,  1000 },   // Hållplats
  { D_NEXT,  1000 },   // Nätverk
  { D_NEXT,  1200 },   // System

  { D_PREV,  1100 },   // tillbaka till Nätverk
  { D_PRESS, 4200 },   // QR-koden — lång paus, den ska hinna skannas i bild
  { D_PRESS, 1100 },   // tillbaka till karusellen

  { D_PREV,   900 },   // Hållplats
  { D_PRESS, 2200 },   // hållplatsskärmen
  { D_LONG,  1000 },   // tillbaka

  { D_PREV,  1000 },   // Färgtema
  { D_PRESS, 1100 },   // temalistan
  { D_NEXT,   750 },   // markeringen glider inuti fönstret
  { D_NEXT,   750 },
  { D_NEXT,   750 },
  { D_NEXT,   750 },   // härifrån glider hela listan
  { D_NEXT,  1000 },
  { D_LONG,  1000 },   // ångra utan att spara

  { D_PREV,  1000 },   // Ljusstyrka
  { D_PRESS, 1800 },   // stapeln — encodern rörs inte, så inget värde ändras
  { D_PRESS, 1100 },   // tillbaka

  { D_LONG,  2200 },   // hem till avgångarna
  { D_LOOP,     0 },
};

static const int kStepCount = (int)(sizeof(kScript) / sizeof(kScript[0]));

static bool     s_active  = false;
static int      s_step    = 0;
static uint32_t s_started = 0;
static bool     s_pending = false;   // stegets åtgärd är ännu inte utförd

// Börja om från logotypen. Bootlogiken går själv vidare till avgångarna.
static void toLogo(void) {
  ui.state        = STATE_BOOT;
  ui.bootStartMs  = 0;
  ui.selectedIndex = 0;
  ui.scrollOffset = 0;
  ui.dirty        = true;
}

void demo_start(void) {
  s_active  = true;
  s_step    = 0;
  s_pending = true;
  s_started = millis();
  toLogo();
}

void demo_stop(void) {
  if (!s_active) return;
  s_active = false;

  ui.state         = STATE_DEPARTURES;
  ui.selectedIndex = 0;
  ui.scrollOffset  = 0;
  ui.dirty         = true;
}

bool demo_active(void) {
  return s_active;
}

void demo_input(PressEvent* press, bool* next, bool* prev) {
  *press = PRESS_NONE;
  *next  = false;
  *prev  = false;

  if (!s_active) return;

  // Åtgärden utförs en gång, i början av steget.
  if (s_pending) {
    switch (kScript[s_step].op) {
      case D_NEXT:  *next  = true;         break;
      case D_PREV:  *prev  = true;         break;
      case D_PRESS: *press = PRESS_SHORT;  break;
      case D_LONG:  *press = PRESS_LONG;   break;
      case D_LOOP:  toLogo();              break;
      default:                             break;
    }
    s_pending = false;
    s_started = millis();
    return;
  }

  // Ligg kvar tills hold-tiden gått ut, gå sedan vidare.
  if (millis() - s_started >= kScript[s_step].hold) {
    s_step++;
    if (s_step >= kStepCount || kScript[s_step].op == D_LOOP) {
      s_step = 0;
      toLogo();
    }
    s_pending = true;
  }
}
