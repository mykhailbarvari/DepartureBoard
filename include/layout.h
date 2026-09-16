#pragma once

// X-layout (horisontell indelning av 128px bred display)
// Varje intervall är INKLUSIVT: [START .. END]

// LINE-fält (t.ex. "802")
#define X_LINE_START       1   // Var linjen (bussnumret) börjar
#define X_LINE_END         18  // Var linjefältet slutar

// DESTINATION-fält (t.ex. "Gullmarsplan")
#define X_DEST_START       21  // Var destinationsnamnet börjar
#define X_DEST_END         89  // Var destinationsfältet slutar

// MINUTER (endast siffror, t.ex. "7", "12")
#define X_MIN_START        92  // Var minut-siffrorna börjar
#define X_MIN_END          126 // Var minut-siffrorna slutar (bra för right-align)

// Y-layout & sidstruktur
#define PAGE_SIZE 5            // Antal avgångar per sida
#define ROWS      5            // Antal rader som visas samtidigt

// Avstånd mellan rader i pixlar
// Fontens maxhöjd är 12px → 12 + 1px luft
#define Y_OFFSET  13           // Distans mellan rader (förhindrar överlapp)

#define CHAR_W    6            // Bredd per tecken inklusive spacing

// Stora bytespunkter (Gullmarsplan 59, T-Centralen 68) ger fler avgangar an
// 30 nu nar vi hamtar alla trafikslag och bada riktningarna och filtrerar
// vid rendering istallet. Kostar ~90 byte per post.
#define MAX_DEPARTURES 70
