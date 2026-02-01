# Departure Board – ESP32 Transit Display

## 🚧🚧🚧 WORK IN PROGRESS🚧🚧🚧

A real-time LED departure board built as my first personal embedded systems project.

The system runs on an "ESP32-S3 Nano" by Waveshare and drives two daisy-chained HUB75E LED matrix panels to display live public transport departure data.

This project marks as my first personal project. Transitioning from theory to building real, physical systems.

## Why this project?
After finishing the courses IS1200 and IS1300 at KTH, I wanted to move beyond course assignments and theory to build something that i have more practical use of every day.

During the winter, I realized how useful a real-time departure display would be, due to the irregular traffic caused by the snow.
After some inspiration from existing transit boards, I decided to design and build my own — from hardware to software.

This project became a way to apply my coursework to a real-world problem.


Everything here was learned by building, breaking, and fixing.

## What does it do?
- Fetches real-time departure data from the Trafiklab API
- Drives two daisy-chained 64×64 P2 HUB75E LED matrix panels
- Renders text on a high-refresh, DMA-driven LED display
- Uses a rotary encoder to navigate a simple on-device UI menu
- Runs separate FreeRTOS tasks for data fetching, display rendering, and UI logic


## Hardware
- [ESP32-S3 Nano (Waveshare)](https://www.waveshare.com/wiki/ESP32-S3-Nano)
- [RGB LED Matrix P2 64×64 (HUB75E)](https://www.waveshare.com/wiki/RGB-Matrix-P2-64x64) ×2 (daisy-chained)
- External 5V high-current power supply
- Custom PCB (designed during the project)
- Rotary encoder with push button


## Software
- Arduino framework (ESP32)
- FreeRTOS
- DMA-driven HUB75E display driver
- ArduinoJson


## About me
I’m Mykhail, an ICT Engineering student at KTH, aiming for an MSc in Embedded Systems.  
This is my first project — It is not perfect, optimized or effective. But i am damn proud of it.
