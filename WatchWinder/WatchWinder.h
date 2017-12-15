/*
  Morse.h - Library for flashing Morse code.
  Created by David A. Mellis, November 2, 2007.
  Released into the public domain.
*/
#ifndef WatchWinder_h
#define WatchWinder_h

#include "Arduino.h"
#include <SparkFun_TB6612.h>

class WatchWinder
{
  public:
    WatchWinder(Motor motor, int turnsPerDay);
    void dot();
    void dash();
  private:
    int _pin;
};

#endif
