#include <api.h>
#include <logic.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <config.h>
#include <settings.h>
#include <portal.h>
#include <WiFiClientSecure.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t gDeparturesMutex;

volatile FetchResult g_lastFetchResult = FETCH_PENDING;
volatile uint32_t    g_lastSuccessMs   = 0;

const char* api_fetchResultName(FetchResult r) {
  switch (r) {
    case FETCH_PENDING:   return "PENDING";
    case FETCH_OK:        return "OK";
    case FETCH_UNCHANGED: return "UNCHANGED";
    case FETCH_NO_WIFI:   return "NO_WIFI";
    case FETCH_HTTP_ERR:  return "HTTP_ERR";
    case FETCH_PARSE_ERR: return "PARSE_ERR";
    case FETCH_EMPTY:     return "EMPTY";
  }
  return "?";
}

// WiFi Inställningar och Initiering
static bool ntpSynced = false;

static bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (portal_isAp()) return false;         // SoftAP aktiv: rör inte radion
  if (!settings_hasWifi()) return false;   // oprovisionerad: portalen tar över
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(g_settings.wifiSsid, g_settings.wifiPass);

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

// ---------------------------------------------------------------- PARSHJÄLP

// SL skickar lokal tid utan offset ("2026-09-16T20:29:26"). TZ är satt till
// CET/CEST ovan, så mktime med tm_isdst = -1 tolkar den rätt året om.
static time_t parseIsoLocal(const char* iso) {
  if (!iso || !iso[0]) return 0;

  struct tm t;
  memset(&t, 0, sizeof(t));

  int y, mo, d, h, mi, s;
  if (sscanf(iso, "%4d-%2d-%2dT%2d:%2d:%2d", &y, &mo, &d, &h, &mi, &s) != 6) return 0;

  t.tm_year  = y - 1900;
  t.tm_mon   = mo - 1;
  t.tm_mday  = d;
  t.tm_hour  = h;
  t.tm_min   = mi;
  t.tm_sec   = s;
  t.tm_isdst = -1;
  return mktime(&t);
}

static uint8_t parseState(const char* s) {
  if (!s)                      return DEP_STATE_OTHER;
  if (!strcmp(s, "EXPECTED"))  return DEP_EXPECTED;
  if (!strcmp(s, "ATSTOP"))    return DEP_ATSTOP;
  if (!strcmp(s, "CANCELLED")) return DEP_CANCELLED;
  return DEP_STATE_OTHER;
}


static void copyStr(char* dst, size_t cap, const char* src) {
  if (!cap) return;
  if (!src) { dst[0] = 0; return; }
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = 0;
}

// Sorteras på faktisk avgångstid. Poster utan giltig tid (depEpoch == 0, t.ex.
// om NTP inte hunnit synka) hamnar sist istället för först.
static int cmpByDeparture(const void* a, const void* b) {
  const Departure* x = (const Departure*)a;
  const Departure* y = (const Departure*)b;
  if (x->depEpoch == y->depEpoch) return 0;
  if (x->depEpoch == 0) return 1;
  if (y->depEpoch == 0) return -1;
  return (x->depEpoch < y->depEpoch) ? -1 : 1;
}

// Byggs upp här och kopieras in under mutex först när den är komplett.
// static (inte stack) — MAX_DEPARTURES poster är för mycket för ApiTasks stack.
static Departure s_temp[MAX_DEPARTURES];

// Så många tomma svar i rad som krävs innan vi tror på dem och blankar listan.
#define EMPTY_CONFIRMATIONS 3
static uint8_t s_consecutiveEmpty = 0;

// ------------------------------------------------------------------ HÄMTNING

