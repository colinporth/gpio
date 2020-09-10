// cTouchscreen.h
#pragma once
#include <cstdint>
#include <string>
#include "lcd/cPointRect.h"
#include "pigpio/pigpio.h"
#include "../shared/utils/utils.h"
#include "../shared/utils/cLog.h"

// J8
// aux miso gpio19 - 35  36 - gpi16 aux ce2
//     pirq gpio26 - 37  38 - gpi20 aux mosi
//              0v - 39  40 - gpi21 aux sclk
constexpr uint8_t kSpiTouchCeGpio = 7;
constexpr uint8_t kSpiLcdCeGpio = 8;
constexpr uint8_t kPirqGpio = 26;

constexpr int kSpiClockXT2406 = 100000;

#define ADS_CTRL_START          0x80 // Start Bit
#define ADS_CTRL_EIGHT_BITS_MOD 0x08 // Mode
#define ADS_CTRL_DFR            0x04 // SER/DFR
#define ADS_CTRL_PD1            0x02 // PD1
#define ADS_CTRL_PD0            0x01 // PD0

#define CMD_ENABLE_PENIRQ  ADS_CTRL_START | 0x10
#define CMD_X_POSITION     ADS_CTRL_START | 0x10 | ADS_CTRL_PD1 | ADS_CTRL_PD0
#define CMD_Y_POSITION     ADS_CTRL_START | 0x50 | ADS_CTRL_PD1 | ADS_CTRL_PD0

class cTouchscreen {
public:
  //{{{
  cTouchscreen() {
    }
  //}}}
  //{{{
  ~cTouchscreen() {
    spiClose (mSpiHandle);
    }
  //}}}

  //{{{
  void init() {
    gpioInitialise();

    gpioSetMode (kSpiTouchCeGpio, PI_OUTPUT);
    gpioWrite (kSpiTouchCeGpio, 1);

    gpioSetMode (kSpiLcdCeGpio, PI_OUTPUT);
    gpioWrite (kSpiLcdCeGpio, 1);

    gpioSetMode (kPirqGpio, PI_INPUT);

    mSpiHandle = spiOpen (0, kSpiClockXT2406, 0x00E3);

    touchCommand (CMD_ENABLE_PENIRQ);
    }
  //}}}

  //{{{
  bool getTouchDown() {

    return gpioRead (kPirqGpio) == 0;
    }
  //}}}
  //{{{
  void getTouchPos (int16_t* x, int16_t* y, int16_t* z, int16_t xlen, uint16_t ylen) {

    #define maxSamples 7
    uint16_t xsamples[maxSamples];
    uint16_t ysamples[maxSamples];

    uint16_t samples = 0;
    while (samples < maxSamples) {
      xsamples[samples] = touchCommand (CMD_X_POSITION);
      ysamples[samples] = touchCommand (CMD_Y_POSITION);
      samples++;
      }

    int16_t xsample = torben (xsamples, samples);
    int16_t ysample = torben (ysamples, samples);

    //*x = -10 +  (ysample * xlen) / 1900;
    //*y = -10 + ((2048 - xsample) * ylen)/ 1900;
    cLog::log (LOGINFO,
               "x:" + dec(xsample) + " y: " + dec(ysample) + " " +
               dec(xsamples[0]) + ", " + dec(ysamples[0]) + " " +
               dec(xsamples[1]) + ", " + dec(ysamples[1]) + " " +
               dec(xsamples[2]) + ", " + dec(ysamples[2]) + " " +
               dec(xsamples[3]) + ", " + dec(ysamples[3]) + " " +
               dec(xsamples[4]) + ", " + dec(ysamples[4]) + " " +
               dec(xsamples[5]) + ", " + dec(ysamples[5]) + " " +
               dec(xsamples[6]) + ", " + dec(ysamples[6]));

    *x = xsample;
    *y = ysample;
    *z = 0;

    touchCommand (CMD_ENABLE_PENIRQ);
    }
  //}}}

private:
  //{{{
  uint32_t touchCommand (uint8_t command) {

    gpioWrite (kSpiTouchCeGpio, 0);

    uint8_t buf[3] = { command, 0, 0 };
    spiXfer (mSpiHandle, (char*)buf, (char*)buf, 3);

    gpioWrite (kSpiTouchCeGpio, 1);

    int b0 = buf[0];
    int b1 = buf[1];
    int b2 = buf[2];
    cLog::log (LOGINFO, hex(int(b0),2) + " " +  hex(int(b1),2)  +" " + hex(int(b2),2));

    return ((buf[1] << 8) | buf[2]) >> 4;
    }
  //}}}
  //{{{
  uint16_t torben (uint16_t m[], int n) {

    int i, less, greater, equal;
    uint16_t  min, max, guess, maxltguess, mingtguess;

    min = max = m[0] ;
    for (i = 1 ; i < n ; i++) {
      if (m[i] < min)
        min = m[i];
      if (m[i] > max)
        max = m[i];
      }

    while (1) {
      guess = (min+max)/2;

      less = 0;
      greater = 0;
      equal = 0;
      maxltguess = min ;
      mingtguess = max ;

      for (i = 0; i < n; i++) {
        if (m[i] < guess) {
          less++;
          if (m[i]>maxltguess)
            maxltguess = m[i] ;
          }
        else if (m[i]>guess) {
          greater++;
          if (m[i]<mingtguess)
            mingtguess = m[i] ;
          }
        else
          equal++;
        }

      if (less <= (n+1)/2 && greater <= (n+1)/2)
        break ;
      else if (less>greater)
        max = maxltguess ;
      else
        min = mingtguess;
      }

    if (less >= (n+1)/2)
      return maxltguess;
    else if (less+equal >= (n+1)/2)
      return guess;
    else
      return mingtguess;
    }
  //}}}

  int mSpiHandle = 0;
  };
