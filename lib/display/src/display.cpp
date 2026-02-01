#include <display.h>
#include <config.h>
#include <layout.h>
#include <stdint.h>
#include <stdbool.h>
#include <font.h>

volatile uint8_t displayBrightness = DEFAULT_BRIGHTNESS;  // Brightness

// Definierar en tom pekare för panelen. Används för tillgång till HUB75E Library
static MatrixPanel_I2S_DMA* display = nullptr;

// Clip States - Definierar maximalt antal pixlar i X-led en given sträng kan anta
static bool g_clipXEnabled = false;
static int  g_clipXMin = 0;
static int  g_clipXMax = 0;

// Aktiverar Clipping (X-led)
void setClipX(int xmin, int xmax) {
  g_clipXEnabled = true;
  g_clipXMin = xmin;
  g_clipXMax = xmax;
}

// Stänger av Clipping (X-led)
void clearClipX(void) {
  g_clipXEnabled = false;
}


// Funktion för Start-Initiering
void display_init(void) {

  // Panel Initiering (Upplösning/Kedja)
  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);

  // Initiering av våra PINS
  HUB75_I2S_CFG::i2s_pins _pins = { PIN_R1, PIN_G1, PIN_B1, PIN_R2, PIN_G2, PIN_B2, PIN_A, PIN_B, PIN_C, PIN_D, PIN_E, PIN_LAT, PIN_OE, PIN_CLK };
  mxconfig.gpio = _pins;

  // Initiering av "display" pekare. Använd "display->" för anrop av befintliga library funktioner
  display = new MatrixPanel_I2S_DMA(mxconfig);
  display->begin();           // Aktivera Panel
  display->clearScreen();     // Rensa tidigare DMA Buffer (Gammal data kan finnas kvar efter omstart)
  display->setBrightness(displayBrightness); // Initiera Ljusstyrka
}

// Funktion som rensar panel
void clearScreen(void) {
  display->clearScreen();
}


// FADE DISPLAY 

static uint8_t g_curBrightness = 0;
static uint32_t g_lastFadeMs = 0;

static void updateBrightness(uint8_t target) {
  const uint8_t STEP = 1;
  const uint32_t MS = 10;

  if (millis() - g_lastFadeMs < MS) return;
  g_lastFadeMs = millis();

  if (g_curBrightness < target) {
    g_curBrightness += STEP;
    if (g_curBrightness > target)
      g_curBrightness = target;
  } else if (g_curBrightness > target) {
    if (g_curBrightness > STEP)
      g_curBrightness -= STEP;
    else
      g_curBrightness = 0;
  }

  display->setBrightness(g_curBrightness);
}

void display_on(void) {
  updateBrightness(displayBrightness);
}

void display_off(void) {
  updateBrightness(0);
}



void beginFrame(void) {
  // Rensa den buffer vi ritar till (backbuffer)
  display->fillScreen(0);
}

void endFrame(void) {
  // Visa det vi nyss ritade (byt buffer)
  display->flipDMABuffer();
}

// Returnerar vilket index en given bokstav har, Se "font.cpp" källfilen
static int fontIndex(uint8_t letter) {
  if(letter >= 'A' && letter <= 'Z'){ // A -> Z (index 0 -> 25)
    return (letter - 'A');} // Se ASCII Tabell för beräknin

  if(letter >= 'a' && letter <= 'z'){ // a -> z (index 29 -> 54)
    return 29 + (letter - 'a');}

  if(letter >= '0' && letter <= '9'){ // 0 -> 9 (index 58 -> 67)
    return 58 + (letter - '0');}

  switch(letter) {  // Andra tecken
    case ' ': return 68;
    case '.': return 69;
    case ',': return 70;
    case ':': return 71;
    case ';': return 72;
    case '!': return 73;
    case '?': return 74;
    case '-': return 75;
    case '_': return 76;
    case '/': return 77;
    case '+': return 78;
    case '=': return 79;
    case '(': return 80;
    case ')': return 81;
    case '[': return 82;
    case ']': return 83;
    case '{': return 84;
    case '}': return 85;
    case '#': return 86;
    case '@': return 87;
    case '&': return 88;
    case 0xC5: return 26;
    case 0xC4: return 27;
    case 0xD6: return 28;
    case 0xE5: return 55;
    case 0xE4: return 56;
    case 0xF6: return 57;
    default:  return 74; // Om okänt tecken - Rita "?"
  }
}

// Funktion som beräknar bredden på en bokstav. Får fonten att bli dynamisk
static uint8_t calcGlyphWidth(int index) {
  const uint8_t CELL_H = 12; // Font 12px hög
  uint8_t left = 5;     // Minsta kolumn som används
  uint8_t right = 0;    // Maximala kolumn som används
  bool any = false;

  for (uint8_t row = 0; row < CELL_H; row++) {
    uint8_t bits = bitmapFont[index][row] & 0x1F; // Logisk AND 5 bitar
    if (!bits) continue; // Hoppar över tomma rader

    any = true; // Används för mellanslag och oskrivna tecken

    for (uint8_t col = 0; col < 5; col++) { // Loopar varje kolumn
      if (bits & (1 << (4 - col))) {        // Kollar om bit tänd
        if (col < left) left = col;         // Beräknar längden. Första Pixel -> Sista Pixel
        if (col > right) right = col;
      }
    }
  }
  if (!any) return 2;               // Om SPACE/Okänt tecken, Rita 2px
  return (right - left + 1);        // Faktisk bredd (1..5)
}

