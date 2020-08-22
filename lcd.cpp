// lcd.cpp
#include "lcd.h"

#include <cstdint>
#include <string>
#include <thread>
#include <byteswap.h>

#include <pigpio.h>

#include "../shared/utils/utils.h"
#include "../shared/utils/cLog.h"
#include "fonts/FreeSansBold.h"

// !!! singleton assumed !|! saves exporting FREETYPE outside
#include <ft2build.h>
#include FT_FREETYPE_H
FT_Library mLibrary;
FT_Face mFace;

// J8 header pins
// - 3.3v                            J8 pin17
constexpr uint8_t kDcPin  = 24;   // J8 pin18
// - SPI0 - MOSI                     J8 pin19
// - 0v                              J8 pin20
// - SPI0 - SCLK                     J8 pin21
constexpr uint8_t kResetPin = 25; // J8 pin22
// - SPI0 - CE0                      J8 pin24

// cLcd - public
//{{{
cLcd::~cLcd() {
  spiClose (mHandle);
  gpioTerminate();
  }
//}}}
//{{{
void cLcd::setFont (const uint8_t* font, const int fontSize)  {

  FT_Init_FreeType (&mLibrary);
  FT_New_Memory_Face (mLibrary, (FT_Byte*)font, fontSize, 0, &mFace);
  }
//}}}

//{{{
void cLcd::rect (const uint16_t colour, const int xorg, const int yorg, const int xlen, const int ylen) {

  uint16_t bigEndianColour = bswap_16 (colour);

  for (int y = yorg; (y < yorg+ylen) && (y < getHeight()); y++)
    for (int x = xorg; (x < xorg+xlen) && (x < getWidth()); x++)
      mFrameBuf[(y*getWidth()) + x] = bigEndianColour;

  mChanged = true;
  }
//}}}
//{{{
void cLcd::pixel (const uint16_t colour, const int x, const int y) {

  mFrameBuf[(y*getWidth()) + x] = bswap_16 (colour);
  mChanged = true;
  }
//}}}
//{{{
void cLcd::blendPixel (const uint16_t colour, const uint8_t alpha, const int x, const int y) {
// magical rgb565 alpha composite
// - linear interp background * (1.0 - alpha) + foreground * alpha
//   - factorized into: result = background + (foreground - background) * alpha
//   - alpha is in Q1.5 format, so 0.0 is represented by 0, and 1.0 is represented by 32
// - Converts  0000000000000000rrrrrggggggbbbbb
// -     into  00000gggggg00000rrrrr000000bbbbb

  if ((alpha >= 0) && (x >= 0) && (y > 0) && (x < getWidth()) && (y < getHeight())) {
    // clip opaque and offscreen
    if (alpha == 0xFF)
      // simple case - set bigEndian frame buffer to littleEndian colour
      mFrameBuf[(y*getWidth()) + x] = bswap_16 (colour);
    else {
      // get bigEndian frame buffer into littleEndian background
      uint32_t background = bswap_16 (mFrameBuf[(y*getWidth()) + x]);

      // composite littleEndian colour
      uint32_t foreground = colour;
      foreground = (foreground | (foreground << 16)) & 0x07e0f81f;
      background = (background | (background << 16)) & 0x07e0f81f;
      background += (((foreground - background) * ((alpha + 4) >> 3)) >> 5) & 0x07e0f81f;

      // set bigEndian frame buffer to littleEndian result
      mFrameBuf[(y*getWidth()) + x] = bswap_16 (background | (background >> 16));
      }

    mChanged = true;
    }

  }
