// Directory Files
#include <logic.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <display.h>
#include <layout.h>
#include <api.h>
#include <config.h>
#include <font.h>
#include <WiFi.h>
#include <ui_carousel.h>
#include <ui_icons.h>
#include <ui_logo.h>
#include <ui_qr.h>
#include <portal.h>


// Håller ui.scrollOffset i takt med ui.selectedIndex för en lista som är
// längre än fönstret: markeringen ska alltid vara synlig, och listan ska aldrig
// scrolla förbi sin sista sida.
static void listClamp(int count, int visible) {
  if (ui.selectedIndex < 0)      ui.selectedIndex = 0;
  if (ui.selectedIndex >= count) ui.selectedIndex = count - 1;

  if (ui.selectedIndex < ui.scrollOffset)
    ui.scrollOffset = ui.selectedIndex;

  if (ui.selectedIndex >= ui.scrollOffset + visible)
    ui.scrollOffset = ui.selectedIndex - visible + 1;

  if (ui.scrollOffset < 0) ui.scrollOffset = 0;

  int maxScroll = count - visible;
  if (maxScroll < 0) maxScroll = 0;
  if (ui.scrollOffset > maxScroll) ui.scrollOffset = maxScroll;
}

Departure departures[MAX_DEPARTURES];
int departureCount = 0;


// ------------------------------------------------------- DATA-HJÄLPFUNKTIONER

// Minuterna räknas NU, inte vid hämtningen. Det är det som gör att siffran
// tickar ner mellan hämtningarna istället för att stå still i 30 sekunder.
int departure_minsUntil(const Departure* d) {
  if (!d) return 0;

  time_t now = time(nullptr);

  // Klockan inte synkad än → falla tillbaka på SL:s egen display-sträng.
  if (now <= 1000000UL || d->depEpoch == 0) {
    if (strchr(d->display, ':')) return 999;  // "14:35" = långt fram
    return atoi(d->display);
  }

  return (int)((d->depEpoch - now) / 60);
}

// Ett enda ställe för filtren, så att rendering och scroll-klampning aldrig
// kan glida isär (de var två separata kopior tidigare).
bool departure_passesFilter(const Departure* d) {
  if (!d) return false;
  if (g_settings.directionCode != 0 && d->directionCode != (uint8_t)g_settings.directionCode) return false;
  if (g_settings.walkMinutes > 0 && departure_minsUntil(d) < g_settings.walkMinutes) return false;
  return true;
}

int departures_visibleCount(void) {
  int n = 0;
  for (int i = 0; i < departureCount; i++) {
    if (departure_passesFilter(&departures[i])) n++;
  }
  return n;
}

const Departure* departures_at(int visibleIndex) {
  if (visibleIndex < 0) return nullptr;
  int n = 0;
  for (int i = 0; i < departureCount; i++) {
    if (!departure_passesFilter(&departures[i])) continue;
    if (n == visibleIndex) return &departures[i];
    n++;
  }
  return nullptr;
}



void drawRow(int row, const Departure* departure, int yOffset) {
  int y = row * Y_OFFSET + yOffset;

  const bool cancelled = (departure->state == DEP_CANCELLED);

  // ===== LINE: max 3 siffror =====
  setClipX(X_LINE_START, X_LINE_END);

  char line3[4]; // 3 siffror + NUL
  lineCode3Digits(line3, sizeof(line3), departure->line);
  drawString(X_LINE_START, y, line3, COLOR_WHITE);

  // ===== DESTINATION: fit + abbreviation =====
  setClipX(X_DEST_START, X_DEST_END);

  char destFit[64];
  int maxPx = (X_DEST_END - X_DEST_START + 1);
  fitTextToWidthPx(destFit, sizeof(destFit), departure->destination, maxPx);
  drawString(X_DEST_START, y, destFit, cancelled ? COLOR_ERROR : g_settings.colourway);

  // ===== MINUTES / TIME: right-aligned =====
  setClipX(X_MIN_START, X_MIN_END);

  int mins = departure_minsUntil(departure);

  char timeStr[8];
  uint16_t timeColor = COLOR_WHITE;

  if (cancelled) {
    strncpy(timeStr, "Inst", sizeof(timeStr));
    timeStr[sizeof(timeStr) - 1] = 0;
    timeColor = COLOR_ERROR;
  } else if (mins > 30 && departure->depTime[0]) {
    strncpy(timeStr, departure->depTime, sizeof(timeStr));
    timeStr[sizeof(timeStr) - 1] = 0;
  } else if (mins <= 0) {
    strncpy(timeStr, "Nu", sizeof(timeStr));
  } else {
    snprintf(timeStr, sizeof(timeStr), "%d min", mins);
  }

  // Försenad men inte inställd: gul tid. Tidiga avgångar lämnas orörda.
  if (!cancelled && departure->delayMin > 0) timeColor = COLOR_WARNING;

  drawTextRightAlignedInBox(X_MIN_START, X_MIN_END, y, timeStr, timeColor);

  clearClipX();

  // Störningsmarkör i vänsterkanten (x=0 är oanvänt, X_LINE_START är 1).
  if (departure->hasDeviation) {
    display_fillRect(0, y + 4, 1, 4, COLOR_WARNING);
  }
}



