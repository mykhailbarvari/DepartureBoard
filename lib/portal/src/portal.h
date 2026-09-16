#pragma once
#include <stdbool.h>

// Enhetens egen konfigurationsportal. Tar bort behovet av att flasha om för
// att byta hållplats, och är enda rimliga sättet att mata in text när den
// enda inmatningsenheten är en rotationsencoder.
//
// Två lägen:
//
//  STA — enheten är på hemnätet och sidan nås på dess LAN-IP. Telefonen har
//        internet, så hållplatssökningen kan gå direkt mot SL.
//
//  AP  — enheten saknar WiFi-uppgifter och startar ett eget öppet nät plus
//        captive portal. Telefonen har DÅ INGEN internetuppkoppling, så
//        hållplatssökningen fungerar inte här. Därför är flödet tvåstegs:
//        välj hemnätverk här, återgå sedan till hemnätet och öppna sidan via
//        enhetens LAN-adress (QR-koden på panelen) för att välja hållplats.

// forceAp tvingar SoftAP aven nar WiFi-uppgifter finns sparade. Utan den
// gar en redan konfigurerad tavla aldrig att omkonfigurera fran panelen.
void portal_begin(bool forceAp);

bool        portal_isAp(void);      // true = kör i SoftAP-läge
const char* portal_apSsid(void);    // SSID:t enheten sänder i AP-läge

// FreeRTOS-task: servar HTTP och (i AP-läge) DNS.
void portal_task(void* pv);