FetchResult api_fetch_departures(int siteId) {
  if (!ensureWiFi()) {
    g_lastFetchResult = FETCH_NO_WIFI;
    return FETCH_NO_WIFI;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(5000);

  // Trafikslag kommer från inställningarna. Tom parameter = alla trafikslag,
  // och då ska den utelämnas helt istället för att skickas tom.
  char transport[48];
  settings_transportParam(transport, sizeof(transport));

  char url[224];
  if (transport[0]) {
    snprintf(url, sizeof(url),
             "https://transport.integration.sl.se/v1/sites/%d/departures?transport=%s",
             siteId, transport);
  } else {
    snprintf(url, sizeof(url),
             "https://transport.integration.sl.se/v1/sites/%d/departures",
             siteId);
  }

  if (!http.begin(client, url)) {
    g_lastFetchResult = FETCH_HTTP_ERR;
    return FETCH_HTTP_ERR;
  }

  int code = http.GET();
  if (code != 200) {
    http.end();
    g_lastFetchResult = FETCH_HTTP_ERR;
    return FETCH_HTTP_ERR;
  }

  JsonDocument filter;
  JsonObject f = filter["departures"].add<JsonObject>();
  f["line"]["designation"]       = true;
  f["destination"]               = true;
  f["display"]                   = true;
  f["direction_code"]            = true;
  f["state"]                     = true;
  f["expected"]                  = true;
  f["stop_area"]["name"]         = true;
  // Bara importance_level — vi behöver veta ATT det finns en störning, inte
  // texten. Meddelandena är långa och skulle äta heap i onödan.
  f["deviations"][0]["importance_level"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, *http.getStreamPtr(),
                                             DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    g_lastFetchResult = FETCH_PARSE_ERR;
    return FETCH_PARSE_ERR;
  }

  JsonArray deps = doc["departures"].as<JsonArray>();
  if (deps.isNull()) {
    g_lastFetchResult = FETCH_PARSE_ERR;
    return FETCH_PARSE_ERR;
  }

  // =========================
  // 1) Bygg TEMP data först
  // =========================
  int tempCount = 0;
  const time_t now = time(nullptr);
  const bool clockValid = (now > 1000000UL);

  // Hållplatsens namn följer med varje avgång. SL har ingen endpoint för att
  // slå upp ett site-id (/v1/sites/<id> svarar 501), så det här är enda
  // sättet att få namnet utan att ladda hela hållplatslistan på 1,3 MB.
  //
  // På bytespunkter förekommer flera stop_area-namn under samma site —
  // T-Centralen 49 gånger mot Stockholm City 14 — så det vanligaste vinner.
  // Görs bara när namnet saknas: väljer användaren hållplats i portalen är
  // det namnet hämtat ur SL:s egen hållplatslista och ska inte skrivas över.
  struct NameTally { char name[32]; uint16_t n; };
  NameTally tally[6];
  int tallyCount = 0;
  const bool wantName = (g_settings.siteName[0] == 0);

  for (JsonObject d : deps) {
    if (tempCount >= MAX_DEPARTURES) break;

    const char* line = d["line"]["designation"] | "";
    const char* dest = d["destination"] | "";
    const char* disp = d["display"] | "";

    if (!line[0] || !dest[0] || !disp[0]) continue;

    Departure* out = &s_temp[tempCount];
    memset(out, 0, sizeof(*out));

    copyStr(out->line,         sizeof(out->line),         line);
    copyStr(out->destination,  sizeof(out->destination),  dest);
    copyStr(out->display,      sizeof(out->display),      disp);

    out->directionCode = (uint8_t)(d["direction_code"] | 0);
    out->state         = parseState(d["state"] | (const char*)nullptr);
    out->hasDeviation  = !d["deviations"].isNull() && d["deviations"].size() > 0;

    const char* expected = d["expected"] | "";
    out->depEpoch = parseIsoLocal(expected);

    // HH:MM ur expected, för visning när avgången ligger långt fram
    const char* tPtr = strchr(expected, 'T');
    if (tPtr && strlen(tPtr) >= 6) {
      snprintf(out->depTime, sizeof(out->depTime), "%.5s", tPtr + 1);
    }

    if (wantName) {
      const char* area = d["stop_area"]["name"] | "";
      if (area[0]) {
        int k = 0;
        for (; k < tallyCount; k++) {
          if (strcmp(tally[k].name, area) == 0) { tally[k].n++; break; }
        }
        if (k == tallyCount && tallyCount < (int)(sizeof(tally) / sizeof(tally[0]))) {
          copyStr(tally[tallyCount].name, sizeof(tally[tallyCount].name), area);
          tally[tallyCount].n = 1;
          tallyCount++;
        }
      }
    }

    // Släng avgångar som redan gått. Gör det bara när klockan är pålitlig —
    // annars vore jämförelsen mot en 1970-klocka meningslös.
    if (clockValid && out->depEpoch && out->depEpoch < now - 60) continue;

    tempCount++;
  }

  if (wantName && tallyCount > 0) {
    int best = 0;
    for (int i = 1; i < tallyCount; i++) {
      if (tally[i].n > tally[best].n) best = i;
    }
    copyStr(g_settings.siteName, sizeof(g_settings.siteName), tally[best].name);
    settings_save();   // namnet är stabilt, så det här sker i praktiken en gång
  }

  qsort(s_temp, tempCount, sizeof(Departure), cmpByDeparture);

  // SL:s API returnerar då och då en tom lista trots att trafik faktiskt går
  // (verifierat: två anrop några sekunder isär gav 0 respektive 18 avgångar).
  // Blanka därför inte en fungerande tavla på ett enstaka tomt svar — men ge
  // efter när de kommer på rad, för då har trafiken verkligen tagit slut.
  if (tempCount == 0) {
    if (s_consecutiveEmpty < 255) s_consecutiveEmpty++;
    if (s_consecutiveEmpty < EMPTY_CONFIRMATIONS && departureCount > 0) {
      g_lastSuccessMs   = millis();
      g_lastFetchResult = FETCH_UNCHANGED;
      return FETCH_UNCHANGED;
    }
  } else {
    s_consecutiveEmpty = 0;
  }

  // =========================
  // 2) Committa data (även tom lista, när den bekräftats ovan).
  //    Annars ligger gamla avgångar kvar för evigt när trafiken tar slut.
  // =========================
  if (gDeparturesMutex) xSemaphoreTake(gDeparturesMutex, portMAX_DELAY);

  bool changed = (tempCount != departureCount);
  if (!changed) {
    for (int i = 0; i < tempCount && !changed; i++) {
      if (s_temp[i].depEpoch != departures[i].depEpoch ||
          s_temp[i].state    != departures[i].state    ||
          strcmp(s_temp[i].line,        departures[i].line)        != 0 ||
          strcmp(s_temp[i].destination, departures[i].destination) != 0) {
        changed = true;
      }
    }
  }

  if (tempCount > 0) memcpy(departures, s_temp, sizeof(Departure) * tempCount);
  departureCount = tempCount;

  if (gDeparturesMutex) xSemaphoreGive(gDeparturesMutex);

  g_lastSuccessMs = millis();

  FetchResult r = (tempCount == 0) ? FETCH_EMPTY
                : changed          ? FETCH_OK
                                   : FETCH_UNCHANGED;
  g_lastFetchResult = r;
  return r;
}
