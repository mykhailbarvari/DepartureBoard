// Directory Files
#include <logic.h>
#include <string.h>
#include <stdio.h>
#include <display.h>
#include <layout.h>
#include <api.h>
#include <config.h>
#include <font.h>


Departure departures[MAX_DEPARTURES];
int departureCount = 0;


void drawRow(int row, const Departure* departure) {
  int y = row * Y_OFFSET;

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
  drawString(X_DEST_START, y, destFit, COLOR_ORANGE);

  // ===== MINUTES: right-aligned =====
  setClipX(X_MIN_START, X_MIN_END);
  drawTextRightAlignedInBox(
      X_MIN_START,
      X_MIN_END,
      y,
      departure->display,
      COLOR_WHITE
  );

  clearClipX();
}




void renderMainFromArray(int startIndex) {
  for (int row = 0; row < ROWS; row++) {
    int idx = startIndex + row;
    if (idx >= departureCount) break;
    drawRow(row, &departures[idx]);
  }
}

void renderBoot(void) { // TEMPORÄR BOOT SKÄRM. HELT HARDCODEAD. 
    beginFrame();
    drawBitmapMask(bitmapBOOT, 128, 64, 0, 0, COLOR_ORANGE);
    endFrame();
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
  drawBitmapMask(bitmap_mainMenu_MainMenu, 128, 64, 0, 0, COLOR_ORANGE);

  // Alla menytexter i grått
  drawBitmapMask(bitmap_mainMenu_Departures,     128, 64, 0, 0, COLOR_GRAY_90);
  drawBitmapMask(bitmap_mainMenu_Station,        128, 64, 0, 0, COLOR_GRAY_90);
  drawBitmapMask(bitmap_mainMenu_Brightness,        128, 64, 0, 0, COLOR_GRAY_90);
  drawBitmapMask(bitmap_mainMenu_SystemSettings, 128, 64, 0, 0, COLOR_GRAY_90);

  // Highlight (orange) – ENDAST selectable
  switch (ui.selectedIndex) {
    case 0: 
      drawBitmapMask(bitmap_mainMenu_Departures,     128, 64, 0, 0, COLOR_ORANGE); 
      drawBitmapMask(bitmap_dot_6x6, 6, 6, 1, 15, COLOR_ORANGE);
      break;

    case 1: 
      drawBitmapMask(bitmap_mainMenu_Station,        128, 64, 0, 0, COLOR_ORANGE); 
      drawBitmapMask(bitmap_dot_6x6, 6, 6, 1, 28, COLOR_ORANGE);
      break;

    case 2: 
      drawBitmapMask(bitmap_mainMenu_Brightness,        128, 64, 0, 0, COLOR_ORANGE); 
      drawBitmapMask(bitmap_dot_6x6, 6, 6, 1, 41, COLOR_ORANGE);
      break;

    case 3: 
      drawBitmapMask(bitmap_mainMenu_SystemSettings, 128, 64, 0, 0, COLOR_ORANGE); 
      drawBitmapMask(bitmap_dot_6x6, 6, 6, 1, 54, COLOR_ORANGE);
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



void mainTask(void) {
    beginFrame();
    renderMainFromArray(0);
    endFrame();
}




