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

// WiFi configuration
#define WIFI_SSID "***BORTTAGET***"
#define WIFI_PASSWORD "***BORTTAGET***"

// API configuration
#define API_KEY "" // Behövs inte än

#define SITE_ID 8032

// Custom PIN Defines för vår ESP32-S3 Nano (Waveshare)
#define PIN_R1   5  // D2
#define PIN_G1   6  // D3
#define PIN_B1   7  // D4
#define PIN_R2   8  // D5
#define PIN_G2   9  // D6
#define PIN_B2   10 // D7
#define PIN_A    17 // D8
#define PIN_B    18 // D9
#define PIN_C    21 // D10
#define PIN_D    1  // A0
#define PIN_E    2  // A1
#define PIN_CLK  3  // A2
#define PIN_LAT  4  // A3
#define PIN_OE   13 // A6

// Deklarerar vilka pins på brädan som är input
#define INPUT1 12 // SVART - GPIO12 A5 (INPUT PULLUP)
#define INPUT2 11 // BLÅ   - GPIO11 A4 (INPUT PULLUP)
#define INPUT3 44 // RÖD   - GPIO44 D0 (INPUT PULLUP)
#define INPUT4 43 // GRÖN  - GPIO43 D1 (INPUT PULLUP)

#define BOOT_MS 10000



