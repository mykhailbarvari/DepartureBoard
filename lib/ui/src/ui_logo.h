#pragma once
#include <stdint.h>

// MB Labs-logotypen, rastrerad ur MB_Labs.svg.
// GENERERAD av tools/gen_logo.py — ändra generatorn, inte ui_logo.cpp.
//
// Lagras som 4 bitars alfa (två pixlar per byte, hög nibble = jämnt x) i
// stället för 1 bit: logotypen består till stor del av diagonaler, och utan
// graderade kanter trappar de sönder vid den här storleken.
//
// Ritas med drawBitmapAlpha(), som skalar en given färg per pixel.

#define UI_LOGO_W 122
#define UI_LOGO_H 64
#define UI_LOGO_BYTES (((UI_LOGO_W + 1) / 2) * UI_LOGO_H)  // 61 * 64 = 3904

extern const uint8_t ui_logo[UI_LOGO_BYTES];
