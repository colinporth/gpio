// cSharpLcd.cpp
//{{{  includes
#include "cSharpLcd.h"

#include <stdio.h>
#include <cstring>
#include <pthread.h>

#include "pigpio/pigpio.h"

using namespace std;
//}}}
static const char SCS      = 23; // J8 - pin16 - used
static const char DISP     = 24; // J8 - pin18 - used
static const char EXTCOMIN = 25; // J8 - pin22 - used

static const char CE1      =  7; // J8 - pin26
static const char CE0      =  8; // J8 - pin24
static const char MISO     =  9; // J8 - pin21
static const char MOSI     = 10; // J8 - pin19 - used
static const char CLK      = 11; // J8 - pin23 - used

static const int kSpiClock = 8000000;

// Delay constants for LCD timing
#define PWRUP_DISP_DELAY      40  // > 30us
#define PWRUP_EXTCOMIN_DELAY  40  // > 30us
#define SCS_HIGH_DELAY         3  // > 3us
#define SCS_LOW_DELAY          1  // > 1us
#define INTERFRAME_DELAY       1  // > 1us

//{{{
cSharpLcd::cSharpLcd() {

  unsigned hardwareRevision = gpioHardwareRevision();
  unsigned version = gpioVersion();
  printf ("pigpio hwRev:%d version:%d\n", hardwareRevision, version);

  if (gpioInitialise() >= 0) {
    gpioSetMode (SCS, PI_OUTPUT);
    gpioSetMode (DISP, PI_OUTPUT);
    gpioSetMode (EXTCOMIN, PI_OUTPUT);

    // initialise private vars
    mCommandByte = 0b10000000;
    mClearByte   = 0b00100000;
    mPaddingByte = 0b00000000;

    pthread_t threadId;
    if (pthread_create (&threadId, NULL, &toggleVcomThread, 0))
      printf ("error creating toggleVcomThread\n");
    else
      printf ("toggleVcomThread created\n");

    mHandle = spiOpen (0, kSpiClock, 0);

    gpioWrite (SCS, 0);
    gpioWrite (DISP, 0);
    gpioWrite (EXTCOMIN, 0);

    // Memory LCD startup sequence with recommended timings
    gpioWrite (DISP, 1);
    gpioDelay (PWRUP_DISP_DELAY);
    gpioWrite (EXTCOMIN, 0);
    gpioDelay (PWRUP_EXTCOMIN_DELAY);

    clearLineBuffer();
    }
  else
    printf ("pigpio initialisation failed\n");
  }
//}}}
//{{{
cSharpLcd::~cSharpLcd() {

  spiClose (mHandle);
  gpioTerminate();
  }
//}}}

//{{{
void cSharpLcd::clearDisplay() {

  gpioWrite (SCS, 1);
  gpioDelay (SCS_HIGH_DELAY);
  spiWrite (mHandle, &mClearByte, 1);
  spiWrite (mHandle, &mPaddingByte, 1);
  gpioDelay (SCS_LOW_DELAY);
  gpioWrite (SCS, 0);

  gpioDelay (INTERFRAME_DELAY);
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
void cSharpLcd::writeLinesToDisplay (int lineNumber, int numLines, char* linesData) {

  gpioWrite (SCS, 1);
  gpioDelay (SCS_HIGH_DELAY);
  spiWrite (mHandle, &mCommandByte, 1);

  for (int i = 0; i < numLines; i++) {
    char reversedLineNumber = reverseByte (lineNumber++);
    spiWrite (mHandle, &reversedLineNumber, 1);
    spiWrite (mHandle, linesData++, kWidth/8);
    spiWrite (mHandle, &mPaddingByte, 1);
    }

  spiWrite (mHandle, &mPaddingByte, 1);
  gpioDelay (SCS_LOW_DELAY);
  gpioWrite (SCS, 0);
  gpioDelay (INTERFRAME_DELAY);
  }
//}}}
void cSharpLcd::writeLineToDisplay (int lineNumber, char* lineData) { writeLinesToDisplay (lineNumber, 1, lineData); }
void cSharpLcd::writeLineBufferToDisplay (int lineNumber) { writeLinesToDisplay (lineNumber, 1, mLineBuffer); }
void cSharpLcd::writeFrameBufferToDisplay() { writeLinesToDisplay (1, kHeight, mFrameBuffer); }

void cSharpLcd::clearLineBuffer() { memset (mLineBuffer, 0xFF, kWidth/8); }
void cSharpLcd::setLineBuffer() { memset (mLineBuffer, 0x00, kWidth/8); }
//{{{
void cSharpLcd::writeByteToLineBuffer (int byteNumber, char byteToWrite) {
// char location expected in the fn args has been extrapolated from the pixel location
// format (see above), so chars go from 1 to kWidth/8, not from 0

  if (byteNumber <= kWidth/8 && byteNumber != 0) {
    byteNumber -= 1;
    mLineBuffer[byteNumber] = byteToWrite;
    }
  }
//}}}
//{{{
void cSharpLcd::writePixelToLineBuffer (int pixel, bool isWhite) {
// pixel location expected in the fn args follows the scheme defined in the datasheet.
// NB: the datasheet defines pixel addresses starting from 1, NOT 0

  if ((pixel <= kWidth) && (pixel != 0)) {
    pixel = pixel - 1;
    if (isWhite)
      mLineBuffer[pixel/8] |=  (1 << (7 - pixel%8));
    else
      mLineBuffer[pixel/8] &= ~(1 << (7 - pixel%8));
    }
  }
//}}}

void cSharpLcd::clearFrameBuffer() { memset (mFrameBuffer, 0xFF, kWidth*kHeight/8); }
void cSharpLcd::setFrameBuffer() { memset (mFrameBuffer, 0x00, kWidth*kHeight/8); }
//{{{
void cSharpLcd::writeByteToFrameBuffer (int byteNumber, int lineNumber, char byteToWrite) {
// char location expected in the fn args has been extrapolated from the pixel location
// format (see above), so chars go from 1 to kWidth/8, not from 0

  if ((byteNumber <= kWidth/8) && (byteNumber != 0) && (lineNumber <= kHeight) & (lineNumber != 0)) {
    byteNumber -= 1;
    lineNumber -= 1;
    mFrameBuffer[(lineNumber*kWidth/8)+byteNumber] = byteToWrite;
    }
  }
//}}}
//{{{
void cSharpLcd::writePixelToFrameBuffer (unsigned int pixel, int lineNumber, bool isWhite) {
// pixel location expected in the fn args follows the scheme defined in the datasheet.
// NB: the datasheet defines pixel addresses starting from 1, NOT 0

  if ((pixel <= kWidth) && (pixel != 0) && (lineNumber <= kHeight) & (lineNumber != 0)) {
    pixel -= 1;
    lineNumber -= 1;
    if(isWhite)
      mFrameBuffer[(lineNumber*kWidth/8)+(pixel/8)] |=  (1 << (7 - pixel%8));
    else
      mFrameBuffer[(lineNumber*kWidth/8)+(pixel/8)] &= ~(1 << (7 - pixel%8));
    }
  }
//}}}

// private
//{{{
char cSharpLcd::reverseByte (int b) {
// reverses the bit order of an unsigned char

  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
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