// Hjälpfunktion för att konvertera UTF-8 byte till token
static uint8_t utf8ToToken(uint8_t b2) {
  switch (b2) {
    case 0x85: return 0xC5; // Å
    case 0x84: return 0xC4; // Ä
    case 0x96: return 0xD6; // Ö
    case 0xA5: return 0xE5; // å
    case 0xA4: return 0xE4; // ä
    case 0xB6: return 0xF6; // ö
    default:   return '?';
  }
}

int measureTextPx(const char* s) {
  int w = 0;

  while (*s) {
    uint8_t b1 = (uint8_t)*s;

    // UTF-8: svenska bokstäver kommer som 0xC3 + en till byte
    if (b1 == 0xC3) {
      uint8_t b2 = (uint8_t)*(s + 1);
      if (b2 == 0) break;

      uint8_t token = utf8ToToken(b2);

      int idx = fontIndex(token);
      w += calcGlyphWidth(idx) + 1;

      s += 2; // hoppa över båda bytes
      continue;
    }

    // Vanlig 1-byte (ASCII)
    int idx = fontIndex(b1);
    w += calcGlyphWidth(idx) + 1;
    s++;
  }

  if (w > 0) w -= 1; // ta bort sista GAP
  return w;
}


// ------------------------------------------------------------------------------------------------------------------ ABBREVIATION FUNKTIONER BÖRJAR
typedef struct {
  const char* from;
  const char* to;
} Abbrev;

// Lägg dina vanligaste/längsta först
static const Abbrev kAbbrevs[] = {
  {"Gullmarsplan", "Gullmarspln"},
  {"centrum", "C"},
  {"T-Centralen",  "T-C"},
  {"Centralen",    "C"},
  {"Station",      "Stn"},
  {"Terminal",     "Term"},
  {"Sjukhus",      "Sjukh."},
  {"Universitetet","Univ."},
  {" via ",        " v. "},
  {" mot ",        " m. "},
};
static const int kAbbrevCount = sizeof(kAbbrevs) / sizeof(kAbbrevs[0]);

static bool replaceAll(char* s, size_t cap, const char* from, const char* to) {
  if (!s || !from || !to) return false;
  size_t fromLen = strlen(from);
  if (fromLen == 0) return false;

  char tmp[128];
  tmp[0] = '\0';

  const char* p = s;
  char* out = tmp;
  size_t left = sizeof(tmp) - 1;
  bool changed = false;

  while (*p && left > 0) {
    const char* hit = strstr(p, from);
    if (!hit) {
      size_t n = strnlen(p, left);
      memcpy(out, p, n);
      out += n; left -= n;
      break;
    }

    // copy before hit
    size_t n = (size_t)(hit - p);
    if (n > left) n = left;
    memcpy(out, p, n);
    out += n; left -= n;

    // copy replacement
    size_t toLen = strlen(to);
    if (toLen > left) break;
    memcpy(out, to, toLen);
    out += toLen; left -= toLen;

    p = hit + fromLen;
    changed = true;
  }

  *out = '\0';
  strncpy(s, tmp, cap - 1);
  s[cap - 1] = '\0';
  return changed;
}

static void shrinkLastWordToFit(char* s, size_t cap, int maxPx) {
  if (!s || !s[0]) return;

  char* lastSpace = strrchr(s, ' ');
  char* word = lastSpace ? (lastSpace + 1) : s;

  if (*word == '\0') return;

  int wLen = (int)strlen(word);

  // Vi använder "." som markör för att ordet är kapat
  while (wLen > 1) {
    char cand[128];
    cand[0] = '\0';

    if (lastSpace) {
      size_t prefixLen = (size_t)(word - s); // inkl space
      if (prefixLen >= sizeof(cand)) prefixLen = sizeof(cand) - 1;
      memcpy(cand, s, prefixLen);
      cand[prefixLen] = '\0';
    }

    strncat(cand, word, (size_t)wLen);
    strncat(cand, ".", sizeof(cand) - strlen(cand) - 1);

    if (measureTextPx(cand) <= maxPx) {
      strncpy(s, cand, cap - 1);
      s[cap - 1] = '\0';
      return;
    }
    wLen--;
  }

  // extrem fallback
  if (lastSpace) {
    *word = '\0';
    strncat(s, ".", cap - strlen(s) - 1);
  } else {
    s[1] = '\0';
    strncat(s, ".", cap - strlen(s) - 1);
  }
}

