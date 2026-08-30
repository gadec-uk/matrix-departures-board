/*
 * Matrix Departures Board (c) 2026 Gadec Software
 *
 * touchSensor Library - supports TTP223 and all simple momentary contact switches
 *
 * https://github.com/gadec-uk/matrix-departures-board
 *
 * This work is licensed under Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/
 */

#pragma once
#include <Arduino.h>

class touchSensor {
  private:
    uint8_t _pin;
    bool lastState = LOW;
    bool currentState = LOW;
    unsigned long lastDebounceMs = 0;
    unsigned long debounceDelay = 80;
    unsigned long touchStartedMs = false;
    bool shortTapDetected = false;
    bool longTapDetected = false;
    unsigned long longTapMs = 800;
    unsigned long lastTapTime = 0;

  public:
    touchSensor(uint8_t pin);
    void updateTouchState();
    bool isTouched();
    bool wasShortTapped();
    bool wasLongTapped();
    int secsSinceLastTap();
    void setLongTapTime(unsigned long ms);
    void setDebounceTime(unsigned long ms);
};
