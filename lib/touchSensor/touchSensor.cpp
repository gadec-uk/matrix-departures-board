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

#include "touchSensor.h"

touchSensor::touchSensor(uint8_t pin) {
  _pin = pin;
  pinMode(_pin, INPUT);
}

void touchSensor::updateTouchState() {
  bool state = digitalRead(_pin);

  if (state != lastState) {
    lastDebounceMs = millis();
  }

  if ((millis()-lastDebounceMs) > debounceDelay) {
    if (state != currentState) {
      currentState = state;
      if (currentState == HIGH) {
        touchStartedMs = millis();
        shortTapDetected = false;
        longTapDetected = false;
        doubleTapDetected = false;
      } else {
        unsigned long touchLengthMs = millis()-touchStartedMs;
        if (touchLengthMs >= minimumTapMs) {
          if (touchLengthMs < longTapMs) {
            if (waitingForDoubleTap && ((millis() - firstTapEndTimeMs) < doubleTapWindowMs)) {
              doubleTapDetected = true;
              waitingForDoubleTap = false;
              lastTapTime = millis();
            } else {
              waitingForDoubleTap = true;
              firstTapEndTimeMs = millis();
            }
          } else {
            longTapDetected = true;
            waitingForDoubleTap = false;
            lastTapTime = millis();
          }
        }
      }
    }
  }

  if (waitingForDoubleTap && ((millis() - firstTapEndTimeMs) >= doubleTapWindowMs)) {
    shortTapDetected = true;
    waitingForDoubleTap = false;
    lastTapTime = firstTapEndTimeMs;
  }

  lastState = state;
}

bool touchSensor::isTouched() {
  return currentState;
}

bool touchSensor::wasShortTapped() {
  if (shortTapDetected) {
    shortTapDetected = false;
    return true;
  } else return false;
}

bool touchSensor::wasLongTapped() {
  if (longTapDetected) {
    longTapDetected = false;
    return true;
  } else return false;
}

bool touchSensor::wasDoubleTapped() {
  if (doubleTapDetected) {
    doubleTapDetected = false;
    return true;
  } else return false;
}

int touchSensor::secsSinceLastTap() {
  return ((millis()-lastTapTime)/1000);
}

void touchSensor::setLongTapTime(unsigned long ms) {
  longTapMs = ms;
}

void touchSensor::setDebounceTime(unsigned long ms) {
  debounceDelay = ms;
}

void touchSensor::setDoubleTapWindow(unsigned long ms) {
  doubleTapWindowMs = ms;
}