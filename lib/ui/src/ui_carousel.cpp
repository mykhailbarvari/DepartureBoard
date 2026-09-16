#include "ui_carousel.h"
#include "ui_icons.h"
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

// Liten triangel: spetsen är smalast, basen 7 px hög.
static void drawArrow(int x, int yc, bool pointsLeft, uint16_t color) {
  for (int k = 0; k < CAR_ARROW_W; k++) {
    const int h   = 2 * k + 1;
    const int col = pointsLeft ? (x + k) : (x + (CAR_ARROW_W - 1) - k);
    display_fillRect(col, yc - k, 1, h, color);
  }
}

void ui_renderCarousel(const CarouselItem* items, int count, int selected,
                       uint16_t accent) {
  if (!items || count <= 0) return;
  if (selected < 0)      selected = 0;
  if (selected >= count) selected = count - 1;

  const CarouselItem* it = &items[selected];

  if (it->icon) {
    drawBitmapMask(it->icon, UI_ICON_W, UI_ICON_H,
                   (CAR_W - UI_ICON_W) / 2, CAR_ICON_Y, accent);
  }

  if (it->label) {
    const int w = measureTextPx(it->label);
    drawString((CAR_W - w) / 2, CAR_LABEL_Y, it->label, COLOR_GRAY_90);
  }

  // Karusellen wrappar, så pilarna visas alltid — de säger "vrid på ratten",
  // inte "det finns fler åt just det hållet".
  if (count > 1) {
    const int yc = CAR_ICON_Y + UI_ICON_H / 2;
    drawArrow(2,                        yc, true,  COLOR_GRAY_25);
    drawArrow(CAR_W - 2 - CAR_ARROW_W,  yc, false, COLOR_GRAY_25);
  }

  // Punktindikator: visar var i karusellen man är.
  const int step   = CAR_DOT_D + CAR_DOT_GAP;
  const int totalW = count * CAR_DOT_D + (count - 1) * CAR_DOT_GAP;
  int x = (CAR_W - totalW) / 2;

  for (int i = 0; i < count; i++) {
    if (i == selected) {
      display_fillRect(x, CAR_DOTS_Y, CAR_DOT_D, CAR_DOT_D, accent);
    } else {
      display_drawRectOutline(x, CAR_DOTS_Y, CAR_DOT_D, CAR_DOT_D, COLOR_GRAY_25);
    }
    x += step;
  }
}
