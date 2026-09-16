#pragma once
#include <stdint.h>
#include <stdbool.h>

// Karusellmeny: ett val i taget, stort och centrerat, med en glidande övergång
// mellan posterna.
//
// Ersätter de tidigare helskärms-bitmapsen. En meny ritades förr med 4-5
// drawBitmapMask()-anrop à 128x64 px — cirka 40 000 drawPixel per frame, och
// varje etikettändring krävde en ny bitmap. Här ritas etiketten med den
// befintliga fonten och bara ikonen är en bitmap (24x24 = 576 px).
typedef struct {
  const uint8_t* icon;   // 24x24, eller nullptr för enbart text
  const char*    label;  // UTF-8, åäö fungerar
} CarouselItem;

// accent = UI:ts accentfärg (g_settings.colourway). Skickas in så att UI-lagret
// slipper bero på app-logiken.
void ui_renderCarousel(const CarouselItem* items, int count, int selected,
                       uint16_t accent);

// Startar den glidande övergången. Anropas när det valda indexet just ändrats.
// dir = +1 för nästa post, -1 för föregående.
void ui_carouselNudge(int dir);

// Nollställer förskjutningen direkt. Anropas när menyn öppnas, så en gammal
// rörelse inte ligger kvar och ger en oväntad glidning vid inträdet.
void ui_carouselReset(void);

// true medan en övergång pågår. Renderloopen använder den för att fortsätta
// rita utan att någon behöver hålla dirty-flaggan satt.
bool ui_carouselAnimating(void);
