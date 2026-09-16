#include "ui_carousel.h"
#include "ui_icons.h"
#include "ui_anim.h"
#include <display.h>

// Layout på 128x64. Justera dessa om det ser trångt ut på panelen —
// de är avsiktligt samlade här och inte utspridda i koden.
#define CAR_W         128
#define CAR_ICON_Y    5    // ikonens övre kant
#define CAR_LABEL_Y   32   // etikettens övre kant (fonten är 12 px hög)
#define CAR_DOTS_Y    52   // punktindikatorns övre kant
#define CAR_DOT_D     3    // punktens sida i px
#define CAR_DOT_GAP   5    // luft mellan punkter
#define CAR_ARROW_W   4

// Förskjutningen i px. Positiv = innehållet ligger till höger om sin vila,
// alltså precis efter ett "nästa"-steg.
static UiAnim s_slide;

// Vilken pil som ska pulsera, och när steget togs.
static int      s_pulseDir   = 0;
static uint32_t s_pulseStart = 0;

void ui_carouselNudge(int dir) {
  if (dir == 0) return;

  // Efter ett "nästa"-steg (dir = +1) ska den nya posten komma in från höger,
  // alltså starta på +CAR_W och glida till 0.
  ui_animNudge(&s_slide, dir * CAR_W, CAR_W);

  s_pulseDir   = dir;
  s_pulseStart = millis();
}

void ui_carouselReset(void) {
  ui_animReset(&s_slide);
  s_pulseDir = 0;
}

bool ui_carouselAnimating(void) {
  return ui_animActive(&s_slide);
}

// Liten triangel: spetsen är smalast, basen 7 px hög.
static void drawArrow(int x, int yc, bool pointsLeft, uint16_t color) {
  for (int k = 0; k < CAR_ARROW_W; k++) {
    const int h   = 2 * k + 1;
    const int col = pointsLeft ? (x + k) : (x + (CAR_ARROW_W - 1) - k);
    display_fillRect(col, yc - k, 1, h, color);
  }
}

// Pilen i den vridna riktningen lyser upp och trappas ner igen. Tre diskreta
// steg istället för färginterpolation — RGB565-blandning är inte värd
// komplexiteten vid den här storleken.
static uint16_t arrowColor(int dir, uint16_t accent) {
  if (s_pulseDir != dir) return COLOR_GRAY_25;

  const uint32_t elapsed = millis() - s_pulseStart;
  if (elapsed >= UI_ANIM_MS)     return COLOR_GRAY_25;
  if (elapsed < UI_ANIM_MS / 3)  return accent;
  if (elapsed < UI_ANIM_MS * 2 / 3) return COLOR_GRAY_75;
  return COLOR_GRAY_25;
}

// Ritar en post centrerad kring xCenter. Helt utanför skärmen = hoppa över.
static void drawItem(const CarouselItem* it, int xCenter, uint16_t accent) {
  if (!it) return;

  if (it->icon) {
    const int x = xCenter - UI_ICON_W / 2;
    if (x + UI_ICON_W > 0 && x < CAR_W) {
      drawBitmapMask(it->icon, UI_ICON_W, UI_ICON_H, x, CAR_ICON_Y, accent);
    }
  }

  if (it->label) {
    const int w = measureTextPx(it->label);
    const int x = xCenter - w / 2;
    if (x + w > 0 && x < CAR_W) {
      drawString(x, CAR_LABEL_Y, it->label, COLOR_GRAY_90);
    }
  }
}

void ui_renderCarousel(const CarouselItem* items, int count, int selected,
                       uint16_t accent) {
  // Före guarden nedan: annars kan en rörelse bli hängande aktiv.
  ui_animUpdate(&s_slide);
  const int offset = (int)s_slide.current;

  if (!items || count <= 0) return;
  if (selected < 0)      selected = 0;
  if (selected >= count) selected = count - 1;

  // Posten med index (selected - k) ritas på CAR_W/2 + offset - k*CAR_W.
  // Kontroll av tecknen: direkt efter ett "nästa"-steg är offset = +CAR_W, så
  // k=0 (den nya posten) hamnar utanför höger kant och k=1 (den gamla) mitt på
  // skärmen. När offseten ebbar ut står den nya i mitten och den gamla har
  // glidit ut åt vänster.
  //
  // Att indexet räknas modulo count är också det som får wrap 4->0 att glida
  // ett steg framåt istället för fyra bakåt.
  for (int k = -1; k <= 1; k++) {
    const int idx = ((selected - k) % count + count) % count;
    drawItem(&items[idx], CAR_W / 2 + offset - k * CAR_W, accent);
  }

  // Pilar och punkter ritas SIST, ovanpå det som glider.
  if (count > 1) {
    const int yc = CAR_ICON_Y + UI_ICON_H / 2;
    drawArrow(2,                       yc, true,  arrowColor(-1, accent));
    drawArrow(CAR_W - 2 - CAR_ARROW_W, yc, false, arrowColor(+1, accent));
  }

  // Punktindikator. Den fyllda punkten glider med innehållet: vid rörelsens
  // början hamnar den på den föregående punktens plats.
  const int step   = CAR_DOT_D + CAR_DOT_GAP;
  const int totalW = count * CAR_DOT_D + (count - 1) * CAR_DOT_GAP;
  const int x0     = (CAR_W - totalW) / 2;

  for (int i = 0; i < count; i++) {
    if (i == selected) continue;   // den aktiva ritas separat, se nedan
    display_drawRectOutline(x0 + i * step, CAR_DOTS_Y,
                            CAR_DOT_D, CAR_DOT_D, COLOR_GRAY_25);
  }

  const int activeX = x0 + selected * step - (offset * step) / CAR_W;
  display_fillRect(activeX, CAR_DOTS_Y, CAR_DOT_D, CAR_DOT_D, accent);
}