//}}}
//{{{
int cLcd::text (const uint16_t colour, const int strX, const int strY, const int height, const std::string& str) {

  FT_Set_Pixel_Sizes (mFace, 0, height);

  int curX = strX;
  for (unsigned i = 0; i < str.size(); i++) {
    FT_Load_Char (mFace, str[i], FT_LOAD_RENDER);
    FT_GlyphSlot slot = mFace->glyph;

    int x = curX + slot->bitmap_left;
    int y = strY + height - slot->bitmap_top;

    if (slot->bitmap.buffer) {
      for (unsigned bitmapY = 0; bitmapY < slot->bitmap.rows; bitmapY++) {
        auto bitmapPtr = slot->bitmap.buffer + (bitmapY * slot->bitmap.pitch);
        for (unsigned bitmapX = 0; bitmapX < slot->bitmap.width; bitmapX++)
          blendPixel (colour, *bitmapPtr++, x + bitmapX, y + bitmapY);
        }

      }
    curX += slot->advance.x / 64;
    }

  return curX;
  }
//}}}

//{{{
void cLcd::delayMs (const int ms) {
  gpioDelay (ms);
  }
//}}}

// cLcd - protected
//{{{
bool cLcd::initResources() {

  unsigned hardwareRevision = gpioHardwareRevision();
  unsigned version = gpioVersion();
  cLog::log (LOGINFO, "pigpio hwRev:%x version:%d", hardwareRevision, version);

  if (gpioInitialise() >= 0) {
    setFont (getFreeSansBold(), getFreeSansBoldSize());

    mFrameBuf = (uint16_t*)malloc (getWidth() * getHeight() * 2);
    return true;
    }

  return false;
  }
//}}}
//{{{
void cLcd::reset (const uint8_t pin) {
  // setup and pulse reset pin
  gpioSetMode (pin, PI_OUTPUT);

  gpioWrite (pin, 0);
  gpioDelay (10000);

  gpioWrite (pin, 1);
  gpioDelay (120000);
  }
//}}}
//{{{
void cLcd::initDcPin (const uint8_t pin) {
  // setup data/command pin to data (hi)
  gpioSetMode (pin, PI_OUTPUT);
  gpioWrite (pin, 1);
  }
//}}}
//{{{
void cLcd::initSpi (const int clockSpeed) {
  // setup spi0, use CE0 active lo as CS
  mHandle = spiOpen (0, clockSpeed, 0);
  }
//}}}

//{{{
void cLcd::command (const uint8_t command) {

  gpioWrite (mDcPin, 0);
  spiWrite (mHandle, (char*)(&command), 1);
  gpioWrite (mDcPin, 1);
  }
//}}}
//{{{
void cLcd::commandData (const uint8_t command, const uint16_t data) {

  gpioWrite (mDcPin, 0);
  spiWrite (mHandle, (char*)(&command), 1);
  gpioWrite (mDcPin, 1);

  char dataBytes[2] = { (char)(data >> 8), (char)(data & 0xff) };
  spiWrite (mHandle, (char*)dataBytes, 2);
  }
//}}}
//{{{
void cLcd::commandData (const uint8_t command, const uint8_t* data, const int len) {

  gpioWrite (mDcPin, 0);
  spiWrite (mHandle, (char*)(&command), 1);
  gpioWrite (mDcPin, 1);

  if (data) {
    if (len > 0x10000) {
      spiWrite (mHandle, (char*)data, len/2);
      spiWrite (mHandle, (char*)data + (len/2), len/2);
      }
    else
      spiWrite (mHandle, (char*)data, len);
    }
  }
//}}}

//{{{
void cLcd::launchUpdateThread (const uint8_t command) {

  std::thread ([=]() {
    // write frameBuffer to lcd ram thread if changed
    while (true) {
      if (mUpdate || (mAutoUpdate && mChanged)) {
        mChanged = false;
        mUpdate = false;
        commandData (command, (const uint8_t*)mFrameBuf, getWidth() * getHeight() * 2);
        }
      gpioDelay (16000);
      }
    } ).detach();
  }
//}}}

// cLcd7735 - public
constexpr uint8_t kWidth7735 = 128;
constexpr uint8_t kHeight7735 = 160;
cLcd7735::cLcd7735() : cLcd (kWidth7735, kHeight7735, kDcPin) {}

//{{{  command constexpr
constexpr uint8_t ST7735_SLPOUT  = 0x11; // no data
constexpr uint8_t ST7735_DISPOFF = 0x28; // no data
constexpr uint8_t ST7735_DISPON  = 0x29; // no data

