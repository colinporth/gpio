// st7735.cpp
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "pigpio/pigpio.h"

#define TFTWIDTH  128
#define TFTHEIGHT 160
static const int kSpiClock = 1000000;

using namespace std;

//{{{  colour defines
#define Black        0x0000  /*   0,   0,   0 */
#define Blue         0x001F  /*   0,   0, 255 */
#define Green        0x07E0  /*   0, 255,   0 */
#define Cyan         0x07FF  /*   0, 255, 255 */
#define Red          0xF800  /* 255,   0,   0 */
#define Magenta      0xF81F  /* 255,   0, 255 */
#define Yellow       0xFFE0  /* 255, 255,   0 */
#define White        0xFFFF  /* 255, 255, 255 */

#define Navy         0x000F  /*   0,   0, 128 */
#define DarkGreen    0x03E0  /*   0, 128,   0 */
#define DarkCyan     0x03EF  /*   0, 128, 128 */
#define Maroon       0x7800  /* 128,   0,   0 */
#define Purple       0x780F  /* 128,   0, 128 */
#define Olive        0x7BE0  /* 128, 128,   0 */
#define LightGrey    0xC618  /* 192, 192, 192 */
#define DarkGrey     0x7BEF  /* 128, 128, 128 */
#define Orange       0xFD20  /* 255, 165,   0 */
#define GreenYellow  0xAFE5  /* 173, 255,  47 */
//}}}
//{{{  commands
#define ST7735_NOP 0x0
#define ST7735_SWRESET 0x01
//#define ST7735_RDDID 0x04
//#define ST7735_RDDST 0x09
//#define ST7735_RDDPM 0x0A
//#define ST7735_RDDMADCTL 0x0B
//#define ST7735_RDDCOLMOD 0x0C
//#define ST7735_RDDIM 0x0D
//#define ST7735_RDDSM 0x0E

#define ST7735_SLPIN   0x10
#define ST7735_SLPOUT  0x11
#define ST7735_PTLON   0x12
#define ST7735_NORON   0x13

#define ST7735_INVOFF  0x20
#define ST7735_INVON   0x21
#define ST7735_GAMSET  0x26

#define ST7735_DISPOFF 0x28
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_RGBSET  0x2D
//#define ST7735_RAMRD 0x2E

#define ST7735_PTLAR   0x30
#define ST7735_TEOFF   0x34
#define ST7735_TEON    0x35
#define ST7735_MADCTL  0x36
#define ST7735_IDMOFF  0x38
#define ST7735_IDMON   0x39
#define ST7735_COLMOD  0x3A

#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR  0xB4
#define ST7735_DISSET5 0xB6

#define ST7735_PWCTR1  0xC0
#define ST7735_PWCTR2  0xC1
#define ST7735_PWCTR3  0xC2
#define ST7735_PWCTR4  0xC3
#define ST7735_PWCTR5  0xC4
#define ST7735_VMCTR1  0xC5
#define ST7735_VMOFCTR 0xC7

#define ST7735_WRID2   0xD1
#define ST7735_WRID3   0xD2
#define ST7735_NVCTR1  0xD9
#define ST7735_NVCTR2  0xDE
#define ST7735_NVCTR3  0xDF

#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1
//}}}
static const uint8_t CS_PIN  = 23; // J8 - pin16 - used
static const uint8_t RES_PIN = 24; // J8 - pin18 - used
static const uint8_t DC_PIN  = 25; // J8 - pin22 - used

int mHandle = 0;

//{{{
void writeCommand (char command) {

  gpioWrite (CS_PIN, 0);
  gpioWrite (DC_PIN, 0);

  spiWrite (mHandle, &command, 1);

  gpioWrite (CS_PIN, 1);
  gpioWrite (DC_PIN, 1);
  }
//}}}
//{{{
void writeData (char data) {

  gpioWrite (CS_PIN, 0);

  spiWrite (mHandle, &data, 1);

  gpioWrite (CS_PIN, 1);
  }
//}}}
//{{{
void writeCommandData (char command, char data) {

  writeCommand (command);
  writeData (data);
}
//}}}

//{{{
void writeColour (uint16_t colour, int length) {

  gpioWrite (CS_PIN, 0);

  for (int i = 0; i < length; i++) {
    char hi = colour >> 8;
    char lo = colour & 0xFF;
    spiWrite (mHandle, &hi, 1);
    spiWrite (mHandle, &lo, 1);
    }

  gpioWrite (CS_PIN, 1);
  }