static void drawCentered(int y, const char* msg, uint16_t color) {
  int w = measureTextPx(msg);
  drawString((128 - w) / 2, y, msg, color);
}

// Visar VARFÖR listan är tom istället för att animera "Fetching" i all evighet.
static void renderEmptyState(void) {
  switch (g_lastFetchResult) {
    case FETCH_NO_WIFI:
      if (portal_isAp()) {
        // Oprovisionerad: visa anslutnings-QR:en istället för ett
        // felmeddelande användaren inte kan göra något åt.
        renderQR();
      } else {
        drawCentered(20, "Ingen WiFi", COLOR_ERROR);
        drawCentered(36, "Hall inne for QR", COLOR_GRAY_50);
      }
      return;

    case FETCH_HTTP_ERR:
    case FETCH_PARSE_ERR:
      drawCentered(26, "SL svarar inte", COLOR_WARNING);
      return;

    case FETCH_EMPTY:
      drawCentered(26, "Inga avgångar", COLOR_GRAY_50);
      return;

    case FETCH_PENDING:
    default: {
      // Har ännu inte fått något svar — animera medan vi väntar.
      static const char* const dotFrames[] = { "", ".", "..", "..." };
      int frame = (int)(millis() / 400) % 4;
      const char* label = "Hämtar";
      int labelW = measureTextPx(label);
      int x = (128 - labelW) / 2;
      drawString(x,              26, label,             COLOR_GRAY_50);
      drawString(x + labelW + 1, 26, dotFrames[frame],  COLOR_GRAY_50);
      return;
    }
  }
}

// Statusraden visas BARA när den har något att säga — annars får avgångarna
// använda alla fem raderna. Returnerar false när allt är i sin ordning.
static bool buildStatusLine(char* out, size_t cap, uint16_t* color) {
  uint32_t ageMin = 0;
  bool haveAge = false;

  if (g_lastSuccessMs != 0) {
    ageMin  = (millis() - g_lastSuccessMs) / 60000UL;
    haveAge = true;
  }

  switch (g_lastFetchResult) {
    case FETCH_NO_WIFI:
      *color = COLOR_ERROR;
      snprintf(out, cap, "Ingen WiFi");
      return true;

    case FETCH_HTTP_ERR:
    case FETCH_PARSE_ERR:
      *color = COLOR_WARNING;
      if (haveAge && ageMin > 0) snprintf(out, cap, "SL: fel, %lu min", (unsigned long)ageMin);
      else                       snprintf(out, cap, "SL svarar inte");
      return true;

    default:
      break;
  }

  // Svaren går igenom, men datan har hunnit bli gammal.
  if (haveAge && ageMin >= STALE_MINUTES) {
    *color = COLOR_WARNING;
    snprintf(out, cap, "%lu min gammal", (unsigned long)ageMin);
    return true;
  }

  return false;
}

// Antal avgangsrader som faktiskt far plats just nu. Statusraden stjal en,
// och bade renderingen och scroll-klampningen maste rakna med samma tal.
int departures_rowCapacity(void) {
  char buf[32];
  uint16_t c;
  return buildStatusLine(buf, sizeof(buf), &c) ? (ROWS - 1) : ROWS;
}


