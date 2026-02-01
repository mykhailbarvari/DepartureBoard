# Departure Board – ESP32 Transit Display

## 🚧🚧🚧 WORK IN PROGRESS🚧🚧🚧

A real-time LED departure board built as my first personal embedded systems project.

The system runs on a "ESP32-S3 Nano" by Waveshare and drives two daisy-chained HUB75E LED matrix panels to display live public transport departure data.

This project marks as my first personal project. Transitioning from theory to building real, physical systems.

---

## Why this project?
After finishing the courses IS1300 Embedded Systems and IS1200 Computer Hardware Engineering at my university, I wanted to move beyond course assignments and theory and build something with practical, everyday use.

During the winter, bus traffic became irregular due to snow, and I often found myself standing in the cold because the scheduled departure times were inaccurate.
That’s when I realized how useful a real-time, always-on departure display would be.

Beyond functionality, I wanted to build something I would actually want to keep in my home. Something useful, but also nice to look at and be proud to show friends.

While searching for inspiration online, I stumbled upon the departure boards built by 
[T-Skylt](https://shop.t-skylt.se/). Their work was a major source of inspiration for this project, especially in terms of concept and overall aesthetic.

---

## What does it do?
- Fetches real-time departure data from the Trafiklab API
- Drives two daisy-chained 64×64 P2 HUB75E LED matrix panels
- Renders text on a high-refresh, DMA-driven LED display
- Uses a rotary encoder to navigate a simple on-device UI menu
- Runs separate FreeRTOS tasks for data fetching, display rendering, and UI logic

---

## Hardware
- [ESP32-S3 Nano (Waveshare)](https://www.waveshare.com/wiki/ESP32-S3-Nano)
- [RGB LED Matrix P2 64×64 (HUB75E)](https://www.waveshare.com/wiki/RGB-Matrix-P2-64x64) ×2 (daisy-chained)
- External 5V high-current power supply
- Custom HUB75E bonnet PCB (designed for this project)
- Rotary encoder for on-device navigation
- On/off power switch
- Custom 3D-printed chassis (CAD designed)

---

## Software
- Visual Studio Code with the [PlatformIO](https://platformio.org/) extension
- Arduino framework (ESP32)
- FreeRTOS
- [HUB75E LED display driver](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA) (by mrcodetastic)  
- ArduinoJson

---

## About me
I’m "Misha", an ICT Engineering student at KTH, aiming for an MSc in Embedded Systems.  
This is my first project — It is not perfect, optimized or effective. But i am damn proud of it.
