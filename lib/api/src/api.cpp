#include <api.h>
#include <logic.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string.h>
#include <time.h>
#include <config.h>
#include <WiFiClientSecure.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t gDeparturesMutex;

// WiFi Inställningar och Initiering
static bool ntpSynced = false;

static bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) {
    delay(100);
  }
  if (WiFi.status() != WL_CONNECTED) return false;

  if (!ntpSynced) {
    configTime(0, 0, "pool.ntp.org");
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    // Vänta tills NTP svarar (max 3s)
    unsigned long t1 = millis();
    while (time(nullptr) < 1000000UL && millis() - t1 < 3000) delay(100);
    ntpSynced = (time(nullptr) > 1000000UL);
  }
  return true;
}

bool api_fetch_departures(int siteId, int directionCode) {
  if (!ensureWiFi()) return false;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(5000);

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

  JsonDocument filter;
  filter["departures"][0]["line"]["designation"] = true;
  filter["departures"][0]["destination"] = true;
  filter["departures"][0]["display"] = true;
  filter["departures"][0]["direction_code"] = true;
  filter["departures"][0]["expected"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, *http.getStreamPtr(),
                                             DeserializationOption::Filter(filter));
  http.end();
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

    const char* line     = d["line"]["designation"] | "";
    const char* dest     = d["destination"] | "";
    const char* disp     = d["display"] | "";
    const char* expected = d["expected"] | "";

    if (!line[0] || !dest[0] || !disp[0]) continue;

    int dir = d["direction_code"] | 0;
    if (directionCode != 0 && dir != directionCode) continue;

    Departure* out = &temp[tempCount];

    strncpy(out->line, line, sizeof(out->line) - 1);
    out->line[sizeof(out->line) - 1] = '\0';

    strncpy(out->destination, dest, sizeof(out->destination) - 1);
    out->destination[sizeof(out->destination) - 1] = '\0';

    strncpy(out->display, disp, sizeof(out->display) - 1);
    out->display[sizeof(out->display) - 1] = '\0';

    // Extrahera HH:MM från expected + beräkna minsUntil via NTP
    const char* tPtr = strchr(expected, 'T');
    if (tPtr && strlen(tPtr) >= 6) {
      snprintf(out->depTime, sizeof(out->depTime), "%.5s", tPtr + 1);
    } else {
      out->depTime[0] = '\0';
    }

    out->minsUntil = 0;
    if (tPtr) {
      struct tm tmDep = {0};
      int y, mo, d, h, mi, s;
      if (sscanf(expected, "%4d-%2d-%2dT%2d:%2d:%2d", &y, &mo, &d, &h, &mi, &s) == 6) {
        tmDep.tm_year = y - 1900;
        tmDep.tm_mon  = mo - 1;
        tmDep.tm_mday = d;
        tmDep.tm_hour = h;
        tmDep.tm_min  = mi;
        tmDep.tm_sec  = s;
        tmDep.tm_isdst = -1;
        time_t depEpoch = mktime(&tmDep);
        time_t nowEpoch = time(nullptr);
        if (nowEpoch > 1000000UL && depEpoch > nowEpoch) {
          out->minsUntil = (uint16_t)((depEpoch - nowEpoch) / 60);
        }
      }
    }
    // Fallback om NTP ej synkat
    if (out->minsUntil == 0) {
      out->minsUntil = strchr(disp, ':') ? 999 : (uint16_t)atoi(disp);
    }

    tempCount++;
  }

  if (tempCount <= 0) return false;

  // =========================
  // 2) Committa alltid, men kolla om något faktiskt ändrades
  // =========================
  if (gDeparturesMutex) xSemaphoreTake(gDeparturesMutex, portMAX_DELAY);

  bool changed = (tempCount != departureCount);
  if (!changed) {
    for (int i = 0; i < tempCount && !changed; i++) {
      if (strcmp(temp[i].display,     departures[i].display)     != 0 ||
          strcmp(temp[i].line,        departures[i].line)        != 0 ||
          strcmp(temp[i].destination, departures[i].destination) != 0) {
        changed = true;
      }
    }
  }

  memcpy(departures, temp, sizeof(Departure) * tempCount);
  departureCount = tempCount;

  if (gDeparturesMutex) xSemaphoreGive(gDeparturesMutex);

  return changed;  // true = data ändrades, false = identisk data (skippa re-render)
}
