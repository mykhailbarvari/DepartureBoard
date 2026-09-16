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
#include <ui_qr.h>
#include <portal.h>


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


// SL:s egna linjefärger. group_of_lines är det mest specifika när det finns
// ("Tunnelbanans gröna linje", "Blåbuss", "Pendeltåg"); annars trafikslaget.
uint16_t departure_lineColor(const Departure* d) {
  if (!d) return COLOR_WHITE;

  const char* g = d->groupOfLines;
  if (g[0]) {
    if (strstr(g, "Blåbuss"))   return RGB565(  0, 122, 193);
    if (strstr(g, "gröna"))     return RGB565(  0, 152,  95);
    if (strstr(g, "röda"))      return RGB565(215,  25,  32);
    if (strstr(g, "blå"))       return RGB565(  0, 122, 193);
    if (strstr(g, "Pendeltåg")) return RGB565(220,  60, 150);
  }

  switch (d->transportMode) {
    case TMODE_BUS:   return RGB565(217,  29,  41);  // SL:s röda stadsbuss
    case TMODE_METRO: return RGB565(  0, 122, 193);
    case TMODE_TRAIN: return RGB565(220,  60, 150);
    case TMODE_TRAM:  return RGB565(230, 120,   0);
    case TMODE_SHIP:  return RGB565(  0, 170, 190);
    default:          return COLOR_WHITE;
  }
}

void drawRow(int row, const Departure* departure, int yOffset) {
  int y = row * Y_OFFSET + yOffset;

  const bool cancelled = (departure->state == DEP_CANCELLED);

  // ===== LINE: max 3 siffror, i linjens egen färg =====
  setClipX(X_LINE_START, X_LINE_END);

  char line3[4]; // 3 siffror + NUL
  lineCode3Digits(line3, sizeof(line3), departure->line);
  drawString(X_LINE_START, y, line3,
             cancelled ? COLOR_GRAY_25 : departure_lineColor(departure));

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
      const char* label = "Fetching";
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

// Tunn stapel i högerkanten: utan den syns det inte att listan fortsätter.
static void drawScrollIndicator(int yTop, int total, int visible, int offset) {
  if (total <= visible) return;

  const int X = 127;            // X_MIN_END är 126, kolumnen är ledig
  const int H = 64 - yTop;

  int thumb = (H * visible) / total;
  if (thumb < 3) thumb = 3;
  if (thumb > H) thumb = H;

  int maxOffset = total - visible;
  int y = yTop + ((maxOffset > 0) ? ((H - thumb) * offset) / maxOffset : 0);

  display_fillRect(X, yTop, 1, H,     COLOR_GRAY_10);
  display_fillRect(X, y,    1, thumb, g_settings.colourway);
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

  drawScrollIndicator(yOffset, visible, rowCount, startIndex);

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
    drawBitmapMask(loadingscreen2, 128, 64, 0, 0, g_settings.colourway);
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
  { icon_brightness_24, "Ljusstyrka" },
  { icon_palette_24,    "Färgtema"   },
  { icon_pin_24,        "Hållplats"  },   // TILLFÄLLIG — se ovan
  { icon_qr_24,         "Nätverk"    },
  { icon_gear_24,       "System"     },
};
const int kMenuItemCount = (int)(sizeof(kMenuItems) / sizeof(kMenuItems[0]));

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
  drawTitle("Färgtema");

  struct Opt { uint16_t colour; const char* name; };
  static const Opt opts[3] = {
    { COLOR_TURQUOISE,  "Turkos" },
    { COLOR_ORANGE,     "Orange" },
    { COLOR_DARK_GREEN, "Grön"   },
  };

  for (int i = 0; i < 3; i++) {
    const int y = 17 + i * 15;
    const bool sel = (ui.selectedIndex == i);

    if (sel) display_fillRect(4, y + 4, 4, 4, opts[i].colour);
    display_fillRect(14, y + 2, 9, 9, opts[i].colour);
    drawString(30, y, opts[i].name, sel ? opts[i].colour : COLOR_GRAY_50);
  }
}

// ------------------------------------------------------------------- NÄTVERK

// QR-koden till webbportalen.
//
// I AP-läge är koden en WIFI:-sträng, så att skanna den ANSLUTER telefonen
// till enhetens nät direkt — varpå captive portal öppnar konfigurationssidan
// av sig själv. Användaren behöver inte veta någonting i förväg.
//
// I STA-läge är den en URL till enhetens LAN-adress. Versaler med flit:
// det gör att QR:en kodas i alfanumeriskt läge och ryms i version 1 (21x21),
// vilket ger 2 px per modul på 64 px höjd istället för 1. URL:ers schema och
// värdnamn är skiftlägesokänsliga, så telefonen bryr sig inte.
void renderQR(void) {
  char payload[64];
  bool drawn;

  if (portal_isAp()) {
    // 31 tecken — ryms precis i version 2 (32 byte i byte-läge). Håll
    // AP-namnet kort om strängen ändras.
    snprintf(payload, sizeof(payload), "WIFI:T:nopass;S:%s;;", portal_apSsid());
    drawn = ui_drawQR(payload, 31, 3, 58);

    drawString(64, 4,  "Skanna", g_settings.colourway);
    drawString(64, 18, "for att", COLOR_GRAY_90);
    drawString(64, 32, "ansluta", COLOR_GRAY_90);
    if (!drawn) drawString(64, 46, "QR fel", COLOR_ERROR);
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    drawTitle("Nätverk");
    drawString(2, 26, "Ansluter...", COLOR_WARNING);
    return;
  }

  const String ip = WiFi.localIP().toString();

  snprintf(payload, sizeof(payload), "HTTP://%s", ip.c_str());
  drawn = ui_drawQR(payload, 31, 3, 58);

  char buf[40];

  fitTextToWidthPx(buf, sizeof(buf), WiFi.SSID().c_str(), 62);
  drawString(64, 4, buf, COLOR_GRAY_90);

  fitTextToWidthPx(buf, sizeof(buf), ip.c_str(), 62);
  drawString(64, 18, buf, COLOR_WHITE);

  snprintf(buf, sizeof(buf), "%d dBm", (int)WiFi.RSSI());
  drawString(64, 32, buf, COLOR_GRAY_50);

  if (drawn) drawString(64, 46, "Skanna", g_settings.colourway);
  else       drawString(64, 46, "QR fel", COLOR_ERROR);
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