constexpr uint8_t ST7735_CASET = 0x2A;
constexpr uint8_t caSetData[4] = { 0, 0, 0, kWidth7735 - 1 };

constexpr uint8_t ST7735_RASET = 0x2B;
constexpr uint8_t raSetData[4] = { 0, 0, 0, kHeight7735 - 1 };

constexpr uint8_t ST7735_RAMWR = 0x2C; // followed by frameBuffer data

constexpr uint8_t ST7735_MADCTL = 0x36;
constexpr uint8_t kMADCTData[1] = { 0xc0 };

constexpr uint8_t ST7735_COLMOD  = 0x3A;
constexpr uint8_t kCOLMODData[1] = { 0x05 };

constexpr uint8_t ST7735_FRMCTR1 = 0xB1;
constexpr uint8_t ST7735_FRMCTR2 = 0xB2;
constexpr uint8_t ST7735_FRMCTR3 = 0xB3;
constexpr uint8_t kFRMCTRData[6] = { 0x01, 0x2c, 0x2d, 0x01, 0x2c, 0x2d };

constexpr uint8_t ST7735_INVCTR  = 0xB4;
constexpr uint8_t kINVCTRData[1] = { 0x07 };

constexpr uint8_t ST7735_PWCTR1  = 0xC0;
constexpr uint8_t kPowerControlData1[3] = { 0xA2, 0x02 /* -4.6V */, 0x84 /* AUTO mode */ };
constexpr uint8_t ST7735_PWCTR2  = 0xC1;
constexpr uint8_t kPowerControlData2[1] = { 0xc5 }; // VGH25 = 2.4C VGSEL =-10 VGH = 3*AVDD
constexpr uint8_t ST7735_PWCTR3  = 0xC2;
constexpr uint8_t kPowerControlData3[2] = { 0x0A /* Opamp current small */, 0x00 /* Boost freq */ };
constexpr uint8_t ST7735_PWCTR4  = 0xC3;
constexpr uint8_t kPowerControlData4[2] = { 0x8A /* BCLK/2, Opamp current small/medium low */, 0x2A };
constexpr uint8_t ST7735_PWCTR5  = 0xC4;
constexpr uint8_t kPowerControlData5[2] = { 0x8A /* BCLK/2, Opamp current small/medium low */, 0xEE };

constexpr uint8_t ST7735_VMCTR1  = 0xC5;
constexpr uint8_t kVMCTR1Data[1] = { 0x0E };

constexpr uint8_t ST7735_GMCTRP1 = 0xE0;
constexpr uint8_t kGMCTRP1Data[16] = { 0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d,
                                       0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10 } ;

constexpr uint8_t ST7735_GMCTRN1 = 0xE1;
constexpr uint8_t kGMCTRN1Data[16] = { 0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
                                       0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10 } ;
//}}}
constexpr static const int kSpiClock7735 = 24000000;
//{{{
bool cLcd7735::initialise() {

  if (initResources()) {
    reset (kResetPin);
    initDcPin (kDcPin);
    initSpi (kSpiClock7735);

    command (ST7735_SLPOUT);
    delayMs (120);

    commandData (ST7735_FRMCTR1, kFRMCTRData, 3); // frameRate normal mode
    commandData (ST7735_FRMCTR2, kFRMCTRData, 3); // frameRate idle mode
    commandData (ST7735_FRMCTR3, kFRMCTRData, 6); // frameRate partial mode
    commandData (ST7735_INVCTR, kINVCTRData, sizeof(kINVCTRData)); // Inverted mode off

    commandData (ST7735_PWCTR1, kPowerControlData1, sizeof(kPowerControlData1)); // POWER CONTROL 1
    commandData (ST7735_PWCTR2, kPowerControlData2, sizeof(kPowerControlData2)); // POWER CONTROL 2
    commandData (ST7735_PWCTR3, kPowerControlData3, sizeof(kPowerControlData3)); // POWER CONTROL 3
    commandData (ST7735_PWCTR4, kPowerControlData4, sizeof(kPowerControlData4)); // POWER CONTROL 4
    commandData (ST7735_PWCTR5, kPowerControlData5, sizeof(kPowerControlData5)); // POWER CONTROL 5

    commandData (ST7735_VMCTR1, kVMCTR1Data, sizeof(kVMCTR1Data)); // POWER CONTROL 6
    commandData (ST7735_MADCTL, kMADCTData, sizeof(kMADCTData)); // ORIENTATION
    commandData (ST7735_COLMOD, kCOLMODData, sizeof(kCOLMODData)); // COLOR MODE - 16bit per pixel

    commandData (ST7735_GMCTRP1, kGMCTRP1Data, sizeof(kGMCTRP1Data)); // gamma GMCTRP1
    commandData (ST7735_GMCTRN1, kGMCTRN1Data, sizeof(kGMCTRN1Data)); // Gamma GMCTRN1

    commandData (ST7735_CASET, caSetData, sizeof(caSetData));
    commandData (ST7735_RASET, raSetData, sizeof(raSetData));

    command (ST7735_DISPON); // display ON

    launchUpdateThread (ST7735_RAMWR);
    return true;
    }

  return false;
  }
