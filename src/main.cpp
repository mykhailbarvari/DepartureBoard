#include <Arduino.h>
#include <display.h>
#include <logic.h>
#include <api.h>
#include <input.h>
#include <config.h>
#include <Preferences.h>
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "driver/gpio.h"

// Globalt Deklarerade Variabler
volatile bool displayDisabled = false;  // Togglar panel, TRUE = PANEL AV
volatile bool g_hasData = false;        // ApiTask sätter TRUE när data finns
volatile bool g_dataUpdated = false;
volatile bool g_fetching = false;       // ApiTask fetchar just nu
volatile int g_walkMinutes   = 0;   // minuter promenad — filtrerar avgångar under detta
volatile int g_directionCode = 0;   // 0 = alla, 1 = riktning 1, 2 = riktning 2
volatile uint16_t g_colourway = COLOR_ORANGE;  // Accentfärg för UI

// ------FreeRTOS------ IN PROGRESS

// -------TASKS-------
void InputTask(void *pv) {  // Pollar inputs
  (void)pv;
  for (;;) {
    input_update();                   // Kollar input status
    displayDisabled = input_onoff();  // MOMENTARY fysiskt, kommer bli switch sen 
    vTaskDelay(pdMS_TO_TICKS(1));  // Polling intervall
  }
}

