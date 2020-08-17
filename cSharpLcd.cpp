// cSharpLcd.cpp
//{{{  includes
#include "cSharpLcd.h"

#include <unistd.h>
#include <cstring>
#include <pthread.h>
#include <iostream>

#include "pigpio/pigpio.h"

using namespace std;
//}}}
static const char SCS      = 23; // J8 - pin16
static const char DISP     = 24; // J8 - pin18
static const char EXTCOMIN = 25; // J8 - pin22
static const char MOSI     = 10; // J8 - pin19
static const char MISO     =  9; // J8 - pin21
static const char CLK      = 11; // J8 - pin23
static const char CE0      =  8; // J8 - pin24
static const char CE1      =  7; // J8 - pin26

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
    commandByte = 0b10000000;
    vcomByte    = 0b01000000;
    clearByte   = 0b00100000;
    paddingByte = 0b00000000;

    // setup separate thread to continuously output the EXTCOMIN signal for as long as the parent runs.
    // NB: this leaves the Memory LCD vulnerable if an image is left displayed after the program stops.
    pthread_t threadId;
    if (pthread_create (&threadId, NULL, &hardToggleVCOM, 0))
      cout << "Error creating EXTCOMIN thread" << endl;
    else
      cout << "PWM thread started successfully" << endl;

    handle = spiOpen (0, kSpiClock, 0);

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

  spiClose (handle);
  gpioTerminate();
  }
//}}}

//{{{
void cSharpLcd::clearDisplay() {

  gpioWrite (SCS, 1);
  gpioDelay (SCS_HIGH_DELAY);
  spiWrite (handle, &clearByte, 1);
  spiWrite (handle, &paddingByte, 1);
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
  spiWrite (handle, &commandByte, 1);

  for (int i = 0; i < numLines; i++) {
    char reversedLineNumber = reverseByte (lineNumber++);
    spiWrite (handle, &reversedLineNumber, 1);
    spiWrite (handle, linesData++, kWidth/8);
    spiWrite (handle, &paddingByte, 1);
    }

  spiWrite (handle, &paddingByte, 1);
  gpioDelay (SCS_LOW_DELAY);
  gpioWrite (SCS, 0);
  gpioDelay (INTERFRAME_DELAY);
  }
//}}}
void cSharpLcd::writeLineToDisplay (int lineNumber, char* lineData) { writeLinesToDisplay (lineNumber, 1, lineData); }
void cSharpLcd::writeLineBufferToDisplay (int lineNumber) { writeLinesToDisplay (lineNumber, 1, lineBuffer); }
void cSharpLcd::writeFrameBufferToDisplay() { writeLinesToDisplay (1, kHeight, frameBuffer); }

void cSharpLcd::clearLineBuffer() { memset (lineBuffer, 0xFF, kWidth/8); }
void cSharpLcd::setLineBuffer() { memset (lineBuffer, 0x00, kWidth/8); }
//{{{
void cSharpLcd::writeByteToLineBuffer (int byteNumber, char byteToWrite) {
// char location expected in the fn args has been extrapolated from the pixel location
// format (see above), so chars go from 1 to kWidth/8, not from 0

  if (byteNumber <= kWidth/8 && byteNumber != 0) {
    byteNumber -= 1;
    lineBuffer[byteNumber] = byteToWrite;
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
      lineBuffer[pixel/8] |=  (1 << (7 - pixel%8));
    else
      lineBuffer[pixel/8] &= ~(1 << (7 - pixel%8));
    }
  }
//}}}

void cSharpLcd::clearFrameBuffer() { memset (frameBuffer, 0xFF, kWidth*kHeight/8); }
void cSharpLcd::setFrameBuffer() { memset (frameBuffer, 0x00, kWidth*kHeight/8); }
//{{{
void cSharpLcd::writeByteToFrameBuffer (int byteNumber, int lineNumber, char byteToWrite) {
// char location expected in the fn args has been extrapolated from the pixel location
// format (see above), so chars go from 1 to kWidth/8, not from 0

  if ((byteNumber <= kWidth/8) && (byteNumber != 0) && (lineNumber <= kHeight) & (lineNumber != 0)) {
    byteNumber -= 1;
    lineNumber -= 1;
    frameBuffer[(lineNumber*kWidth/8)+byteNumber] = byteToWrite;
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
      frameBuffer[(lineNumber*kWidth/8)+(pixel/8)] |=  (1 << (7 - pixel%8));
    else
      frameBuffer[(lineNumber*kWidth/8)+(pixel/8)] &= ~(1 << (7 - pixel%8));
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
void* cSharpLcd::hardToggleVCOM (void* arg) {
//char extcomin = (char)arg;

  while (true) {
    gpioDelay (250000);
    gpioWrite (EXTCOMIN, 1);
    gpioDelay (250000);
    gpioWrite (EXTCOMIN, 0);
    }

  pthread_exit (NULL);
  }
//}}}
