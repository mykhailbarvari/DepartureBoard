#pragma once

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <stdint.h>
#include <config.h>
#include <stddef.h>



// RGB565 Hjälp Makro
#define RGB565(r, g, b) ( \
  ((uint16_t)((r)&0xF8) << 8) | ((uint16_t)((g)&0xFC) << 3) | ((uint16_t)((b)&0xF8) >> 3))

// Grun
#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF
#define COLOR_RED RGB565(255, 0, 0)
#define COLOR_GREEN RGB565(0, 255, 0)
#define COLOR_BLUE RGB565(0, 0, 255)

// Sekundära
#define COLOR_YELLOW RGB565(255, 255, 0)
#define COLOR_CYAN RGB565(0, 255, 255)
#define COLOR_MAGENTA RGB565(255, 0, 255)
#define COLOR_ORANGE RGB565(255, 165, 0)
#define COLOR_PINK RGB565(255, 105, 180)
#define COLOR_PURPLE RGB565(128, 0, 128)

// Dashboard-Friendly
#define COLOR_DARK_RED RGB565(128, 0, 0)
#define COLOR_DARK_GREEN RGB565(0, 128, 0)
#define COLOR_DARK_BLUE RGB565(0, 0, 128)
#define COLOR_OLIVE RGB565(128, 128, 0)
#define COLOR_TEAL RGB565(0, 128, 128)
#define COLOR_NAVY RGB565(0, 0, 80)

// Gråskalor (UI/text)
#define COLOR_GRAY_10 RGB565(26, 26, 26)
#define COLOR_GRAY_25 RGB565(64, 64, 64)
#define COLOR_GRAY_50 RGB565(128, 128, 128)
#define COLOR_GRAY_75 RGB565(192, 192, 192)
#define COLOR_GRAY_90 RGB565(230, 230, 230)

// Status
#define COLOR_OK RGB565(0, 200, 0)
#define COLOR_WARNING RGB565(255, 180, 0)
#define COLOR_ERROR RGB565(255, 0, 0)
#define COLOR_INFO RGB565(0, 160, 255)

// Special
#define COLOR_AMBER RGB565(255, 191, 0)
#define COLOR_LIME RGB565(191, 255, 0)
#define COLOR_SKY RGB565(135, 206, 235)
#define COLOR_TURQUOISE RGB565(64, 224, 208)
#define COLOR_VIOLET RGB565(238, 130, 238)

// Funktioner från "display.cpp"
void display_init(void);
int drawString(int x, int y, const char* text, uint16_t color);
void clearScreen(void);
int measureTextPx(const char* s);
void setClipX(int xmin, int xmax);
void clearClipX(void);
void drawTextRightAlignedInBox(int xmin, int xmax, int y, const char* text, uint16_t color);
void beginFrame(void);
void endFrame(void);
void display_on(void);
void display_off(void);
void fitTextToWidthPx(char* out, size_t outCap, const char* in, int maxPx);
void lineCode3Digits(char* out, size_t outCap, const char* in);
void drawBitmapMask(const uint8_t* bitmap, int w, int h, int xOff, int yOff, uint16_t color);
void display_fillRect(int x, int y, int w, int h, uint16_t color);
void display_drawRectOutline(int x, int y, int w, int h, uint16_t color);
void display_setBrightness(uint8_t val);
uint8_t display_getBrightness(void);
void display_saveBrightness(void);
void display_stopDMA(void);
void display_resumeDMA(void);