void renderMainFromArray(int startIndex) {
  // Uppdatera FÖRE de tidiga returerna nedan. Annars slutar animationen aldrig
  // när listan är tom, och DisplayTask ritar om i 20 ms-takt i all evighet.
  ui_animUpdate(&ui.scrollAnim);
  const int slide = (int)ui.scrollAnim.current;

  if (departureCount == 0) {
    renderEmptyState();
    return;
  }

  int visible = departures_visibleCount();
  if (visible == 0) {
    drawCentered(26, "Inga avgångar i tid", COLOR_GRAY_50);
    return;
  }

  char status[32];
  uint16_t statusColor = COLOR_WARNING;
  const bool showStatus = buildStatusLine(status, sizeof(status), &statusColor);

  // Statusraden lägger beslag på radplats 0 och trycker ner avgångarna.
  const int yOffset  = showStatus ? Y_OFFSET : 0;
  const int rowCount = departures_rowCapacity();

  // Under en glidning behövs en extra rad i var ände, annars uppstår ett tomt
  // band där innehållet kommit ifrån respektive är på väg.
  const int extra = (slide != 0) ? 1 : 0;

  for (int row = -extra; row < rowCount + extra; row++) {
    const int i = startIndex + row;
    if (i < 0 || i >= visible) continue;

    const Departure* d = departures_at(i);
    if (!d) continue;

    drawRow(row, d, yOffset + slide);
  }


  // Statusraden ritas SIST och med egen svart botten: biblioteket klipper bara
  // i x-led, så en rad som glider uppåt ritar annars rakt in i den här ytan.
  if (showStatus) {
    display_fillRect(0, 0, 128, Y_OFFSET, COLOR_BLACK);
    setClipX(0, 126);
    drawString(1, 0, status, statusColor);
    clearClipX();
  }
}



void renderBoot(void) {
  drawBitmapAlpha(ui_logo, UI_LOGO_W, UI_LOGO_H,
                  (128 - UI_LOGO_W) / 2, (64 - UI_LOGO_H) / 2,
                  g_settings.colourway);
}

// ============================ KARUSELLMENYN ============================
UiState ui = {
  .state = STATE_BOOT,  // Nuvarande State
  .selectedIndex = 0,   // Index som vi selectar
  .scrollOffset = 0,    // Hjälper Scroll
  .dirty = true,        // Behövs ritas om
  .bootStartMs = 0
};

// Avgångsskärmen är hem och är därför INTE ett menyval.
//
// "Hållplats" är tillfällig: gångtid och riktning flyttar till webbportalen
// i fas 4, men tills portalen finns är detta enda sättet att ställa dem.
// Ta bort posten (och STATE_STATION*) när portalen är i mål.
static const CarouselItem kMenuItems[] = {
  { icon_list_24,       "Avgångar"   },
  { icon_brightness_24, "Ljusstyrka" },
  { icon_palette_24,    "Färgtema"   },
  { icon_pin_24,        "Hållplats"  },   // TILLFÄLLIG — se ovan
  { icon_wifi_24,       "Nätverk"    },
  { icon_gear_24,       "System"     },
};
const int kMenuItemCount = (int)(sizeof(kMenuItems) / sizeof(kMenuItems[0]));

// Färgteman. EN sanningskälla: webbportalen hämtar den här listan via
// /api/settings och bygger sin meny av den, i stället för att ha en egen
// hårdkodad kopia av RGB565-värdena. Kopior glider isär — turkos skickades
// en gång som 16576 när rätt värde var 18202, och temat matchade inte.
const Theme kThemes[] = {
  { COLOR_ORANGE,     "Orange" },
  { COLOR_TURQUOISE,  "Turkos" },
  { COLOR_DARK_GREEN, "Grön"   },
  { COLOR_INFO,       "Blå"    },
  { COLOR_RED,        "Röd"    },
  { COLOR_AMBER,      "Gul"    },
  { COLOR_PINK,       "Rosa"   },
  { COLOR_WHITE,      "Vit"    },
};
const int kThemeCount = (int)(sizeof(kThemes) / sizeof(kThemes[0]));

// Temamenyns layout. Behövs både av renderaren och av stegningen nedan, så den
// bor på filnivå. Fonten är 12 px och skärmen 64, så fyra rader ryms under
// rubriken.
#define THEME_VISIBLE 4
#define THEME_ROW_H   12
#define THEME_TOP     14   // 2 px luft under rubriken

// Markeringens rad räknat från fönstrets överkant.
static int themeScreenRow(void) {
  return ui.selectedIndex - ui.scrollOffset;
}

void themeMenuOpen(uint16_t currentColour) {
  int idx = 0;
  for (int i = 0; i < kThemeCount; i++) {
    if (kThemes[i].colour == currentColour) { idx = i; break; }
  }

  ui.selectedIndex = idx;
  ui.scrollOffset  = 0;
  listClamp(kThemeCount, THEME_VISIBLE);

  ui_animReset(&ui.scrollAnim);
  ui_animReset(&ui.selectAnim);
}

