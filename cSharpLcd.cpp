#include "cSharpLcd.h"

#include <bcm2835.h>
#include <unistd.h>
#include <pthread.h>
#include <iostream>

// Delay constants for LCD timing
#define PWRUP_DISP_DELAY  40      // > 30us
#define PWRUP_EXTCOMIN_DELAY  40  // > 30us
#define SCS_HIGH_DELAY    3       // > 3us
#define SCS_LOW_DELAY   1         // > 1us
#define INTERFRAME_DELAY  1       // > 1us

using namespace std;

//{{{
cSharpLcd::cSharpLcd (char SCSpin, char DISPpin, char EXTCOMINpin, bool useEXTCOMIN) {

  bcm2835_init();

  // Initialise private vars (constructor args)
  // Set DISPpin = 255, when you call the constructor if you want to save a pin
  // and not have hardware control of _DISP
  SCS  = SCSpin;
  DISP = DISPpin;
  EXTCOMIN = EXTCOMINpin;
  enablePWM = useEXTCOMIN;

  bcm2835_gpio_fsel (SCS, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_fsel (DISP, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_fsel (EXTCOMIN, BCM2835_GPIO_FSEL_OUTP);

  // initialise private vars (others)
  commandByte = 0b10000000;
  vcomByte    = 0b01000000;
  clearByte   = 0b00100000;
  paddingByte = 0b00000000;

  // Introduce delay to allow cSharpLcd to reach 5V
  // (probably redundant here as Pi's boot is much longer than Arduino's power-on time)
  bcm2835_delayMicroseconds (800);  // minimum 750us

  // setup separate thread to continuously output the EXTCOMIN signal for as long as the parent runs.
  // NB: this leaves the Memory LCD vulnerable if an image is left displayed after the program stops.
  pthread_t threadId;
  if (enablePWM) {
    if (pthread_create (&threadId, NULL, &hardToggleVCOM, (void*)EXTCOMIN)) {
      cout << "Error creating EXTCOMIN thread" << endl;
      }
    else {
      cout << "PWM thread started successfully" << endl;
      }
    }
  else {
    // TODO: setup timer driven interrupt instead?
    }

  // SETUP SPI
  // Datasheet says SPI clock must have <1MHz frequency (BCM2835_SPI_CLOCK_DIVIDER_256)
  // but it may work up to 4MHz (BCM2835_SPI_CLOCK_DIVIDER_128, BCM2835_SPI_CLOCK_DIVIDER_64)
  /*
   * The Raspberry Pi GPIO pins reserved for SPI once bcm2835_spi_begin() is called are:
   * P1-19 (MOSI)
   * P1-21 (MISO)
   * P1-23 (CLK)
   * P1-24 (CE0)
   * P1-26 (CE1)
   */
  bcm2835_spi_begin();

  // set MSB here - setting to LSB elsewhere doesn't work. So I'm manually reversing lineAddress bit order instead.
  bcm2835_spi_setBitOrder (BCM2835_SPI_BIT_ORDER_MSBFIRST);
  bcm2835_spi_setClockDivider (BCM2835_SPI_CLOCK_DIVIDER_128); // this is the 2 MHz setting
  bcm2835_spi_setDataMode (BCM2835_SPI_MODE0);
  bcm2835_spi_chipSelect (BCM2835_SPI_CS_NONE);

  // Not sure if I can use the built-in bcm2835 Chip Select functions as the docs suggest it only
  // affects bcm2835_spi_transfer() calls so I'm setting it to inactive and setting up my own CS pin
  // as I want to use bcm2835_spi_writenb() to send data over SPI instead.
  // Set pin modes
  bcm2835_gpio_write (SCS, LOW);

  if(DISP != 255) {
    bcm2835_gpio_write (DISP, LOW);
    }
  if(enablePWM) {
    bcm2835_gpio_write (EXTCOMIN, LOW);
    }

  // Memory LCD startup sequence with recommended timings
  bcm2835_gpio_write (DISP, HIGH);
  bcm2835_delayMicroseconds (PWRUP_DISP_DELAY);

  bcm2835_gpio_write (EXTCOMIN, LOW);
  bcm2835_delayMicroseconds (PWRUP_EXTCOMIN_DELAY);

  clearLineBuffer();
  }
//}}}
//{{{
cSharpLcd::~cSharpLcd() {
  bcm2835_spi_end();
  bcm2835_close();
  }
//}}}

//{{{
void cSharpLcd::writeLineToDisplay (char lineNumber, char* line) {
  writeMultipleLinesToDisplay (lineNumber, 1, line);
  }
//}}}
//{{{
void cSharpLcd::writeMultipleLinesToDisplay (char lineNumber, char numLines, char* lines) {
  // this implementation writes multiple lines that are CONSECUTIVE (although they don't
  // have to be, as an address is given for every line, not just the first in the sequence)
  // data for all lines should be stored in a single array

  char * linePtr = lines;
  bcm2835_gpio_write (SCS, HIGH);
  bcm2835_delayMicroseconds (SCS_HIGH_DELAY);
  bcm2835_spi_writenb (&commandByte, 1);

  for(char x = 0; x < numLines; x++) {
    char reversedLineNumber = reverseByte (lineNumber);
    bcm2835_spi_writenb (&reversedLineNumber, 1);
    bcm2835_spi_writenb (linePtr++, LCDWIDTH/8); // Transfers a whole line of data at once
            // pointer arithmetic assumes an array of lines - TODO: change this?
    bcm2835_spi_writenb (&paddingByte, 1);
    lineNumber++;
    }

  bcm2835_spi_writenb (&paddingByte, 1);  // trailing paddings
  bcm2835_delayMicroseconds (SCS_LOW_DELAY);
  bcm2835_gpio_write (SCS, LOW);
  bcm2835_delayMicroseconds (INTERFRAME_DELAY);  // can I delete this delay?
  }
//}}}
//{{{
void cSharpLcd::writePixelToLineBuffer (unsigned int pixel, bool isWhite) {
// pixel location expected in the fn args follows the scheme defined in the datasheet.
// NB: the datasheet defines pixel addresses starting from 1, NOT 0

  if ((pixel <= LCDWIDTH) && (pixel != 0)) {
    pixel = pixel - 1;
    if (isWhite)
      lineBuffer[pixel/8] |=  (1 << (7 - pixel%8));
    else
      lineBuffer[pixel/8] &= ~(1 << (7 - pixel%8));
    }
  }
//}}}
//{{{
void cSharpLcd::writeByteToLineBuffer (char byteNumber, char byteToWrite) {
// char location expected in the fn args has been extrapolated from the pixel location
// format (see above), so chars go from 1 to LCDWIDTH/8, not from 0

  if (byteNumber <= LCDWIDTH/8 && byteNumber != 0) {
    byteNumber -= 1;
    lineBuffer[byteNumber] = byteToWrite;
    }
  }
//}}}
//{{{
void cSharpLcd::copyByteWithinLineBuffer (char sourceByte, char destinationByte) {

  if (sourceByte <= LCDWIDTH/8 && destinationByte <= LCDWIDTH/8) {
    sourceByte -= 1;
    destinationByte -= 1;
    lineBuffer[destinationByte] = lineBuffer[sourceByte];
    }
  }
//}}}

//{{{
void cSharpLcd::setLineBufferBlack() {

  for (char i = 0; i < LCDWIDTH/8; i++)
    lineBuffer[i] = 0x00;
  }
//}}}
//{{{
void cSharpLcd::setLineBufferWhite() {
  for (char i = 0; i < LCDWIDTH/8; i++)
    lineBuffer[i] = 0xFF;
  }
//}}}

//{{{
void cSharpLcd::writeLineBufferToDisplay (char lineNumber) {
  writeMultipleLinesToDisplay (lineNumber, 1, lineBuffer);
  }
//}}}
//{{{
void cSharpLcd::writeLineBufferToDisplayRepeatedly (char lineNumber, char numLines) {
  writeMultipleLinesToDisplay (lineNumber, numLines, lineBuffer);
  }
//}}}
//{{{
void cSharpLcd::writePixelToFrameBuffer (unsigned int pixel, char lineNumber, bool isWhite) {
// pixel location expected in the fn args follows the scheme defined in the datasheet.
// NB: the datasheet defines pixel addresses starting from 1, NOT 0

  i f((pixel <= LCDWIDTH) && (pixel != 0) && (lineNumber <=LCDHEIGHT) & (lineNumber != 0)) {
    pixel -= 1;
    lineNumber -= 1;
    if(isWhite)
      frameBuffer[(lineNumber*LCDWIDTH/8)+(pixel/8)] |=  (1 << (7 - pixel%8));
    else
      frameBuffer[(lineNumber*LCDWIDTH/8)+(pixel/8)] &= ~(1 << (7 - pixel%8));
    }
  }
//}}}
//{{{
void cSharpLcd::writeByteToFrameBuffer (char byteNumber, char lineNumber, char byteToWrite) {
// char location expected in the fn args has been extrapolated from the pixel location
// format (see above), so chars go from 1 to LCDWIDTH/8, not from 0

  if ((byteNumber <= LCDWIDTH/8) && (byteNumber != 0) && (lineNumber <=LCDHEIGHT) & (lineNumber != 0)) {
    byteNumber -= 1;
    lineNumber -= 1;
    frameBuffer[(lineNumber*LCDWIDTH/8)+byteNumber] = byteToWrite;
    }
  }
//}}}

//{{{
void cSharpLcd::setFrameBufferBlack() {

  for(char i=0; i<LCDWIDTH*LCDHEIGHT/8; i++) {
    frameBuffer[i] = 0x00;
    }
  }
//}}}
//{{{
void cSharpLcd::setFrameBufferWhite() {

  for  (char i = 0; i < LCDWIDTH*LCDHEIGHT/8; i++) {
    frameBuffer[i] = 0xFF;
    }
  }
//}}}

//{{{
void cSharpLcd::writeFrameBufferToDisplay() {
  writeMultipleLinesToDisplay (1, LCDHEIGHT, frameBuffer);
  }
//}}}

//{{{
void cSharpLcd::clearLineBuffer() {
  setLineBufferWhite();
  }
//}}}
//{{{
void cSharpLcd::clearFrameBuffer() {
  setFrameBufferWhite();
  }
//}}}
//{{{
void cSharpLcd::clearDisplay() {

  bcm2835_gpio_write (SCS, HIGH);
  bcm2835_delayMicroseconds (SCS_HIGH_DELAY);
  bcm2835_spi_writenb (&clearByte, 1);
  bcm2835_spi_writenb (&paddingByte, 1);
  bcm2835_delayMicroseconds (SCS_LOW_DELAY);
  bcm2835_gpio_write (SCS, LOW);
  bcm2835_delayMicroseconds (INTERFRAME_DELAY);
  }
//}}}

//{{{
void cSharpLcd::turnOff() {
// won't work if DISP pin is not used

  if (DISP != 255)
    bcm2835_gpio_write (DISP, HIGH);
  }
//}}}
//{{{
void cSharpLcd::turnOn() {
// won't work if DISP pin is not used

  if (DISP != 255)
    bcm2835_gpio_write(DISP, LOW);
  }
//}}}

//{{{
void cSharpLcd::softToggleVCOM() {

  vcomByte ^= 0b01000000;
  bcm2835_gpio_write (SCS, HIGH);
  bcm2835_delayMicroseconds (SCS_HIGH_DELAY);
  bcm2835_spi_writenb (&vcomByte, 1);
  bcm2835_spi_writenb (&paddingByte, 1);
  bcm2835_delayMicroseconds (SCS_LOW_DELAY);
  bcm2835_gpio_write (SCS, LOW);
  bcm2835_delayMicroseconds (10);
  }
//}}}

// private
//{{{
void* cSharpLcd::hardToggleVCOM (void* arg) {
//char extcomin = (char)arg;

  char extcomin = *((char*)(&arg));
  cout << "Value of extcomin in hardToggleVCOM is " << (unsigned int)extcomin << endl;
  // intended to be run as a separate thread. Do not execute in main loop!
  while(1) {
    bcm2835_delay (250);
    bcm2835_gpio_write (extcomin, HIGH);
    bcm2835_delay (250);
    bcm2835_gpio_write (extcomin, LOW);
    }

  pthread_exit(NULL);
  }
//}}}
//{{{
char cSharpLcd::reverseByte (char b) {
// reverses the bit order of an unsigned char
// needed to reverse the bit order of the Memory LCD lineAddress sent over SPI
// I can't get the bcm2835 bcm2835_spi_setBitOrder to work!
// (found this snippet on StackOverflow)

  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
  }
//}}}
