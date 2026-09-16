#pragma once
#include <stdint.h>

// 24x24 1-bits ikoner för karusellmenyn.
// GENERERADE av tools/gen_icons.py — ändra generatorn, inte ui_icons.cpp.
// Packade radvis MSB först, så drawBitmapMask() kan rita dem direkt.

#define UI_ICON_W 24
#define UI_ICON_H 24
#define UI_ICON_BYTES (((UI_ICON_W + 7) / 8) * UI_ICON_H)  // 3 * 24 = 72

extern const uint8_t icon_brightness_24[UI_ICON_BYTES];
extern const uint8_t icon_palette_24[UI_ICON_BYTES];
extern const uint8_t icon_pin_24[UI_ICON_BYTES];
extern const uint8_t icon_wifi_24[UI_ICON_BYTES];
extern const uint8_t icon_train_24[UI_ICON_BYTES];
extern const uint8_t icon_gear_24[UI_ICON_BYTES];
