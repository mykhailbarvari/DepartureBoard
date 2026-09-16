# Departure Board — ESP32 Transit Display

A real-time LED departure board that shows live public transport departures from
any stop in Stockholm. Built on a Waveshare
[ESP32-S3 Nano](https://www.waveshare.com/wiki/ESP32-S3-Nano) driving two
daisy-chained [HUB75E LED matrix panels](https://www.waveshare.com/wiki/RGB-Matrix-P2-64x64),
with a custom PCB and a 3D-printed chassis.

It configures itself over WiFi — scan the QR code on the panel and the board
serves its own settings page. No reflashing to change stop, direction or
appearance.

<!-- TODO: byt ut mot en GIF inspelad med demoläget (System → Starta demo) -->
> **Demo:** a recording of the interface goes here.

---

## Why this project?
After finishing the courses IS1300 Embedded Systems and IS1200 Computer Hardware Engineering at my university, I wanted to move beyond course assignments and theory and build something with practical, everyday use.

During the winter, bus traffic became irregular due to snow, and I often found myself standing in the cold because the scheduled departure times were inaccurate.
That's when I realized how useful a real-time, always-on departure display would be.

Beyond functionality, I wanted to build something I would actually want to keep in my home. Something useful, but also nice to look at and be proud to show friends.

While searching for inspiration online, I stumbled upon the departure boards built by 
[T-Skylt](https://shop.t-skylt.se/). Their work was a major source of inspiration for this project, especially in terms of concept and overall aesthetic.

---

## What it does

- **Live countdown.** Minutes are derived from each departure's timestamp at
  render time, so the number ticks down on its own rather than sitting frozen
  between API polls. Departures that have left are dropped.
- **Honest failure.** The board says *why* the list is empty — no WiFi, SL not
  answering, or genuinely no departures — instead of animating a spinner
  forever. Data older than two minutes is flagged with its age.
- **Cancellations and disruptions.** A cancelled departure turns red and reads
  "Inst"; a departure carrying a service deviation gets a marker in the margin.
- **Configures itself.** A QR code on the panel opens a settings page served by
  the board: stop, transport modes, direction, walking time, brightness and
  colour theme.
- **Carousel menu** driven by a single rotary encoder, with sliding transitions.
- **Demo mode** that tours the whole interface hands-free, for filming.
- Runs five FreeRTOS tasks: input, rendering, UI logic, API polling and the
  web portal.

## The interface

The departures list is home. One rotary encoder with a push button drives
everything:

| | Short press | Long press |
|---|---|---|
| **Departures (home)** | open the menu | show the QR code |
| **Menu and submenus** | select | back one level |

Turning the encoder scrolls the list, walks the carousel, or changes a value,
always in the same direction. If it reads backwards on your hardware,
`ENCODER_INVERT` in `include/config.h` flips it in one place.

The carousel holds **Avgångar, Ljusstyrka, Färgtema, Hållplats, Nätverk** and
**System**. There are eight colour themes, and the whole interface follows the
one you pick.

Three things animate, all sharing one easing helper so they feel like the same
movement: the carousel slides between entries, the departures list slides when
it scrolls, and the theme list slides while its marker glides between rows.

The on/off switch fades the panel down rather than cutting it, and fades back up
starting from the logo. "Off" means a dark panel, not sleep — WiFi, the portal
and the polling keep running, so the departures are current the moment it comes
back.

## Configuration portal

The board runs a small web server. The network screen shows a QR code; scanning
it opens the settings page on the board's own address.

Setup is deliberately two stage. An unconfigured board starts its own open
network and shows a WiFi-join QR code — scanning that connects your phone and
the captive portal opens the page by itself. But a phone attached to the board's
own network has **no internet**, and the stop search needs it, so:

1. Pick your home network on the portal. The board connects and shows its LAN
   address as a new QR code.
2. Rejoin your home WiFi, scan that code, and search for your stop.

The stop search runs in the browser and queries SL directly — their API sends
`access-control-allow-origin: *`, so the 1.3 MB site list is fetched and
filtered by the phone and never touches the ESP32.

To reconfigure a board that already has credentials, hold the encoder button
while it powers up to force the setup network.

## Getting started

```bash
git clone https://github.com/mykhailbarvari/DepartureBoard.git
cd DepartureBoard
cp include/secrets.h.example include/secrets.h   # then fill in your details
pio run --target upload
```

`include/secrets.h` is gitignored and holds the WiFi credentials and the
starting stop id. The build stops with a clear message if it is missing.

Credentials only need to be right once: after the first boot everything can be
changed from the portal instead.

Library versions are pinned in `platformio.ini`, so a fresh clone builds without
any local Arduino library folder.

## Repo layout

| Path | What lives there |
|---|---|
| `src/main.cpp` | FreeRTOS tasks and the UI state machine |
| `lib/api/` | SL Transport API: fetch, parse, sort, fetch results |
| `lib/logic/` | screens, departure filtering, colour themes, demo mode |
| `lib/display/` | HUB75 panel, text rendering, bitmaps, brightness fading |
| `lib/ui/` | carousel, icons, logo, easing helper |
| `lib/qr/` | QR rendering with its own capacity check |
| `lib/portal/` | web server, settings page, embedded logo |
| `lib/settings/` | one struct, one place that talks to NVS |
| `lib/input/` | debouncing, encoder, short/long press gestures |
| `lib/font/` | the 12 px bitmap font |
| `tools/` | Python generators for the icons and the logo |
| `include/` | pin map, layout constants, tunables |

## Generated assets

The icons and the boot logo are **generated, not hand-edited**. Editing packed
hex by hand is how icon sets rot, so the shapes live as code:

```bash
python tools/gen_icons.py    # -> lib/ui/src/ui_icons.cpp
python tools/gen_logo.py     # -> lib/ui/src/ui_logo.cpp + portal_logo.h
```

Both print an ASCII preview of what they produced, so a shape can be judged
before anything is flashed.

`gen_logo.py` rasterises `MB_Labs.svg` in plain Python — it parses the path
data, flattens the curves and fills with nonzero winding. No SVG rasteriser is
needed as a build dependency, which matters because none was installed and the
file is potrace output using only `M m c l z`. The logo is stored as 4-bit alpha
rather than 1-bit: at this size it is mostly diagonals, and without graded edges
they break into stairsteps.

## Hardware
- [ESP32-S3 Nano (Waveshare)](https://www.waveshare.com/wiki/ESP32-S3-Nano)
- [RGB LED Matrix P2 64×64 (HUB75E)](https://www.waveshare.com/wiki/RGB-Matrix-P2-64x64) ×2 (daisy-chained)
- External 5V high-current power supply
- Custom HUB75E bonnet PCB (designed for this project)
- Rotary encoder with push button for on-device navigation
- Latching on/off switch
- Custom 3D-printed chassis (CAD designed)

## Software
- Visual Studio Code with the [PlatformIO](https://platformio.org/) extension
- Arduino framework (ESP32), FreeRTOS
- [ESP32-HUB75-MatrixPanel-DMA](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA) — panel driver
- [ArduinoJson](https://arduinojson.org/) — streaming JSON parser
- [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library) + BusIO
- [QRCode](https://github.com/ricmoo/QRCode) by ricmoo

---

## Notes on the SL API

Things that cost time to work out, written down so the next person does not have
to:

- **There is no way to look up a single stop.** `/v1/sites/<id>` answers
  `501 Not Implemented`, and the full site list is 1.3 MB across 6 514 stops —
  far too much for the device to parse. The stop's name arrives with every
  departure as `stop_area.name` instead, so the board takes it from data it was
  already fetching. Interchanges return several names under one site, so the
  most frequent one wins.
- **The API intermittently returns an empty list while service is running.** Two
  calls seconds apart returned 0 and then 18 departures. An empty list is only
  believed after three consecutive empty responses, otherwise a working board
  would blank itself at random.
- **Times come as local time without an offset.** `mktime` with `tm_isdst = -1`
  and `TZ` set to `CET-1CEST` reads them correctly year round.
- `expected` vs `scheduled` gives delay, `state` gives cancellations, and
  `deviations[]` carries disruption messages.

## What's next

The board does everything it was built to do and runs in my home, but a few
things are deliberately unfinished:

- **No unit tests.** Several pure functions — text fitting, time parsing,
  sorting — are testable without hardware, but there is no host compiler
  installed on my machine to run a `native` environment.
- **The station screen is temporary.** Walking time and direction moved to the
  portal; the on-device screen is still there and marked as such in the code.
- **Night dimming and OTA** are planned but unbuilt. The 16 MB partition table
  already reserves space for OTA.

---

## Custom PCB

Strictly speaking, this project did not require a custom PCB.
There are many existing HUB75 bonnets and breakout boards for the Nano DevKit form factor that would have worked just fine.

However, I chose to design a custom PCB to reduce cable clutter and minimize the physical footprint of the system.
More importantly, I wanted an introduction to PCB design and the basic workflow in KiCad.
Being able to see something I designed myself work as a real, physical part of the project was a big motivation.

Designing my own HUB75E bonnet PCB therefore became a natural step.

The PCB acts as a dedicated interface between the ESP32-S3 Nano and the LED matrix panels, keeping signal routing clean and making assembly much easier compared to loose jumper wires.

I designed both the schematic and the PCB layout in KiCad and ordered the board as part of the learning process.
The finished design was exported to Gerber files and manufactured via [JLCPCB](https://jlcpcb.com/).

### Revision note
My first PCB revision wasn't perfect.
I miscalculated the dev board length, which caused clearance issues with the HUB75E connector.
As a workaround, I replaced the HUB75E connector with a horizontal 16-pin GPIO header for the panel connection.

This turned out to be a valuable lesson in mechanical constraints and the importance of validating physical dimensions early in the design process.

This was my first PCB design, and it taught me a lot about planning, layout constraints, and thinking about both electronics and physical integration at the same time.

## PCB Images
<table>
  <tr>
    <td align="center">
      <b>3D PCB Model</b><br>
      <img src="assets/CustomPCB_3Dmodel.png" width="450">
    </td>
    <td align="center">
      <b>PCB Layout</b><br>
      <img src="assets/CustomPCB_layout.png" width="450">
    </td>
  </tr>
</table>

## CAD & Chassis Design
The LED matrix panels are daisy-chained, which meant the project quickly became more than just a small dev board on a table.
I wanted to design a proper enclosure that could house everything cleanly and turn the system into a single, cohesive unit.

The custom chassis was designed in CAD to fit the LED panels, PCB, and user controls, with attention to clearances, mounting points, and cable routing.
The focus was on creating a sturdy, clean enclosure rather than optimizing for the smallest possible size.

The chassis was 3D-printed and iterated alongside the electronics design, highlighting the importance of mechanical design even in relatively simple embedded systems.

---

## Licence

MIT — see [LICENSE](LICENSE).
