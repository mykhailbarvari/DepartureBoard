#pragma once

// Display ( 0 -> 255 )
#define DEFAULT_BRIGHTNESS 20
#define MAX_BRIGHTNESS     255
#define MIN_BRIGHTNESS     0 // OFF

// Panelens Upplösning
#define PANEL_RES_X 64
#define PANEL_RES_Y 64

// Antal Rader * Antal Kolonner = Antalet Daisy-Chainade Paneler
#define NUM_ROWS 1
#define NUM_COLS 2
#define PANEL_CHAIN (NUM_ROWS * NUM_COLS)

// Hemligheter (WiFi-creds, site-id) - se secrets.h.example
#if !defined(__has_include) || !__has_include("secrets.h")
  #error "include/secrets.h saknas. Kopiera include/secrets.h.example till include/secrets.h och fyll i dina uppgifter."
#endif
#include "secrets.h"

// Custom PIN Defines för vår ESP32-S3 Nano (Waveshare)
#define PIN_R1   47  // D12
#define PIN_B1   38  // D11
#define PIN_R2   21  // D10
#define PIN_B2   18  // D9
#define PIN_A    17  // D8
#define PIN_C    10  // D7
#define PIN_CLK  9   // D6
#define PIN_OE   8   // D5

#define PIN_G1   11  // A4
#define PIN_G2   4   // A3
#define PIN_E    3   // A2
#define PIN_B    2   // A1
#define PIN_D    1   // A0
#define PIN_LAT  48  // D13


// Deklarerar vilka pins på brädan som är input
#define INPUT1 44   // SW1    - D0 ON/OFF

// ROTARY ENCODER
#define INPUT2 5    // SW2    - D2 (INPUT PULLUP)
#define INPUT3 6    // ENC1   - D3 (INPUT PULLUP)
#define INPUT4 7    // ENC2   - D4 (INPUT PULLUP)

#define BOOT_MS 10000