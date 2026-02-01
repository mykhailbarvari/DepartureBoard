#include <api.h>
#include <logic.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string.h>
#include <config.h>
#include <WiFiClientSecure.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t gDeparturesMutex;

// WiFi Inställningar och Initiering
static bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) {
    delay(100); // OK i ApiTask, men kan bytas till vTaskDelay om du vill
  }
  return (WiFi.status() == WL_CONNECTED);
}

bool api_fetch_departures(int siteId) {
  if (!ensureWiFi()) return false;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(5000); // bra att ha, annars kan den hänga länge

  char url[192];
  snprintf(url, sizeof(url),
           "https://transport.integration.sl.se/v1/sites/%d/departures?transport=BUS",
           siteId);

  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(96 * 1024);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) return false;

  JsonArray deps = doc["departures"].as<JsonArray>();
  if (deps.isNull()) return false;

  // =========================
  // 1) Bygg TEMP data först
  // =========================
  Departure temp[MAX_DEPARTURES];
  int tempCount = 0;

  for (JsonObject d : deps) {
    if (tempCount >= MAX_DEPARTURES) break;

    const char* line = d["line"]["designation"] | "";
    const char* dest = d["destination"] | "";
    const char* disp = d["display"] | "";

    if (!line[0] || !dest[0] || !disp[0]) continue;

    Departure* out = &temp[tempCount];

    strncpy(out->line, line, sizeof(out->line) - 1);
    out->line[sizeof(out->line) - 1] = '\0';

    strncpy(out->destination, dest, sizeof(out->destination) - 1);
    out->destination[sizeof(out->destination) - 1] = '\0';

    strncpy(out->display, disp, sizeof(out->display) - 1);
    out->display[sizeof(out->display) - 1] = '\0';

    tempCount++;
  }

  if (tempCount <= 0) return false;

  // =========================
  // 2) COMMIT till render-data
  // =========================
  if (gDeparturesMutex) xSemaphoreTake(gDeparturesMutex, portMAX_DELAY);

  // Kopiera hela blocket på en gång (snabbt, “atomiskt nog”)
  memcpy(departures, temp, sizeof(Departure) * tempCount);
  departureCount = tempCount;

  if (gDeparturesMutex) xSemaphoreGive(gDeparturesMutex);

  Serial.printf("API-FETCH ok (%d)\n", departureCount);
  return true;
}