// HUVUDFUNKTION: passar in text i maxPx genom:
// 1) om den ryms: klart
// 2) applicera kända abbreviations
// 3) korta sista ordet tills det ryms
void fitTextToWidthPx(char* out, size_t outCap, const char* in, int maxPx) {
  if (!out || outCap == 0) return;
  out[0] = '\0';
  if (!in) return;

  strncpy(out, in, outCap - 1);
  out[outCap - 1] = '\0';

  if (measureTextPx(out) <= maxPx) return;

  // Steg 1: kända replacements
  for (int i = 0; i < kAbbrevCount; i++) {
    bool changed = replaceAll(out, outCap, kAbbrevs[i].from, kAbbrevs[i].to);
    if (changed && measureTextPx(out) <= maxPx) return;
  }

  // Steg 2: korta sista ordet
  if (measureTextPx(out) > maxPx) {
    shrinkLastWordToFit(out, outCap, maxPx);
  }

  // Sista sista fallback: klipp tecken tills det ryms
  while (measureTextPx(out) > maxPx && strlen(out) > 0) {
    out[strlen(out) - 1] = '\0';
  }
}


void lineCode3Digits(char* out, size_t outCap, const char* in) {
  if (!out || outCap == 0) return;
  out[0] = '\0';
  if (!in) return;

  int n = 0;
  for (int i = 0; in[i] != '\0' && n < 3; i++) {
    char c = in[i];
    if (c >= '0' && c <= '9') {
      out[n++] = c;
    }
  }
  out[n] = '\0';
}

// ------------------------------------------------------------------------------------------------------------------ ABBREVIATION FUNKTIONER SLUTAR









// PRIVAT - Rita bokstav/tecken (BOTTOM-ALIGNED)
// returnerar hur mycket X ska flyttas (advance)
static int drawLetter(int x, int y, uint8_t letter, uint16_t color) {
  const uint8_t CELL_H  = 12;
  const uint8_t GAP     = 1;   // <- alltid 1 px mellan bokstäver
  int index = fontIndex(letter);
  uint8_t left = 5, right = 0;
  bool any = false;
  // hitta tight left/right
  for (uint8_t row = 0; row < CELL_H; row++) {
    uint8_t bits = bitmapFont[index][row] & 0x1F;
    if (!bits) continue;
    any = true;
    for (uint8_t col = 0; col < 5; col++) {
      if (bits & (1 << (4 - col))) {
        if (col < left) left = col;
        if (col > right) right = col;
      }
    }
  }

  // SPACE / helt tom glyph
  if (!any) return 2 + GAP;
  uint8_t width = (right - left + 1);
  // rita (packa X)
  for (uint8_t row = 0; row < CELL_H; row++) {
    uint8_t bits = bitmapFont[index][row] & 0x1F;
    for (uint8_t col = left; col <= right; col++) {
      if (bits & (1 << (4 - col))) {
        int px = x + (col - left);
        int py = y + row;

          // X-clipping
          if (!g_clipXEnabled || (px >= g_clipXMin && px <= g_clipXMax)) {
          display->drawPixel(px, py, color);
          }

        }
    }
  }
  return (int)width + GAP;
}

int drawString(int x, int y, const char* text, uint16_t color) { // ÅÄÖ RETURNERAR 2 TAL
  int cx = x;
  for (int i = 0; text[i] != '\0'; i++) {
    uint8_t b1 = (uint8_t)text[i];
    // UTF-8: svenska bokstäver kommer som 0xC3 + en till byte
    if (b1 == 0xC3) {
      uint8_t b2 = (uint8_t)text[i + 1];
      if (b2 == 0) break;
      uint8_t token = utf8ToToken(b2);
      cx += drawLetter(cx, y, token, color);
      i++; // hoppa över byte #2
      continue;
    }
    // vanlig 1-byte (ASCII / din encoding)
    cx += drawLetter(cx, y, b1, color);
  }
  return cx;
}

void drawTextRightAlignedInBox(int xmin, int xmax, int y, const char* text, uint16_t color) {
  if (!text || !text[0]) return;
  int w = measureTextPx(text);
  int xStart = xmax - (w - 1);
  if (xStart < xmin) xStart = xmin;
  drawString(xStart, y, text, color);
}

// Ritar en 1-bit bitmap som en "mask":
// - bit = 1  => rita pixel med color
// - bit = 0  => rita inget (transparent)
//
// bitmap-data antas vara packad radvis, MSB först per byte (0x80..0x01).
void drawBitmapMask(const uint8_t* bitmap, int w, int h, int xOff, int yOff, uint16_t color) {
  const int bytesPerRow = (w + 7) / 8; // säkert även om w ej är delbart med 8

  for (int y = 0; y < h; y++) {
    const int rowBase = y * bytesPerRow;

    for (int x = 0; x < w; x++) {
      const int byteIndex = rowBase + (x >> 3);     // x / 8
      const uint8_t b = bitmap[byteIndex];
      const uint8_t mask = (uint8_t)(0x80 >> (x & 7)); // MSB först

      if (b & mask) {
        display->drawPixel(xOff + x, yOff + y, color);
      }
    }
  }
}










