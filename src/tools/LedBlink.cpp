#include "LedBlink.h"

#include <Arduino.h>

void ledBlinkSetup() {

    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);

    ledcAttach(LED_RED, 5000, 8);      // channel for red LED, 5kHz, 8-bit resolution
    ledcAttach(LED_GREEN, 5000, 8);    // channel for green LED
    ledcAttach(LED_BLUE, 5000, 8);     // channel for blue LED

}

void ledBlinkLoop() {

  static int hueDegrees = 0;

  hueDegrees+=6; if (hueDegrees >= 360) hueDegrees = 0;
  float hueRadians = hueDegrees * 3.14159 / 180.0;

  // Simple HSV→RGB-like approximation
  int r = (sin(hueRadians) * 127 + 128);
  int g = (sin(hueRadians + 2.09439) * 127 + 128);
  int b = (sin(hueRadians + 4.18878) * 127 + 128);

  ledcWrite(LED_RED, 255 - (r/5));
  ledcWrite(LED_GREEN, 255 - (g/5));
  ledcWrite(LED_BLUE, 255 - (b/5));
}