#pragma once
#include <stdbool.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t gDeparturesMutex;

// Utfallet av ett hamtningsforsok. UI:t anvander detta for att kunna saga
// VARFOR listan ar tom, istallet for att visa "Fetching..." i all evighet.
typedef enum {
  FETCH_PENDING = 0,  // har annu inte gjort ett forsta forsok
  FETCH_OK,     // ny data committad
  FETCH_UNCHANGED,  // svar OK, men identiskt med det vi redan visar
  FETCH_NO_WIFI,
  FETCH_HTTP_ERR,
  FETCH_PARSE_ERR,
  FETCH_EMPTY       // svar OK men noll avgangar (t.ex. nattetid)
} FetchResult;

// Hamtar avgangar for en hallplats.
// Riktning och gangtid filtreras vid RENDERING, inte har, sa att ett
// riktningsbyte slar igenom direkt utan att invanta nasta hamtning.
FetchResult api_fetch_departures(int siteId);

const char* api_fetchResultName(FetchResult r);

// Vacker ApiTask direkt istallet for att vanta ut intervallet.
// Anropas nar en installning andrats (fran UI:t eller webbportalen).
void requestFetch(void);

// Senaste utfall + millis() vid senaste lyckade svar (0 = aldrig lyckats).
// Lases av UI:t for att kunna visa "uppdaterad X min sedan".
extern volatile FetchResult g_lastFetchResult;
extern volatile uint32_t    g_lastSuccessMs;
