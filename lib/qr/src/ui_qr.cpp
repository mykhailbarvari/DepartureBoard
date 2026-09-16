#include "ui_qr.h"
#include <qrcode.h>
#include <display.h>
#include <string.h>

// Vi går aldrig högre än version 4: på 64 px höjd blir version 5+ bara
// 1 px per modul, vilket ingen telefon läser på rimligt avstånd.
#define QR_MAX_VERSION 4

// Specen säger 4 moduler tyst zon. Det får inte plats på 64 px, och 2 räcker
// i praktiken när kontrasten är så här hög.
#define QR_QUIET 2

// (33*33 + 7) / 8 = 137 byte för version 4.
#define QR_BUF_BYTES 137
static uint8_t s_modules[QR_BUF_BYTES];

// Kapacitet i tecken för ECC_LOW, version 1-4. Biblioteket kontrollerar inte
// detta själv, så tabellen finns här.
static const uint16_t kCapNumeric[QR_MAX_VERSION]      = {  41,  77, 127, 187 };
static const uint16_t kCapAlphanumeric[QR_MAX_VERSION] = {  25,  47,  77, 114 };
static const uint16_t kCapByte[QR_MAX_VERSION]         = {  17,  32,  53,  78 };

// Samma teckenuppsättning som bibliotekets getAlphanumeric().
static bool isAlnumChar(char c) {
  if (c >= '0' && c <= '9') return true;
  if (c >= 'A' && c <= 'Z') return true;
  switch (c) {
    case ' ': case '$': case '%': case '*':
    case '+': case '-': case '.': case '/': case ':':
      return true;
  }
  return false;
}

// Minsta version som rymmer texten, eller 0.
static uint8_t pickVersion(const char* text) {
  const size_t len = strlen(text);
  if (len == 0) return 0;

  bool numeric = true, alnum = true;
  for (size_t i = 0; i < len; i++) {
    const char c = text[i];
    if (c < '0' || c > '9') numeric = false;
    if (!isAlnumChar(c))    alnum   = false;
  }

  const uint16_t* cap = numeric ? kCapNumeric
                      : alnum   ? kCapAlphanumeric
                                : kCapByte;

  for (uint8_t v = 1; v <= QR_MAX_VERSION; v++) {
    if (len <= cap[v - 1]) return v;
  }
  return 0;
}

// Modulsida i pixlar för en version som ska rymmas i maxSize. 0 = får inte plats.
static int scaleFor(uint8_t version, int maxSize) {
  const int modules = 4 * version + 17 + 2 * QR_QUIET;
  return (modules > 0) ? (maxSize / modules) : 0;
}

int ui_qrSize(const char* text, int maxSize) {
  if (!text) return 0;

  const uint8_t v = pickVersion(text);
  if (v == 0) return 0;

  const int scale = scaleFor(v, maxSize);
  if (scale < 1) return 0;

  return (4 * v + 17 + 2 * QR_QUIET) * scale;
}

bool ui_drawQR(const char* text, int xCenter, int yTop, int maxSize) {
  if (!text || !text[0]) return false;

  const uint8_t version = pickVersion(text);
  if (version == 0) return false;

  if (qrcode_getBufferSize(version) > QR_BUF_BYTES) return false;

  const int scale = scaleFor(version, maxSize);
  if (scale < 1) return false;

  QRCode qr;
  if (qrcode_initText(&qr, s_modules, version, ECC_LOW, text) != 0) return false;

  const int side = (qr.size + 2 * QR_QUIET) * scale;
  const int x0   = xCenter - side / 2;

  // Ljus botten, tysta zonen inkluderad.
  display_fillRect(x0, yTop, side, side, COLOR_WHITE);

  for (uint8_t my = 0; my < qr.size; my++) {
    for (uint8_t mx = 0; mx < qr.size; mx++) {
      if (!qrcode_getModule(&qr, mx, my)) continue;
      display_fillRect(x0   + (QR_QUIET + mx) * scale,
                       yTop + (QR_QUIET + my) * scale,
                       scale, scale, COLOR_BLACK);
    }
  }

  return true;
}