// Ett steg flyttar ANTINGEN markeringen inuti fönstret ELLER hela fönstret —
// aldrig båda. Därför startas exakt en av de två rörelserna per vridning.
void themeMenuStep(int delta) {
  if (delta == 0) return;

  const int beforeRow    = themeScreenRow();
  const int beforeScroll = ui.scrollOffset;

  ui.selectedIndex += delta;
  listClamp(kThemeCount, THEME_VISIBLE);

  const int rowDelta    = themeScreenRow() - beforeRow;
  const int scrollDelta = ui.scrollOffset - beforeScroll;

  if (scrollDelta != 0) {
    // Fönstret flyttade sig: innehållet ska glida, markeringen åker med sin rad.
    ui_animNudge(&ui.scrollAnim, scrollDelta * THEME_ROW_H, THEME_ROW_H);
  } else if (rowDelta != 0) {
    // Markeringen bytte rad inuti fönstret.
    ui_animNudge(&ui.selectAnim, rowDelta * THEME_ROW_H, THEME_ROW_H);
  }
}

uint16_t themeMenuColour(void) {
  if (ui.selectedIndex < 0 || ui.selectedIndex >= kThemeCount) {
    return g_settings.colourway;
  }
  return kThemes[ui.selectedIndex].colour;
}

void renderMainMenu(void) {
  ui_renderCarousel(kMenuItems, kMenuItemCount, ui.selectedIndex, g_settings.colourway);
}

void mainMenuWrap() {
  if (ui.selectedIndex < 0)              ui.selectedIndex = kMenuItemCount - 1;
  if (ui.selectedIndex >= kMenuItemCount) ui.selectedIndex = 0;
  ui.scrollOffset = 0;  // karusellen scrollar aldrig
}

void mainMenuStep(int delta) {
  if (delta == 0) return;
  ui.selectedIndex += delta;
  mainMenuWrap();
  ui_carouselNudge(delta);
}

// ------------------------------------------------------- GEMENSAM SKÄRMHJÄLP

static void drawTitle(const char* title) {
  const int w = measureTextPx(title);
  drawString((128 - w) / 2, 1, title, g_settings.colourway);
}

// ---------------------------------------------------------------- LJUSSTYRKA

void renderBrightness(void) {
  drawTitle("Ljusstyrka");

  const int BAR_X = 12, BAR_Y = 24, BAR_W = 104, BAR_H = 14;

  const uint8_t b = display_getBrightness();
  int fillW = (int)((uint32_t)b * BAR_W / 255);
  if (fillW > BAR_W) fillW = BAR_W;

  display_drawRectOutline(BAR_X - 1, BAR_Y - 1, BAR_W + 2, BAR_H + 2, COLOR_GRAY_25);
  if (fillW > 0) display_fillRect(BAR_X, BAR_Y, fillW, BAR_H, g_settings.colourway);

  // Fonten saknar '%', så siffran står ensam under stapeln.
  char val[8];
  snprintf(val, sizeof(val), "%d", (int)((uint32_t)b * 100 / 255));
  const int w = measureTextPx(val);
  drawString((128 - w) / 2, 46, val, COLOR_GRAY_90);
}

// ------------------------------------------------------------------ FÄRGTEMA

void renderColourwayMenu(void) {
  ui_animUpdate(&ui.scrollAnim);
  ui_animUpdate(&ui.selectAnim);

  const int listSlide   = (int)ui.scrollAnim.current;
  const int selectSlide = (int)ui.selectAnim.current;

  // Under en glidning behövs en extra rad i var ände, annars uppstår ett tomt
  // band där innehållet kommit ifrån respektive är på väg.
  const int extra = (listSlide != 0) ? 1 : 0;

  for (int row = -extra; row < THEME_VISIBLE + extra; row++) {
    const int i = ui.scrollOffset + row;
    if (i < 0 || i >= kThemeCount) continue;

    const int  y   = THEME_TOP + row * THEME_ROW_H + listSlide;
    const bool sel = (ui.selectedIndex == i);

    display_fillRect(11, y + 2, 6, 6, kThemes[i].colour);
    drawString(22, y, kThemes[i].name, sel ? kThemes[i].colour : COLOR_GRAY_50);
  }

  // Markeringen är pinnad mot fönstret och får därför INTE listSlide: när
  // listan scrollar står den still och raderna glider under den, som i vilken
  // scrollande meny som helst. Bara selectSlide rör den, alltså när den byter
  // rad inuti fönstret.
  const int selY = THEME_TOP + themeScreenRow() * THEME_ROW_H + selectSlide;
  display_fillRect(3, selY + 3, 4, 4, themeMenuColour());

  // Rubriken ritas SIST med egen svart botten: biblioteket klipper bara i
  // x-led, så en rad som glider uppåt ritar annars rakt in i den här ytan.
  display_fillRect(0, 0, 128, THEME_TOP, COLOR_BLACK);
  drawTitle("Färgtema");
}


