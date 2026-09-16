#pragma once
#include <stdint.h>
#include <stdbool.h>

// QR-kod på LED-panelen.
//
// Två saker som är lätta att göra fel och som den här modulen tar hand om:
//
//  1. Koden ritas MÖRK PÅ LJUS — svarta moduler på vit botten, inklusive den
//     tysta zonen. Det är frestande att rita tvärtom på en LED-panel (lysande
//     moduler mot svart), men en inverterad QR läses inte av de flesta
//     telefonkameror.
//
//  2. Versionen väljs utifrån en egen kapacitetstabell. Biblioteket
//     (ricmoo/QRCode) har "@TODO: Return error if data is too big" i koden och
//     validerar alltså inte längden — ett för långt meddelande ger en tyst
//     trasig kod, eller värre.

// Ritar en QR-kod. xCenter är kodens mittpunkt i x-led, yTop dess överkant.
// maxSize är största tillåtna sida i pixlar (koden är alltid kvadratisk).
// Returnerar false om texten inte får plats — då ritas ingenting.
bool ui_drawQR(const char* text, int xCenter, int yTop, int maxSize);

// Kodens sida i pixlar för en given text, utan att rita. 0 = får inte plats.
int ui_qrSize(const char* text, int maxSize);