void DisplayTask(void *pv) {  // ENDAST FÖR RENDERING
  (void)pv;
  bool wasDisabled = false;
  for (;;) {
    if (displayDisabled) {
      display_stopDMA();
      esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
      gpio_wakeup_enable((gpio_num_t)INPUT1, GPIO_INTR_HIGH_LEVEL);
      esp_sleep_enable_gpio_wakeup();
      esp_light_sleep_start();
      // Woke up — SW1 released
      esp_wifi_set_ps(WIFI_PS_NONE);
      display_resumeDMA();
      wasDisabled = true;
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    display_on();

    if (wasDisabled) {
      ui.dirty = true;
      wasDisabled = false;
    }
    if (!ui.dirty) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    // STATE_DEPARTURES: kolla mutex INNAN beginFrame så vi undviker svart flicker
    if (ui.state == STATE_DEPARTURES) {
      if (!gDeparturesMutex || xSemaphoreTake(gDeparturesMutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        // Kan inte låsa data just nu — hoppa över denna frame, gamla bilden stannar
        vTaskDelay(pdMS_TO_TICKS(50));
        continue;
      }
      beginFrame();
      renderMainFromArray(ui.scrollOffset);
      xSemaphoreGive(gDeparturesMutex);
      endFrame();
      ui.dirty = false;
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    beginFrame();

    switch (ui.state) {

      case STATE_BOOT:
        renderBoot();
        break;

      case STATE_INIT:
        // ENDAST RENDERING
        break;

      case STATE_MENU:
        renderMainMenu();
        break;

      case STATE_DEPARTURES:
        break;  // hanteras ovan

      case STATE_DISPLAY_MENU:
        renderDisplayMenu();
        break;

      case STATE_BRIGHTNESS:
        renderBrightness();
        break;

      case STATE_COLOURWAY:
        renderColourwayMenu();
        break;

      case STATE_STATION:
        renderStation(ui.scrollOffset, false, (int)g_directionCode, (int)g_walkMinutes);
        break;

      case STATE_STATION_WALKTIME:
        renderStation(0, true, (int)g_directionCode, ui.selectedIndex);
        break;

      case STATE_SYSTEM_SETTINGS:
        renderSystemSettings();
        break;
    }
    endFrame();
    ui.dirty = false;

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void ControlLogicTask(void *pv) {  // ENDAST STATE MACHINE. INGEN RENDERING SKER HÄR. BESKRIVER VILKET STATE SOM ÄR AKTIVT
  (void)pv;

  static ButtonTracker selectTracker = {false, false};

  for (;;) {
    if (displayDisabled) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    // STATE MACHINE
    switch (ui.state) {
      case STATE_BOOT:
        {

          // Första gången vi kommer in i BOOT
          if (ui.bootStartMs == 0) {
            ui.bootStartMs = millis();
            ui.dirty = true;  // ← rita boot EN gång
          }

          // Efter 3 sek → gå till menu
          if (millis() - ui.bootStartMs >= BOOT_MS) {
            ui.bootStartMs = 0;
            ui.state = STATE_DEPARTURES;
            ui.scrollOffset = 0;
            g_dataUpdated = false;
            ui.dirty = true;
          }
        }
        break;


      case STATE_INIT:
        // ENDAST LOGIK
        break;

      case STATE_MENU:
        {
          bool upPressed = input_encoderDown();
          bool downPressed = input_encoderUp();
          bool selectPressed = input_isPressed(selectTracker, input_select());

          if (upPressed) {
            ui.selectedIndex--;
            mainMenuWrap();
            ui.dirty = true;
          }

          if (downPressed) {
            ui.selectedIndex++;
            mainMenuWrap();
            ui.dirty = true;
          }

          if (selectPressed) {
            // Handle menu selection
            switch (ui.selectedIndex) {
              case 0:
                ui.state = STATE_DEPARTURES;
                ui.scrollOffset = 0;
                ui.dirty = true;
                g_dataUpdated = false;
                break;

              case 1:
                ui.state = STATE_STATION;
                ui.scrollOffset = 0;
                ui.dirty = true;
                break;

              case 2:
                ui.state = STATE_DISPLAY_MENU;
                ui.selectedIndex = 0;
                ui.dirty = true;
                break;

              case 3:
                ui.state = STATE_SYSTEM_SETTINGS;
                ui.dirty = true;
                break;
            }
          }
        }
        break;

      case STATE_DISPLAY_MENU:
        {
          bool upPressed     = input_encoderDown();
          bool downPressed   = input_encoderUp();
          bool selectPressed = input_isPressed(selectTracker, input_select());

          if (upPressed) {
            if (ui.selectedIndex > 0) { ui.selectedIndex--; ui.dirty = true; }
          }
          if (downPressed) {
            if (ui.selectedIndex < 2) { ui.selectedIndex++; ui.dirty = true; }
          }

          if (selectPressed) {
            switch (ui.selectedIndex) {
              case 0:
                ui.state = STATE_BRIGHTNESS;
                ui.dirty = true;
                break;
              case 1:
                ui.state = STATE_COLOURWAY;
                ui.selectedIndex = (g_colourway == COLOR_TURQUOISE) ? 0
                                 : (g_colourway == COLOR_DARK_GREEN) ? 2 : 1;
                ui.dirty = true;
                break;
              case 2:
                ui.state = STATE_MENU;
                ui.selectedIndex = 2;
                ui.dirty = true;
                break;
            }
          }
        }
        break;

      case STATE_COLOURWAY:
        {
          bool upPressed     = input_encoderDown();
          bool downPressed   = input_encoderUp();
          bool selectPressed = input_isPressed(selectTracker, input_select());

          if (upPressed) {
            if (ui.selectedIndex > 0) { ui.selectedIndex--; ui.dirty = true; }
          }
          if (downPressed) {
            if (ui.selectedIndex < 2) { ui.selectedIndex++; ui.dirty = true; }
          }

          if (selectPressed) {
            switch (ui.selectedIndex) {
              case 0: g_colourway = COLOR_TURQUOISE;  break;
              case 1: g_colourway = COLOR_ORANGE;     break;
              case 2: g_colourway = COLOR_DARK_GREEN; break;
            }
            Preferences prefs;
            prefs.begin("board", false);
            prefs.putUShort("colourway", (uint16_t)g_colourway);
            prefs.end();
            ui.state = STATE_DISPLAY_MENU;
            ui.selectedIndex = 1;  // Colourway-alternativet i Display-menyn
            ui.dirty = true;
          }
        }
        break;

      case STATE_DEPARTURES:
        {
          bool backPressed = input_isPressed(selectTracker, input_select());

          if (backPressed) {
            ui.state = STATE_MENU;
            ui.dirty = true;
          }

          // Räkna filtrerade avgångar för korrekt scroll-klamp
          int filteredCount = 0;
          for (int i = 0; i < departureCount; i++) {
            if (g_walkMinutes == 0 || departures[i].minsUntil >= (uint16_t)g_walkMinutes) filteredCount++;
          }
          int maxOffset = (filteredCount > ROWS) ? filteredCount - ROWS : 0;

          if (input_encoderUp()) {
            if (ui.scrollOffset < maxOffset) { ui.scrollOffset++; ui.dirty = true; }
          }
          if (input_encoderDown()) {
            if (ui.scrollOffset > 0) { ui.scrollOffset--; ui.dirty = true; }
          }

          if (g_dataUpdated) {
            g_dataUpdated = false;
            ui.dirty = true;
          }

          // Håll animationen aktiv medan data saknas eller fetch pågår
          if (g_fetching || departureCount == 0) {
            ui.dirty = true;
          }
        }
        break;

      case STATE_STATION:
        {
          bool selectPressed = input_isPressed(selectTracker, input_select());

          if (input_encoderUp()) {
            if (ui.scrollOffset < 2) { ui.scrollOffset++; ui.dirty = true; }
          }
          if (input_encoderDown()) {
            if (ui.scrollOffset > 0) { ui.scrollOffset--; ui.dirty = true; }
          }

          if (selectPressed) {
            switch (ui.scrollOffset) {
              case 0:  // Walk time-editor
                ui.selectedIndex = (int)g_walkMinutes;
                ui.state = STATE_STATION_WALKTIME;
                ui.dirty = true;
                break;

              case 1:  // Toggla direction: 0→1→2→0, spara direkt
                g_directionCode = (g_directionCode + 1) % 3;
                {
                  Preferences prefs;
                  prefs.begin("board", false);
                  prefs.putUChar("dircode", (uint8_t)g_directionCode);
                  prefs.end();
                }
                ui.dirty = true;
                break;

              case 2:  // Save & Exit
                {
                  Preferences prefs;
                  prefs.begin("board", false);
                  prefs.putUChar("walkmin", (uint8_t)g_walkMinutes);
                  prefs.end();
                }
                departureCount = 0;
                g_hasData = false;
                g_dataUpdated = false;
                ui.state = STATE_MENU;
                ui.selectedIndex = 1;
                ui.dirty = true;
                break;
            }
          }
        }
        break;

      case STATE_STATION_WALKTIME:
        {
          bool confirmPressed = input_isPressed(selectTracker, input_select());

          if (confirmPressed) {
            g_walkMinutes = ui.selectedIndex;
            Preferences prefs;
            prefs.begin("board", false);
            prefs.putUChar("walkmin", (uint8_t)g_walkMinutes);
            prefs.end();
            ui.state = STATE_STATION;
            ui.scrollOffset = 0;  // återgå till Walk-raden
            ui.dirty = true;
          }

          if (input_encoderUp()) {
            if (ui.selectedIndex < 60) { ui.selectedIndex++; ui.dirty = true; }
          }
          if (input_encoderDown()) {
            if (ui.selectedIndex > 0) { ui.selectedIndex--; ui.dirty = true; }
          }
        }
        break;

      case STATE_BRIGHTNESS:
        {
          const uint8_t STEP = 8;
          bool backPressed = input_isPressed(selectTracker, input_select());

          if (backPressed) {
            display_saveBrightness();
            ui.state = STATE_DISPLAY_MENU;
            ui.selectedIndex = 0;
            ui.dirty = true;
          }

          if (input_encoderUp()) {
            uint8_t cur = display_getBrightness();
            display_setBrightness(cur + STEP > 255 ? 255 : cur + STEP);
            ui.dirty = true;
          }
          if (input_encoderDown()) {
            uint8_t cur = display_getBrightness();
            display_setBrightness(cur > STEP ? cur - STEP : 0);
            ui.dirty = true;
          }
        }
        break;

      case STATE_SYSTEM_SETTINGS:
        {
          bool backPressed = input_isPressed(selectTracker, input_select());
          if (backPressed) {
            ui.state = STATE_MENU;
            ui.dirty = true;
          }
        }
        break;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void ApiTask(void *pv) {
  (void)pv;
  for (;;) {
    g_fetching = true;
    bool changed = api_fetch_departures(SITE_ID, g_directionCode);
    g_fetching = false;
    if (departureCount > 0) g_hasData = true;
    if (changed) {
      g_dataUpdated = true;
      if (ui.state == STATE_DEPARTURES) {
        ui.dirty = true;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(30000));
  }
}






// Setup
TaskHandle_t hInput = NULL;
TaskHandle_t hDisplay = NULL;
TaskHandle_t hLogic = NULL;
TaskHandle_t hApi = NULL;

SemaphoreHandle_t gDeparturesMutex;

void setup() {
  Serial.begin(115200);

  input_init();
  display_init();  // laddar brightness från NVS

  Preferences prefs;
  prefs.begin("board", true);
  g_walkMinutes   = (int)prefs.getUChar("walkmin", 0);
  g_directionCode = (int)prefs.getUChar("dircode", 0);
  g_colourway     = prefs.getUShort("colourway", COLOR_ORANGE);
  prefs.end();

  gDeparturesMutex = xSemaphoreCreateMutex(); // API MUTEX

  xTaskCreate(InputTask, "Input", 4096, NULL, 3, &hInput);
  xTaskCreate(DisplayTask, "Display", 8192, NULL, 4, &hDisplay);
  xTaskCreate(ControlLogicTask, "Logic", 4096, NULL, 2, &hLogic);
  xTaskCreate(ApiTask, "API", 8192, NULL, 1, &hApi);
}

// Main-loop
void loop() {
  vTaskDelay(portMAX_DELAY);
}
