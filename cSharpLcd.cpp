// cSharpLcd.cpp
//{{{  includes
#include "cSharpLcd.h"

#include <stdio.h>
#include <cstring>
#include <pthread.h>

#include "pigpio/pigpio.h"

using namespace std;
//}}}
static const int kSpiClock = 8000000;

//static const uint8_t CE1      =  7; // J8 - pin26
//static const uint8_t CE0      =  8; // J8 - pin24
//static const uint8_t MISO     =  9; // J8 - pin21
//static const uint8_t MOSI     = 10; // J8 - pin19 - used
//static const uint8_t CLK      = 11; // J8 - pin23 - used
static const uint8_t SCS      = 23; // J8 - pin16 - used
static const uint8_t DISP     = 24; // J8 - pin18 - used
static const uint8_t EXTCOMIN = 25; // J8 - pin22 - used

//{{{
cSharpLcd::cSharpLcd() {

  unsigned hardwareRevision = gpioHardwareRevision();
  unsigned version = gpioVersion();
  printf ("pigpio hwRev:%x version:%d\n", hardwareRevision, version);

  if (gpioInitialise() >= 0) {
    gpioSetMode (SCS, PI_OUTPUT);
    gpioSetMode (DISP, PI_OUTPUT);
    gpioSetMode (EXTCOMIN, PI_OUTPUT);

    pthread_t threadId;
    if (pthread_create (&threadId, NULL, &toggleVcomThread, 0))
      printf ("error creating toggleVcomThread\n");
    else
      printf ("toggleVcomThread created\n");

    // set CE0 active hi
    mHandle = spiOpen (0, kSpiClock, 0x00004);

    gpioWrite (SCS, 0);
    gpioWrite (DISP, 0);
    gpioWrite (EXTCOMIN, 0);

    // Memory LCD startup sequence with recommended timings
    gpioWrite (DISP, 1);
    gpioWrite (EXTCOMIN, 0);

    clearLine();
    clearDisplay();
    }
  else
    printf ("pigpio initialisation failed\n");
  }
//}}}
//{{{
cSharpLcd::~cSharpLcd() {

  turnOff();

  spiClose (mHandle);
  gpioTerminate();
  }
//}}}

//{{{
void cSharpLcd::clearDisplay() {

  gpioWrite (SCS, 1);
  spiWrite (mHandle, mClear, 2);
  gpioWrite (SCS, 0);

  clearFrame();
  }
//}}}
//{{{
void cSharpLcd::turnOff() {
  gpioWrite (DISP, 1);
  }
//}}}
//{{{
void cSharpLcd::turnOn() {
  gpioWrite (DISP, 0);
  }
//}}}

//{{{
void cSharpLcd::displayFrame() {

  gpioWrite (SCS, 1);
  spiWrite (mHandle, mFrameBuffer, kFrameBytes);
  gpioWrite (SCS, 0);
  }
//}}}

//{{{
void cSharpLcd::setLine() {
  memset (mLineBuffer, 0x00, kRowDataBytes);
  }
//}}}
//{{{
void cSharpLcd::clearLine() {
  memset (mLineBuffer, 0xFF, kRowDataBytes);
  }
//}}}
//{{{
void cSharpLcd::writeByteToLine (uint8_t byteNumber, uint8_t byteToWrite) {

  if (byteNumber < kRowDataBytes)
    mLineBuffer[byteNumber] = byteToWrite;
  }
//}}}
//{{{
void cSharpLcd::writePixelToLine (uint16_t pixel, bool on) {

  if (pixel < kWidth) {
    if (on)
      mLineBuffer[pixel/8] |=  (1 << (7 - (pixel % 8)));
    else
      mLineBuffer[pixel/8] &= ~(1 << (7 - (pixel % 8)));
    }
  }
//}}}
//{{{
void cSharpLcd::lineToFrame (uint8_t lineNumber) {
  memcpy (mFrameBuffer + (lineNumber * kRowBytes) + kRowHeader, mLineBuffer, kRowDataBytes);
  }
//}}}

//{{{
void cSharpLcd::setFrame() {

  auto frameBufferPtr = mFrameBuffer;
  for (int lineNumber = 1; lineNumber <= kHeight; lineNumber++) {
    *frameBufferPtr++ = 0b10000000;               // command byte
    *frameBufferPtr++ = reverseByte (lineNumber); // line number byte, numbered from 1
    memset (frameBufferPtr, 0x00, kRowDataBytes); // data 8 pixels to a byte
    frameBufferPtr += kRowDataBytes;
    }

  // trailing padding byte
  *frameBufferPtr++ = 0;
  }
//}}}
//{{{
void cSharpLcd::clearFrame() {

  auto frameBufferPtr = mFrameBuffer;
  for (int lineNumber = 1; lineNumber <= kHeight; lineNumber++) {
    *frameBufferPtr++ = 0b10000000;               // command byte
    *frameBufferPtr++ = reverseByte (lineNumber); // line number byte, numbered from 1
    memset (frameBufferPtr, 0xFF, kRowDataBytes); // data 8 pixels to a byte
    frameBufferPtr += kRowDataBytes;
    }

  // trailing padding byte
  *frameBufferPtr++ = 0;
  }
//}}}
//{{{
void cSharpLcd::writeByteToFrame (uint8_t byteNumber, uint8_t lineNumber, uint8_t byteToWrite) {
// char location expected in the fn args has been extrapolated from the pixel location
// format (see above), so chars go from 1 to kWidth/8, not from 0

  if ((byteNumber < kRowDataBytes) && (lineNumber < kHeight)) {
    mFrameBuffer[(lineNumber * kRowBytes) + 2 + byteNumber] = byteToWrite;
    }
  }
//}}}
//{{{
void cSharpLcd::writePixelToFrame (uint16_t pixel, uint8_t lineNumber, bool on) {

  if ((pixel < kWidth) && (lineNumber < kHeight)) {
    if (on)
      mFrameBuffer[(lineNumber * kRowBytes) + 2 + (pixel/8)] |=  (1 << (7 - pixel%8));
    else
      mFrameBuffer[(lineNumber * kRowBytes) + 2 + (pixel/8)] &= ~(1 << (7 - pixel%8));
    }
  }
//}}}

// private
//{{{
uint8_t cSharpLcd::reverseByte (uint8_t b) {
// reverses the bit order of an unsigned char

  b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4);
  b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2);
  return ((b & 0xAA) >> 1) | ((b & 0x55) << 1);
  }
//}}}
//{{{
void* cSharpLcd::toggleVcomThread (void* arg) {

  printf ("toggleVcomThread running\n");
  while (true) {
    gpioDelay (250000);
    gpioWrite (EXTCOMIN, 1);
    gpioDelay (250000);
    gpioWrite (EXTCOMIN, 0);
    }

  pthread_exit (NULL);
  }
//}}}
