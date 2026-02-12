#include <Arduino.h>
#include <display.h>
#include <logic.h>
#include <api.h>
#include <input.h>
#include <config.h>

// Globalt Deklarerade Variabler
volatile bool displayDisabled = false;  // Togglar panel, TRUE = PANEL AV
volatile bool g_hasData = false;        // ApiTask sätter TRUE när data finns
volatile bool g_dataUpdated = false;

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
      display_off();  // setBrightness(0)
      wasDisabled = true;
      vTaskDelay(pdMS_TO_TICKS(20));
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
        if (gDeparturesMutex && xSemaphoreTake(gDeparturesMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
          renderMainFromArray(ui.scrollOffset);
          xSemaphoreGive(gDeparturesMutex);
        }
        break;

      case STATE_BRIGHTNESS:
        // ENDAST RENDERING
        break;

      case STATE_STATION:
        // ENDAST RENDERING
        break;

      case STATE_SYSTEM_SETTINGS:
        // ENDAST RENDERING
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
            ui.state = STATE_MENU;
            ui.dirty = true;  // ← rita menyn
          }
        }
        break;


      case STATE_INIT:
        // ENDAST LOGIK
        break;

      case STATE_MENU:
        {
static bool prevSelect = false;

bool upPressed   = input_nav2();   // consume-event
bool downPressed = input_nav1();   // consume-event

bool select = input_select();
bool selectPressed = select && !prevSelect;
prevSelect = select;


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
            // exempel: välj meny
            switch (ui.selectedIndex) {
              case 0:
                ui.state = STATE_DEPARTURES;
                ui.scrollOffset = 0;
                ui.dirty = true;
                g_dataUpdated = false;
                break;

              case 1:
                ui.state = STATE_STATION;
                break;

              case 2:
                ui.state = STATE_BRIGHTNESS;
                break;

              case 3:
                ui.state = STATE_SYSTEM_SETTINGS;
                break;
            }
            ui.dirty = true;
          }

          if (selectPressed && ui.selectedIndex == 0) {
            ui.state = STATE_DEPARTURES;
            ui.dirty = true;        // <-- rita departures direkt
            g_dataUpdated = false;  // valfritt: "vi kommer visa senaste"
          }
        }
        break;

      case STATE_DEPARTURES:
        {
          static bool prevBack = false;
          bool back = input_select();  // välj en knapp som “back”
          bool backPressed = back && !prevBack;
          prevBack = back;

          if (backPressed) {
            ui.state = STATE_MENU;
            ui.dirty = true;
          }

          if (g_dataUpdated) {
            g_dataUpdated = false;
            ui.dirty = true;
          }
        }
        // -------------------------------------------------------------------------------HÄR SKA SCROLL LOGIK VARA
        break;

      case STATE_STATION:
        // ENDAST LOGIK
        break;

      case STATE_BRIGHTNESS:
        // ENDAST LOGIK
        break;

      case STATE_SYSTEM_SETTINGS:
        // ENDAST LOGIK
        break;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void ApiTask(void *pv) {
  (void)pv;
  for (;;) {
    bool ok = api_fetch_departures(SITE_ID);  // den ska fylla din departures-array
    if (ok) {
      g_hasData = true;
      g_dataUpdated = true;  // <-- "ny data finns"
      // RITA OM bara om vi är i departures (eller annat state som visar datan)
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
  display_init();

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