//}}}

// cLcd9225b - public
constexpr uint8_t kWidth9225b = 176;
constexpr int8_t kHeight9225b = 220;
cLcd9225b::cLcd9225b() : cLcd(kWidth9225b, kHeight9225b, kDcPin) {}

constexpr int kSpiClock9225b = 16000000;
//{{{
bool cLcd9225b::initialise() {

  if (initResources()) {
    reset (kResetPin);
    initDcPin (kDcPin);
    initSpi (kSpiClock9225b);

    commandData (0x01,0x011C); // set SS and NL bit

    commandData (0x02,0x0100); // set 1 line inversion
    commandData (0x03,0x1030); // set GRAM write direction and BGR=1
    commandData (0x08,0x0808); // set BP and FP
    commandData (0x0C,0x0000); // RGB interface setting R0Ch=0x0110 for RGB 18Bit and R0Ch=0111for RGB16
    commandData (0x0F,0x0b01); // Set frame rate//0b01

    commandData (0x20,0x0000); // Set GRAM Address
    commandData (0x21,0x0000); // Set GRAM Address
    delayMs (50000);

    //{{{  power On sequence
    commandData (0x10,0x0a00); // Set SAP,DSTB,STB//0800
    commandData (0x11,0x1038); // Set APON,PON,AON,VCI1EN,VC
    delayMs (50000);
    //}}}

    commandData (0x12,0x1121); // Internal reference voltage= Vci;
    commandData (0x13,0x0063); // Set GVDD
    commandData (0x14,0x4b44); // Set VCOMH/VCOML voltage//3944

    //{{{  set GRAM area
    commandData (0x30,0x0000);
    commandData (0x31,0x00DB);
    commandData (0x32,0x0000);
    commandData (0x33,0x0000);
    commandData (0x34,0x00DB);
    commandData (0x35,0x0000);
    commandData (0x36,0x00AF);
    commandData (0x37,0x0000);
    commandData (0x38,0x00DB);
    commandData (0x39,0x0000);
    //}}}
    //{{{  set Gamma Curve
    commandData (0x50,0x0003);
    commandData (0x51,0x0900);
    commandData (0x52,0x0d05);
    commandData (0x53,0x0900);
    commandData (0x54,0x0407);
    commandData (0x55,0x0502);
    commandData (0x56,0x0000);
    commandData (0x57,0x0005);
    commandData (0x58,0x1700);
    commandData (0x59,0x001F);
    delayMs (50000);
    //}}}

    commandData (0x07,0x1017);
    //{{{  set ram area
    commandData (0x36, getWidth()-1);
    commandData (0x37, 0);
    commandData (0x38, getHeight()-1);
    commandData (0x39, 0);
    commandData (0x20, 0);
    commandData (0x21, 0);
    //}}}

    launchUpdateThread (0x22);
    return true;
    }

  return false;
  }
//}}}