//}}}
//{{{
bool writeWindow (int16_t xorg, int16_t yorg, uint16_t xlen, uint16_t ylen) {

  uint16_t xend = xorg + xlen - 1;
  uint16_t yend = yorg + ylen - 1;

  if ((xend < TFTWIDTH) && (yend < TFTHEIGHT)) {
    writeCommand (ST7735_CASET);  // column addr set
    writeData (0);
    writeData (xorg);   // XSTART
    writeData (0);
    writeData (xend);   // XEND

    writeCommand (ST7735_RASET);  // row addr set
    writeData (0);
    writeData (yorg);    // YSTART
    writeData (0);
    writeData (yend);    // YEND

    writeCommand (ST7735_RAMWR);

    return true;
    }

  return false;
  }
//}}}

//{{{
void drawRect (uint16_t colour, int16_t xorg, int16_t yorg, uint16_t xlen, uint16_t ylen) {

  if (writeWindow (xorg, yorg, xlen, ylen))
    writeColour (colour, xlen*ylen);
  }
//}}}

//{{{
void init() {

  unsigned hardwareRevision = gpioHardwareRevision();
  unsigned version = gpioVersion();
  printf ("pigpio hwRev:%x version:%d\n", hardwareRevision, version);

  if (gpioInitialise() >= 0) {
    gpioSetMode (CS_PIN, PI_OUTPUT);
    gpioSetMode (RES_PIN, PI_OUTPUT);
    gpioSetMode (DC_PIN, PI_OUTPUT);

    // set CE0 active hi
    mHandle = spiOpen (0, kSpiClock, 0x00004);

    gpioWrite (CS_PIN, 1);
    gpioWrite (DC_PIN, 1);

    gpioWrite (RES_PIN, 0);
    gpioDelay (10000);
    gpioWrite (RES_PIN, 1);
    gpioDelay (120000);

    writeCommand (ST7735_SLPOUT);
    gpioDelay (120);

    writeCommand (ST7735_FRMCTR1); // Frame rate in normal mode
    writeData (0x01);
    writeData (0x2C);
    writeData (0x2D);

    writeCommand (ST7735_FRMCTR2); // Frame rate in idle mode
    writeData (0x01);
    writeData (0x2C);
    writeData (0x2D);

    writeCommand (ST7735_FRMCTR3); // Frame rate in partial mode
    writeData (0x01);
    writeData (0x2C);
    writeData (0x2D);
    writeData (0x01);   // inversion mode settings
    writeData (0x2C);
    writeData (0x2D);

    writeCommandData (ST7735_INVCTR, 0x07); // Inverted mode off

    writeCommand (ST7735_PWCTR1); // POWER CONTROL 1
    writeData (0xA2);
    writeData (0x02);             // -4.6V
    writeData (0x84);             // AUTO mode

    writeCommandData (ST7735_PWCTR2, 0xC5); // POWER CONTROL 2 - VGH25 = 2.4C VGSEL =-10 VGH = 3*AVDD

    writeCommand (ST7735_PWCTR3); // POWER CONTROL 3
    writeData (0x0A);             // Opamp current small
    writeData (0x00);             // Boost freq

    writeCommand (ST7735_PWCTR4); // POWER CONTROL 4
    writeData (0x8A);             // BCLK/2, Opamp current small / medium low
    writeData (0x2A);

    writeCommand (ST7735_PWCTR5); // POWER CONTROL 5
    writeData (0x8A);             // BCLK/2, Opamp current small / medium low
    writeData (0xEE);

    writeCommandData (ST7735_VMCTR1, 0x0E); // POWER CONTROL 6
    writeCommandData (ST7735_MADCTL, 0xC0); // ORIENTATION
    writeCommandData (ST7735_COLMOD, 0x05); // COLOR MODE - 16bit per pixel

    //{{{  gamma GMCTRP1
    writeCommand (ST7735_GMCTRP1);
    writeData (0x02);
    writeData (0x1c);
    writeData (0x07);
    writeData (0x12);
    writeData (0x37);
    writeData (0x32);
    writeData (0x29);
    writeData (0x2d);
    writeData (0x29);
    writeData (0x25);
    writeData (0x2B);
    writeData (0x39);
    writeData (0x00);
    writeData (0x01);
    writeData (0x03);
    writeData (0x10);
    //}}}
    //{{{  Gamma GMCTRN1
    writeCommand (ST7735_GMCTRN1);
    writeData (0x03);
    writeData (0x1d);
    writeData (0x07);
    writeData (0x06);
    writeData (0x2E);
    writeData (0x2C);
    writeData (0x29);
    writeData (0x2D);
    writeData (0x2E);
    writeData (0x2E);
    writeData (0x37);
    writeData (0x3F);
    writeData (0x00);
    writeData (0x00);
    writeData (0x02);
    writeData (0x10);
    //}}}

    writeCommand (ST7735_DISPON); // display ON
    }
  }
//}}}

int main() {

  init();
  drawRect (Red, 0, 0, TFTWIDTH, TFTHEIGHT);

  return 0;
  }
