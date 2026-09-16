#include <Arduino.h>
#include <display.h>
#include <logic.h>
#include <api.h>
#include <input.h>
#include <config.h>
#include <Preferences.h>
#include <time.h>
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "driver/gpio.h"

// Globalt Deklarerade Variabler
volatile bool displayDisabled = false;  // Togglar panel, TRUE = PANEL AV
volatile bool g_dataUpdated = false;
volatile bool g_fetching = false;       // ApiTask fetchar just nu
volatile int g_walkMinutes   = 0;   // minuter promenad — filtrerar avgångar under detta
volatile int g_directionCode = 0;   // 0 = alla, 1 = riktning 1, 2 = riktning 2
volatile uint16_t g_colourway = COLOR_ORANGE;  // Accentfärg för UI

// Task-handles deklareras före tasksen, så ControlLogicTask kan väcka ApiTask.
TaskHandle_t hInput = NULL;
TaskHandle_t hDisplay = NULL;
TaskHandle_t hLogic = NULL;
TaskHandle_t hApi = NULL;

SemaphoreHandle_t gDeparturesMutex;

// Väcker ApiTask direkt istället för att vänta ut hämtningsintervallet.
// Används när användaren gör något som bör ge färsk data omedelbart.
static void requestFetch(void) {
  if (hApi) xTaskNotifyGive(hApi);
}

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

          // Efter BOOT_MS → gå till departures
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
                requestFetch();   // färsk data direkt när man öppnar listan
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

          // Samma filterlogik som renderingen använder — en enda källa.
          int filteredCount = departures_visibleCount();
          int maxOffset = (filteredCount > ROWS) ? filteredCount - ROWS : 0;

          // Listan kan ha krympt sedan förra varvet (avgångar som gått).
          if (ui.scrollOffset > maxOffset) {
            ui.scrollOffset = maxOffset;
            ui.dirty = true;
          }

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

          // Minuterna beräknas vid rendering, så bilden måste ritas om när
          // minuten växlar — annars står nedräkningen still ändå.
          static int lastMinute = -1;
          int nowMinute = (int)(time(nullptr) / 60);
          if (nowMinute != lastMinute) {
            lastMinute = nowMinute;
            ui.dirty = true;
          }

          // Håll "Fetching..."-animationen levande, men bara medan den visas.
          if (g_fetching || g_lastFetchResult == FETCH_PENDING) {
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

              case 1:  // Toggla direction: 0→1→2→0, spara direkt.
                // Riktningen filtreras numera vid rendering, så bytet slår
                // igenom direkt utan att invänta en ny hämtning.
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

  const uint32_t INTERVAL_OK_MS = 30000;  // normalt hämtningsintervall
  const uint32_t BACKOFF_MIN_MS = 5000;   // första omförsöket efter ett fel
  const uint32_t BACKOFF_MAX_MS = 60000;  // taket för backoff

  uint32_t backoffMs = BACKOFF_MIN_MS;
  uint32_t waitMs    = 0;  // första varvet: hämta direkt

  for (;;) {
    // Vaknar antingen när tiden gått ut eller när requestFetch() kallats.
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(waitMs));

    g_fetching = true;
    FetchResult r = api_fetch_departures(SITE_ID);
    g_fetching = false;

    Serial.printf("[api] %s (%d avgangar)\n", api_fetchResultName(r), departureCount);

    if (r == FETCH_NO_WIFI || r == FETCH_HTTP_ERR || r == FETCH_PARSE_ERR) {
      // Fel: backa av exponentiellt istället för att hamra var 30:e sekund.
      waitMs = backoffMs;
      backoffMs = (backoffMs >= BACKOFF_MAX_MS / 2) ? BACKOFF_MAX_MS : backoffMs * 2;
    } else {
      backoffMs = BACKOFF_MIN_MS;
      waitMs    = INTERVAL_OK_MS;
    }

    if (r == FETCH_OK || r == FETCH_EMPTY) {
      g_dataUpdated = true;
      if (ui.state == STATE_DEPARTURES) {
        ui.dirty = true;
      }
    }
  }
}




// Setup
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
