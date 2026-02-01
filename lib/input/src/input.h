#pragma once
#include <Arduino.h>
#include <stdbool.h>
#include <config.h>

void input_init(void);
void input_update(void);

bool input_onoff(void);
bool input_nav1(void);
bool input_nav2(void);
bool input_select(void);