// ------------------------------------------------------------------- NÄTVERK

// QR-koden till webbportalen, centrerad.
//
// I AP-läge är koden en WIFI:-sträng, så att skanna den ANSLUTER telefonen till
// enhetens nät direkt — varpå captive portal öppnar konfigurationssidan av sig
// själv. Den koden är version 2 (58 px) och fyller höjden, så den får ingen
// text under sig; det finns ändå ingen IP att visa.
//
// I STA-läge är den en URL till LAN-adressen. Versaler med flit: det håller
// koden i alfanumeriskt läge och därmed i version 1 (50 px), vilket lämnar
// plats åt IP-raden. URL:ers schema och värdnamn är skiftlägesokänsliga.
void renderQR(void) {
  char payload[64];

  if (portal_isAp()) {
    snprintf(payload, sizeof(payload), "WIFI:T:nopass;S:%s;;", portal_apSsid());
    if (!ui_drawQR(payload, 64, 3, 58)) {
      drawCentered(26, "QR fel", COLOR_ERROR);
    }
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    drawCentered(26, "Ansluter...", COLOR_WARNING);
    return;
  }

  const String ip = WiFi.localIP().toString();
  snprintf(payload, sizeof(payload), "HTTP://%s", ip.c_str());

  // Storleken hämtas i förväg så IP-raden hamnar rätt även om koden skulle
  // behöva en högre version än väntat.
  const int size = ui_qrSize(payload, 50);
  if (size == 0 || !ui_drawQR(payload, 64, 0, 50)) {
    drawCentered(26, "QR fel", COLOR_ERROR);
    return;
  }

  char buf[24];
  fitTextToWidthPx(buf, sizeof(buf), ip.c_str(), 124);
  const int w = measureTextPx(buf);
  drawString((128 - w) / 2, size + 1, buf, COLOR_GRAY_50);
}

// -------------------------------------------------------------------- SYSTEM

void renderSystem(void) {
  drawTitle("System");

  char buf[40];

  const uint32_t up = millis() / 1000UL;
  snprintf(buf, sizeof(buf), "Uppe %luh %lum",
           (unsigned long)(up / 3600UL), (unsigned long)((up / 60UL) % 60UL));
  drawString(2, 18, buf, COLOR_GRAY_90);

  snprintf(buf, sizeof(buf), "Heap %luk",
           (unsigned long)(ESP.getFreeHeap() / 1024UL));
  drawString(2, 32, buf, COLOR_GRAY_90);

  snprintf(buf, sizeof(buf), "SL %s", api_fetchResultName(g_lastFetchResult));
  drawString(2, 46, buf,
             (g_lastFetchResult == FETCH_OK || g_lastFetchResult == FETCH_UNCHANGED)
               ? COLOR_OK : COLOR_WARNING);
}

// ------------------------------------------- HÅLLPLATS (tillfällig, se fas 4)

void renderStation(int navIndex, bool walkEditing, int directionCode, int walkMinutes) {
  drawTitle("Hållplats");

  const char* labels[3] = { "Gångtid", "Riktning", "Spara" };

  for (int i = 0; i < 3; i++) {
    const int y = 17 + i * 15;
    const bool sel = (!walkEditing && navIndex == i);
    const uint16_t c = sel ? g_settings.colourway : COLOR_GRAY_50;

    if (sel) display_fillRect(4, y + 4, 4, 4, g_settings.colourway);
    drawString(12, y, labels[i], c);
  }

  char buf[16];

  // Gångtid
  if (walkMinutes == 0) snprintf(buf, sizeof(buf), "Av");
  else                  snprintf(buf, sizeof(buf), "%d min", walkMinutes);
  drawTextRightAlignedInBox(70, 126, 17, buf,
                            walkEditing ? g_settings.colourway : COLOR_GRAY_90);

  // Riktning
  if (directionCode == 0)      snprintf(buf, sizeof(buf), "Båda");
  else                         snprintf(buf, sizeof(buf), "%d", directionCode);
  drawTextRightAlignedInBox(70, 126, 32, buf, COLOR_GRAY_90);
}
