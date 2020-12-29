// cTouchscreen.h
#pragma once
#include <cstdint>
#include <string>
#include "lcd/cPointRect.h"
#include "pigpio/pigpioLite.h"
#include "../shared/utils/utils.h"
#include "../shared/utils/cLog.h"

// J8
// aux miso gpio19 - 35  36 - gpi16 aux ce2
//     pirq gpio26 - 37  38 - gpi20 aux mosi
//              0v - 39  40 - gpi21 aux sclk
constexpr uint8_t kSpiTouchCeGpio = 16;
constexpr uint8_t kPirqGpio = 26;
constexpr int kSpiClockXT2406 = 500000;

#define CTRL_START 0x80 // start Bit
#define CTRL_8BIT  0x08 // mode
#define CTRL_DFR   0x04 // ser/dfr
#define CTRL_PD1   0x02 // pd1
#define CTRL_PD0   0x01 // pd0

class cTouchscreen {
public:
  cTouchscreen() {}
  ~cTouchscreen() { spiClose (mSpiHandle); }

  //{{{
  void init() {

    gpioSetMode (kSpiTouchCeGpio, PI_OUTPUT);
    //gpioWrite (kSpiTouchCeGpio, 1);

    gpioSetMode (kPirqGpio, PI_INPUT);

    mSpiHandle = spiOpen (2, kSpiClockXT2406, 0x0160);

    touchCommand (CTRL_START | 0x10);
    }
  //}}}

  //{{{
  bool getTouchDown() {

    return gpioRead (kPirqGpio) == 0;
    }
  //}}}
  //{{{
  bool getTouchPos (int16_t* x, int16_t* y, int16_t* z, int16_t xlen, uint16_t ylen) {

    #define maxSamples 7

    uint16_t xsamples[maxSamples];
    uint16_t ysamples[maxSamples];

    uint16_t samples = 0;
    while (getTouchDown() && (samples < maxSamples)) {
      int x =  touchCommand (CTRL_START | 0x10 | CTRL_PD1 | CTRL_PD0);
      int y =  touchCommand (CTRL_START | 0x50 | CTRL_PD1 | CTRL_PD0);
      //cLog::log (LOGINFO, "x:" + dec(x) + " y: " + dec(y));
      xsamples[samples] = x;
      ysamples[samples] = y;
      samples++;
      }

    int16_t xsample = torben (xsamples, samples);
    int16_t ysample = torben (ysamples, samples);

    *x = ((xsample - 170) * xlen) / (1920-170);
    *y = ((ysample - 140) * ylen) / (1930-140);
    if ((*x) < 0)
      *x = 0;
    if ((*y) < 0)
      *y = 0;
    *z = 0;

    touchCommand (CTRL_START | 0x10);
    return samples == maxSamples;
    }
  //}}}

private:
  //{{{
  uint32_t touchCommand (uint8_t command) {

    //gpioWrite (kSpiTouchCeGpio, 0);
    uint8_t buf[3] = { command, 0, 0 };
    spiXfer (mSpiHandle, (char*)buf, (char*)buf, 3);
    //gpioWrite (kSpiTouchCeGpio, 1);

    //cLog::log (LOGINFO, hex(int(buf2[0]),2) + " " +  hex(int(buf2[1]),2)  +" " + hex(int(buf2[2]),2));
    return ((buf[1] << 8) | buf[2]) >> 4;
    }
  //}}}
  //{{{
  uint16_t torben (uint16_t m[], int n) {
  // torben median

    uint16_t min = m[0];
    uint16_t max = m[0];

    for (int i = 1 ; i < n ; i++) {
      if (m[i] < min)
        min = m[i];
      if (m[i] > max)
        max = m[i];
      }

    int less;
    int greater;
    int equal;
    uint16_t guess;
    uint16_t maxltguess;
    uint16_t mingtguess;
    while (true) {
      guess = (min + max) / 2;

      less = 0;
      greater = 0;
      equal = 0;
      maxltguess = min;
      mingtguess = max;

      for (int i = 0; i < n; i++) {
        if (m[i] < guess) {
          less++;
          if (m[i]>maxltguess)
            maxltguess = m[i];
          }
        else if (m[i]>guess) {
          greater++;
          if (m[i]<mingtguess)
            mingtguess = m[i];
          }
        else
          equal++;
        }

      if ((less <= (n+1)/2) && (greater <= (n+1)/2))
        break ;
      else if (less > greater)
        max = maxltguess;
      else
        min = mingtguess;
      }

    if (less >= (n+1)/2)
      return maxltguess;
    else if (less + equal >= (n+1)/2)
      return guess;
    else
      return mingtguess;
    }
  //}}}

  int mSpiHandle = 0;
  };
