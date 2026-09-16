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

  for (;;) {
    if (displayDisabled) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    // Läs EN gång per varv — händelserna konsumeras när de läses.
    // Gestmodell: kort tryck = välj, långt tryck = tillbaka ett steg.
    // Avgångsskärmen ÄR hemskärmen, så där finns inget att gå tillbaka till
    // och långtrycket är reserverat för QR-koden (fas 4).
    const PressEvent press = input_selectEvent();
    const bool next = input_encoderNext();  // nedåt i listan / högre värde
    const bool prev = input_encoderPrev();  // uppåt i listan / lägre värde

    switch (ui.state) {
      case STATE_BOOT:
        {
          if (ui.bootStartMs == 0) {
            ui.bootStartMs = millis();
            ui.dirty = true;  // rita boot EN gång
          }

          const uint32_t elapsed = millis() - ui.bootStartMs;
          const bool dataReady = (g_lastFetchResult != FETCH_PENDING);

          // Lämna bootskärmen så fort första hämtningen svarat, men låt
          // logotypen synas åtminstone BOOT_MIN_MS.
          if (elapsed >= BOOT_MS || (dataReady && elapsed >= BOOT_MIN_MS)) {
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
          if (prev) { ui.selectedIndex--; mainMenuWrap(); ui.dirty = true; }
          if (next) { ui.selectedIndex++; mainMenuWrap(); ui.dirty = true; }

          if (press == PRESS_LONG) {          // tillbaka till hemskärmen
            ui.state = STATE_DEPARTURES;
            ui.scrollOffset = 0;
            ui.dirty = true;
            break;
          }

          if (press == PRESS_SHORT) {
            switch (ui.selectedIndex) {
              case 0:
                ui.state = STATE_DEPARTURES;
                ui.scrollOffset = 0;
                g_dataUpdated = false;
                requestFetch();   // färsk data direkt när man öppnar listan
                ui.dirty = true;
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
          if (prev && ui.selectedIndex > 0) { ui.selectedIndex--; ui.dirty = true; }
          if (next && ui.selectedIndex < 2) { ui.selectedIndex++; ui.dirty = true; }

          if (press == PRESS_LONG) {
            ui.state = STATE_MENU;
            ui.selectedIndex = 2;
            ui.dirty = true;
            break;
          }

          if (press == PRESS_SHORT) {
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
          if (prev && ui.selectedIndex > 0) { ui.selectedIndex--; ui.dirty = true; }
          if (next && ui.selectedIndex < 2) { ui.selectedIndex++; ui.dirty = true; }

          if (press == PRESS_LONG) {   // ångra utan att spara
            ui.state = STATE_DISPLAY_MENU;
            ui.selectedIndex = 1;
            ui.dirty = true;
            break;
          }

          if (press == PRESS_SHORT) {
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
          if (press == PRESS_SHORT) {
            ui.state = STATE_MENU;
            ui.dirty = true;
          }
          // PRESS_LONG: reserverad för QR-koden, se fas 4.

          // Samma filterlogik som renderingen använder — en enda källa.
          // Radkapaciteten varierar: statusraden stjal en rad nar den visas.
          const int filteredCount = departures_visibleCount();
          const int capacity = departures_rowCapacity();
          const int maxOffset = (filteredCount > capacity) ? filteredCount - capacity : 0;

          // Listan kan ha krympt sedan förra varvet (avgångar som gått).
          if (ui.scrollOffset > maxOffset) {
            ui.scrollOffset = maxOffset;
            ui.dirty = true;
          }

          if (next && ui.scrollOffset < maxOffset) { ui.scrollOffset++; ui.dirty = true; }
          if (prev && ui.scrollOffset > 0)         { ui.scrollOffset--; ui.dirty = true; }

          if (g_dataUpdated) {
            g_dataUpdated = false;
            ui.dirty = true;
          }

          // Minuterna beräknas vid rendering, så bilden måste ritas om när
          // minuten växlar — annars står nedräkningen still ändå.
          static int lastMinute = -1;
          const int nowMinute = (int)(time(nullptr) / 60);
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
          if (next && ui.scrollOffset < 2) { ui.scrollOffset++; ui.dirty = true; }
          if (prev && ui.scrollOffset > 0) { ui.scrollOffset--; ui.dirty = true; }

          if (press == PRESS_LONG) {
            ui.state = STATE_MENU;
            ui.selectedIndex = 1;
            ui.dirty = true;
            break;
          }

          if (press == PRESS_SHORT) {
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
          if (next && ui.selectedIndex < 60) { ui.selectedIndex++; ui.dirty = true; }
          if (prev && ui.selectedIndex > 0)  { ui.selectedIndex--; ui.dirty = true; }

          if (press == PRESS_LONG) {   // ångra utan att spara
            ui.state = STATE_STATION;
            ui.scrollOffset = 0;
            ui.dirty = true;
            break;
          }

          if (press == PRESS_SHORT) {
            g_walkMinutes = ui.selectedIndex;
            Preferences prefs;
            prefs.begin("board", false);
            prefs.putUChar("walkmin", (uint8_t)g_walkMinutes);
            prefs.end();
            ui.state = STATE_STATION;
            ui.scrollOffset = 0;  // återgå till Walk-raden
            ui.dirty = true;
          }
        }
        break;

      case STATE_BRIGHTNESS:
        {
          const uint8_t STEP = 8;

          if (next) {
            uint8_t cur = display_getBrightness();
            display_setBrightness(cur + STEP > 255 ? 255 : cur + STEP);
            ui.dirty = true;
          }
          if (prev) {
            uint8_t cur = display_getBrightness();
            display_setBrightness(cur > STEP ? cur - STEP : 0);
            ui.dirty = true;
          }

          // Både kort och långt tryck lämnar skärmen; värdet sparas ändå.
          if (press != PRESS_NONE) {
            display_saveBrightness();
            ui.state = STATE_DISPLAY_MENU;
            ui.selectedIndex = 0;
            ui.dirty = true;
          }
        }
        break;

      case STATE_SYSTEM_SETTINGS:
        {
          if (press != PRESS_NONE) {
            ui.state = STATE_MENU;
            ui.selectedIndex = 3;
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
