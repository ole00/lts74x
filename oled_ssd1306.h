/*
*****************************************************************************
@file         Deneyap_OLED.h
@mainpage     Deneyap OLED Display Module SSD1306 Arduino library header file
@version      v1.0.3
@date         September 20, 2022
@brief        This file contains all function prototypes and macros
              for Deneyap OLED Display Module SSD1306 Arduino library
*****************************************************************************
*/

#ifndef __DENEYAP_OLED_H
#define __DENEYAP_OLED_H

#include <Wire.h>

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#ifdef __AVR__
#include <avr/pgmspace.h>
#define OLEDFONT(name) static const uint8_t __attribute__((progmem)) name[]
#elif defined(ESP8266)
#include <pgmspace.h>
#define OLEDFONT(name) static const uint8_t name[]
#else
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#define OLEDFONT(name) static const uint8_t name[]
#endif

#include "font7x8.h"

  
  bool oled_begin(uint8_t address, TwoWire &wirePort = Wire);
  void oled_clear_display();
  void oled_set_brightness(unsigned char Brightness);
  void oled_set_y(unsigned char y);
  void oled_draw_h_line(unsigned char x1, unsigned char x2, unsigned char y, unsigned char color);
  void oled_draw_v_line(unsigned char y1, unsigned char y2, unsigned char x);

  void oled_draw_string(char* string, bool inverse);
  void oled_set_inverse(bool inverse);

#endif  // End of __DENEYAP_OLED_H definition check
