#include "LedBlink.h"

#include <Arduino.h>

// LED PWM controllers
// PWM controllers
#define LEDC_RED 0
#define LEDC_GREEN 1
#define LEDC_BLUE 2 

void ledBlinkSetup() {

    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);

    ledcSetup(LEDC_RED, 5000, 8);      // channel for red LED, 5kHz, 8-bit resolution
    ledcAttachPin(LED_RED, LEDC_RED);
    ledcSetup(LEDC_GREEN, 5000, 8);    // channel for green LED
    ledcAttachPin(LED_GREEN, LEDC_GREEN);
    ledcSetup(LEDC_BLUE, 5000, 8);     // channel for blue LED
    ledcAttachPin(LED_BLUE, LEDC_BLUE);

}

void ledBlinkLoop() {

  static int hueDegrees = 0;

  hueDegrees+=6; if (hueDegrees >= 360) hueDegrees = 0;
  float hueRadians = hueDegrees * 3.14159 / 180.0;

  // Simple HSV→RGB-like approximation
  int r = (sin(hueRadians) * 127 + 128);
  int g = (sin(hueRadians + 2.09439) * 127 + 128);
  int b = (sin(hueRadians + 4.18878) * 127 + 128);

  ledcWrite(LEDC_RED, 255 - (r/5));
  ledcWrite(LEDC_GREEN, 255 - (g/5));
  ledcWrite(LEDC_BLUE, 255 - (b/5));
}