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
  if (g_directionCode != 0 && d->directionCode != (uint8_t)g_directionCode) return false;
  if (g_walkMinutes > 0 && departure_minsUntil(d) < g_walkMinutes) return false;
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

  // ===== LINE: max 3 siffror =====
  setClipX(X_LINE_START, X_LINE_END);

  char line3[4]; // 3 siffror + '\0'
  lineCode3Digits(line3, sizeof(line3), departure->line);
  drawString(X_LINE_START, y, line3, COLOR_WHITE);

  // ===== DESTINATION: fit + abbreviation =====
  setClipX(X_DEST_START, X_DEST_END);

  char destFit[64];
  int maxPx = (X_DEST_END - X_DEST_START + 1);
  fitTextToWidthPx(destFit, sizeof(destFit), departure->destination, maxPx);
  drawString(X_DEST_START, y, destFit, g_colourway);

  // ===== MINUTES / TIME: right-aligned =====
  setClipX(X_MIN_START, X_MIN_END);

  int mins = departure_minsUntil(departure);

  char timeStr[8];
  if (mins > 30 && departure->depTime[0]) {
    strncpy(timeStr, departure->depTime, sizeof(timeStr));
    timeStr[sizeof(timeStr) - 1] = '\0';
  } else if (mins <= 0) {
    strncpy(timeStr, "Nu", sizeof(timeStr));
  } else {
    snprintf(timeStr, sizeof(timeStr), "%d min", mins);
  }
  drawTextRightAlignedInBox(X_MIN_START, X_MIN_END, y, timeStr, COLOR_WHITE);

  clearClipX();
}


static void drawCentered(int y, const char* msg, uint16_t color) {
  int w = measureTextPx(msg);
  drawString((128 - w) / 2, y, msg, color);
}

