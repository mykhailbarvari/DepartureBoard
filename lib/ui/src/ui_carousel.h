#pragma once
#include <stdint.h>

// Karusellmeny: ett val i taget, stort och centrerat.
//
// Ersätter de tidigare helskärms-bitmapsen. En meny ritades förr med 4-5
// drawBitmapMask()-anrop à 128x64 px — cirka 40 000 drawPixel per frame, och
// varje etikettändring krävde en ny bitmap. Här ritas etiketten med den
// befintliga fonten och bara ikonen är en bitmap (24x24 = 576 px).
typedef struct {
  const uint8_t* icon;   // 24x24, eller nullptr för enbart text
  const char*    label;  // UTF-8, åäö fungerar
} CarouselItem;

// accent = UI:ts accentfarg (g_colourway). Skickas in sa att UI-lagret
// slipper bero pa app-logiken.
void ui_renderCarousel(const CarouselItem* items, int count, int selected,
                       uint16_t accent);
