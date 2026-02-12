#pragma once
#include <Arduino.h>
#include <stdbool.h>
#include <config.h>

// Button press tracker to eliminate repetitive state management
typedef struct {
  bool current;
  bool lastState;
} ButtonTracker;

void input_init(void);
void input_update(void);

bool input_onoff(void);
bool input_select(void);
bool input_encoderUp(void);
bool input_encoderDown(void);

// Helper: Detect rising edge (press event) of a button
// Initialize tracker with {false, false}
bool input_isPressed(ButtonTracker &tracker, bool currentState);