// Visar VARFÖR listan är tom istället för att animera "Fetching" i all evighet.
static void renderEmptyState(void) {
  switch (g_lastFetchResult) {
    case FETCH_NO_WIFI:
      drawCentered(26, "Ingen WiFi", COLOR_ERROR);
      return;

    case FETCH_HTTP_ERR:
    case FETCH_PARSE_ERR:
      drawCentered(26, "SL svarar inte", COLOR_WARNING);
      return;

    case FETCH_EMPTY:
      drawCentered(26, "Inga avgangar", COLOR_GRAY_50);
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

void renderMainFromArray(int startIndex) {
  if (departureCount == 0) {
    renderEmptyState();
    return;
  }

  int visible = departures_visibleCount();
  if (visible == 0) {
    drawCentered(26, "No departures nearby", COLOR_GRAY_50);
    return;
  }

  int row = 0;
  for (int i = startIndex; i < visible && row < ROWS; i++, row++) {
    const Departure* d = departures_at(i);
    if (!d) break;
    drawRow(row, d, (int)ui.bouncePixels);
  }
}

void renderBoot(void) {
    drawBitmapMask(loadingscreen2, 128, 64, 0, 0, g_colourway);
}

// ======== MAIN MENU =============
UiState ui = {
  .state = STATE_BOOT,  // Nuvarande State
  .selectedIndex = 0,   // Index som vi selectar
  .scrollOffset = 0,    // Hjälper Scroll
  .dirty = true,         // Behövs ritas om
  .bootStartMs = 0
};

#define MAIN_MENU_ITEMS 4  // Departures, Station, Brightness, System Settings

void renderMainMenu(void) {

  // Titel
  drawBitmapMask(bitmap_mainMenu_MainMenu, 128, 64, 0, 0, g_colourway);

  // Alla menytexter i grått
  drawBitmapMask(bitmap_mainMenu_Departures,     128, 64, 0, 0, COLOR_GRAY_90);
  drawBitmapMask(bitmap_mainMenu_Station,        128, 64, 0, 0, COLOR_GRAY_90);
  drawBitmapMask(bitmap_mainMenu_Brightness,        128, 64, 0, 0, COLOR_GRAY_90);
  drawBitmapMask(bitmap_mainMenu_SystemSettings, 128, 64, 0, 0, COLOR_GRAY_90);

  // Highlight (orange) – ENDAST selectable
  switch (ui.selectedIndex) {
    case 0:
      drawBitmapMask(bitmap_mainMenu_Departures,     128, 64, 0, 0, g_colourway);
      drawBitmapMask(bitmap_dot_6x6, 6, 6, 1, 15, g_colourway);
      break;

    case 1:
      drawBitmapMask(bitmap_mainMenu_Station,        128, 64, 0, 0, g_colourway);
      drawBitmapMask(bitmap_dot_6x6, 6, 6, 1, 28, g_colourway);
      break;

    case 2:
      drawBitmapMask(bitmap_mainMenu_Brightness,        128, 64, 0, 0, g_colourway);
      drawBitmapMask(bitmap_dot_6x6, 6, 6, 1, 41, g_colourway);
      break;

    case 3:
      drawBitmapMask(bitmap_mainMenu_SystemSettings, 128, 64, 0, 0, g_colourway);
      drawBitmapMask(bitmap_dot_6x6, 6, 6, 1, 54, g_colourway);
      break;
  }

}

void mainMenuWrap() {
  if (ui.selectedIndex < 0)
    ui.selectedIndex = MAIN_MENU_ITEMS - 1;

  if (ui.selectedIndex >= MAIN_MENU_ITEMS)
    ui.selectedIndex = 0;

  ui.scrollOffset = 0; // main menu scrollar aldrig
}

// ------------------------------------------------------------ HELPER FUNKTIONER -------------------------------------
void listClamp(int count, int visible) {
  if (ui.selectedIndex < 0) ui.selectedIndex = 0;
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



void renderStation(int navIndex, bool walkEditing, int directionCode, int walkMinutes) {
  drawBitmapMask(Station_MenuHeader, 128, 64, 0, 0, g_colourway);

  uint16_t walkLabelColor = (!walkEditing && navIndex == 0) ? g_colourway : COLOR_GRAY_90;
  uint16_t dirColor       = (!walkEditing && navIndex == 1) ? g_colourway : COLOR_GRAY_90;
  uint16_t saveColor      = (!walkEditing && navIndex == 2) ? g_colourway : COLOR_GRAY_90;

  drawBitmapMask(Station_WalkOption,      128, 64, 1, 0, walkLabelColor);
  drawBitmapMask(Station_DirectionOption, 128, 64, 1, 0, dirColor);
  drawBitmapMask(SaveAndExit,             128, 64, 0, 0, saveColor);

  if (directionCode == 1 || directionCode == 0)
    drawBitmapMask(Station_DirectionOption1, 128, 64, 0, 0, dirColor);
  if (directionCode == 2 || directionCode == 0)
    drawBitmapMask(Station_DirectionOption2, 128, 64, 0, 0, dirColor);

  char walkBuf[12];
  if (walkMinutes == 0) snprintf(walkBuf, sizeof(walkBuf), "Off");
  else                  snprintf(walkBuf, sizeof(walkBuf), "%d min", walkMinutes);
  uint16_t walkValColor = walkEditing ? g_colourway : COLOR_GRAY_90;
  drawString(38, 14, walkBuf, walkValColor);

  if (!walkEditing) {
    switch (navIndex) {
      case 0: drawBitmapMask(bitmap_dot_6x6, 6, 6, 1, 17, g_colourway); break;
      case 1: drawBitmapMask(bitmap_dot_6x6, 6, 6, 1, 30, g_colourway); break;
      case 2: drawBitmapMask(bitmap_dot_6x6, 6, 6, 1, 54, g_colourway); break;
    }
  }
}

void renderDisplayMenu(void) {
  drawBitmapMask(Display_Header,           128, 64, 0, 0, g_colourway);

  uint16_t cBrightness = (ui.selectedIndex == 0) ? g_colourway : COLOR_GRAY_90;
  uint16_t cColourway  = (ui.selectedIndex == 1) ? g_colourway : COLOR_GRAY_90;
  uint16_t cSave       = (ui.selectedIndex == 2) ? g_colourway : COLOR_GRAY_90;

  drawBitmapMask(Display_BrightnessOption, 128, 64, 0, 0, cBrightness);
  drawBitmapMask(Display_ColourwayOption,  128, 64, 0, 0, cColourway);
  drawBitmapMask(SaveAndExit,              128, 64, 0, 0, cSave);

  switch (ui.selectedIndex) {
    case 0: drawBitmapMask(bitmap_dot_6x6, 6, 6, 1, 18, g_colourway); break;
    case 1: drawBitmapMask(bitmap_dot_6x6, 6, 6, 1, 32, g_colourway); break;
    case 2: drawBitmapMask(bitmap_dot_6x6, 6, 6, 1, 54, g_colourway); break;
  }
}

void renderBrightness(void) {
  drawBitmapMask(Brightness_SettingHeader, 128, 64, 0, 0, g_colourway);

  // Inner area of the box in Brightness_SettingHeader: x=11, y=23, w=106, h=22
  const int BAR_X = 11;
  const int BAR_Y = 23;
  const int BAR_W = 106;
  const int BAR_H = 22;

  uint8_t brightness = display_getBrightness();
  int fillW = (int)((uint32_t)brightness * BAR_W / 255);
  if (fillW > 0) {
    display_fillRect(BAR_X, BAR_Y, fillW, BAR_H, g_colourway);
  }
}

void renderColourwayMenu(void) {
  drawBitmapMask(Colourway_SettingsHeader, 128, 64, 0, 0, g_colourway);

  uint16_t cTurquoise = (ui.selectedIndex == 0) ? COLOR_TURQUOISE : COLOR_GRAY_90;
  uint16_t cOrange    = (ui.selectedIndex == 1) ? COLOR_ORANGE    : COLOR_GRAY_90;
  uint16_t cGreen     = (ui.selectedIndex == 2) ? COLOR_DARK_GREEN : COLOR_GRAY_90;

  drawBitmapMask(Colourway_TurquioseOption, 128, 64, 0, 0, cTurquoise);
  drawBitmapMask(Colourway_OrangeOption,    128, 64, 0, 0, cOrange);
  drawBitmapMask(Colourway_GreenOption,     128, 64, 0, 0, cGreen);

  switch (ui.selectedIndex) {
    case 0: drawBitmapMask(bitmap_dot_6x6, 6, 6, 1, 18, COLOR_TURQUOISE);  break;
    case 1: drawBitmapMask(bitmap_dot_6x6, 6, 6, 1, 32, COLOR_ORANGE);     break;
    case 2: drawBitmapMask(bitmap_dot_6x6, 6, 6, 1, 46, COLOR_DARK_GREEN); break;
  }
}

void renderSystemSettings(void) {
  drawBitmapMask(SystemSettings_MenuHeader, 128, 64, 0, 0, g_colourway);
  drawBitmapMask(SystemSettings_WiFiOption, 128, 64, 0, 0, g_colourway);

  // SSID till höger om WiFi-bitmapen (y=16, x=38)
  if (WiFi.status() == WL_CONNECTED) {
    char ssidBuf[32];
    fitTextToWidthPx(ssidBuf, sizeof(ssidBuf), WiFi.SSID().c_str(), 128 - 38);
    drawString(38, 16, ssidBuf, COLOR_WHITE);
  } else {
    drawString(38, 16, "NOT CONNECTED", COLOR_ERROR);
  }
}

void mainTask(void) {
    beginFrame();
    renderMainFromArray(0);
    endFrame();
}
