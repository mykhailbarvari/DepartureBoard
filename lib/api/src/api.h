#pragma once
#include <stdbool.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t gDeparturesMutex;

bool api_fetch_departures(int siteId, int directionCode);

