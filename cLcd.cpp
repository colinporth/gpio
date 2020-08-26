// cLcd.cpp
//{{{  includes
#include "cLcd.h"

#include <cstdint>
#include <string>
#include <thread>
#include <byteswap.h>

#include <pigpio.h>

#include "../shared/utils/utils.h"
#include "../shared/utils/cLog.h"
#include "fonts/FreeSansBold.h"

using namespace std;
//}}}
//{{{  freetype library static include
// !!! singleton assumed !|! saves exporting FREETYPE outside
#include <ft2build.h>
#include FT_FREETYPE_H

static FT_Library mLibrary;
static FT_Face mFace;
//}}}

// J8 header pins
// - 3.3v                                     J8 pin17
constexpr uint8_t kDataCommandGpio  = 24;  // J8 pin18
//constexpr uint8_t kBackLightGpio  = 24;  // J8 pin18
// - SPI0 - MOSI                              J8 pin19
// - 0v                                       J8 pin20
// - SPI0 - SCLK                              J8 pin21
constexpr uint8_t kResetGpio = 25;         // J8 pin22
// - SPI0 - CE0                               J8 pin24
constexpr uint8_t kSpiCe0Gpio = 8;         // J8 pin24

// cLcd - public
//{{{
cLcd::~cLcd() {
  gpioTerminate();
  }
//}}}
//{{{
void cLcd::setFont (const uint8_t* font, const int fontSize)  {

  FT_Init_FreeType (&mLibrary);
  FT_New_Memory_Face (mLibrary, (FT_Byte*)font, fontSize, 0, &mFace);
  }
//}}}

// bigEndian frameBuf
//{{{
void cLcd::rect (const uint16_t colour, const int xorg, const int yorg, const int xlen, const int ylen) {

  uint16_t bigEndianColour = bswap_16 (colour);

  int xmax = min (xorg+xlen, (int)getWidth());
  int ymax = min (yorg+ylen, (int)getHeight());

  for (int y = yorg; y < ymax; y++) {
    uint16_t* ptr = mFrameBuf + y*getWidth() + xorg;
    for (int x = xorg; x < xmax; x++)
      *ptr++ = bigEndianColour;
      }

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
      // simple case - set bigEndianColour frameBuf pixel to littleEndian colour
      mFrameBuf[(y*getWidth()) + x] = bswap_16 (colour);
    else {
      // get bigEndianColour frame buffer into littleEndian background
      uint32_t background = bswap_16 (mFrameBuf[(y*getWidth()) + x]);

      // composite littleEndian colour
      uint32_t foreground = colour;
      foreground = (foreground | (foreground << 16)) & 0x07e0f81f;
      background = (background | (background << 16)) & 0x07e0f81f;
      background += (((foreground - background) * ((alpha + 4) >> 3)) >> 5) & 0x07e0f81f;

      // set bigEndianColour frameBuf pixel to littleEndian background result
      mFrameBuf[(y*getWidth()) + x] = bswap_16 (background | (background >> 16));
      }

    mChanged = true;
    }
  }
//}}}
//{{{
int cLcd::text (const uint16_t colour, const int strX, const int strY, const int height, const string& str) {

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
void cLcd::delayUs (const int us) {
  gpioDelay (us);
  }
//}}}

// cLcd - protected
//{{{
bool cLcd::initResources() {

  unsigned version = gpioVersion();
  unsigned hardwareRevision = gpioHardwareRevision();

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
void cLcd::initResetPin() {

  if (mResetGpio != 0xFF) {
    gpioSetMode (mResetGpio, PI_OUTPUT);

    gpioWrite (mResetGpio, 0);
    gpioDelay (10000);

    gpioWrite (mResetGpio, 1);
    gpioDelay (120000);
    }
  }
//}}}
//{{{
void cLcd::initChipEnablePin() {

  if (mChipEnableGpio != 0xFF) {
    gpioSetMode (mChipEnableGpio, PI_OUTPUT);
    gpioWrite (mChipEnableGpio, 1);
    }
  }
//}}}
//{{{
void cLcd::initDataCommandPin() {

  if (mDataCommandGpio != 0xFF) {
    gpioSetMode (mDataCommandGpio, PI_OUTPUT);
    gpioWrite (mDataCommandGpio, 1);
    }
  }
//}}}
//{{{
void cLcd::launchUpdateThread (const uint8_t command) {

  thread ([=]() {
    // write frameBuffer to lcd ram thread if changed
    while (true) {
      if (mUpdate || (mAutoUpdate && mChanged)) {
        mChanged = false;
        mUpdate = false;
        writeCommandMultipleData (command, (const uint8_t*)mFrameBuf, getWidth() * getHeight() * 2);
        }
      gpioDelay (16000);
      }
    } ).detach();
  }
//}}}

// cLcdSpi - uses bigEndian frameBuf
//{{{
cLcdSpi::~cLcdSpi() {
  spiClose (mSpiHandle);
  }
//}}}
//{{{
void cLcdSpi::initSpi() {
// if mChipEnableGpio then disable autoSpiCe0
// - can't send multiple SPI's without pulsing CE screwing up dataSequence

  mSpiHandle = spiOpen (0, mSpiClock, (mSpiMode0 ? 0 : 3) | ((mChipEnableGpio == 0xFF) ? 0x00 : 0x40));
  }
//}}}
//{{{
void cLcdSpi::writeCommand (const uint8_t command) {

  if (mUseSequence) {
    uint8_t commandSequence[3] = { 0x70, 0, command };
    gpioWrite (mChipEnableGpio, 0);
    spiWrite (mSpiHandle, (char*)commandSequence, 3);
    gpioWrite (mChipEnableGpio, 1);
    }
  else {
    gpioWrite (mDataCommandGpio, 0);
    spiWrite (mSpiHandle, (char*)(&command), 1);
    gpioWrite (mDataCommandGpio, 1);
    }

  }
//}}}
//{{{
void cLcdSpi::writeCommandData (const uint8_t command, const uint16_t data) {

  writeCommand (command);

  if (mUseSequence) {
    uint8_t dataSequence[3] = { 0x72, uint8_t(data >> 8), uint8_t(data & 0xff) };
    gpioWrite (mChipEnableGpio, 0);
    spiWrite (mSpiHandle, (char*)dataSequence, 3);
    gpioWrite (mChipEnableGpio, 1);
    }
  else {
    uint8_t dataBytes[2] = { uint8_t(data >> 8), uint8_t(data & 0xff) };
    spiWrite (mSpiHandle, (char*)dataBytes, 2);
    }
  }
//}}}
//{{{
void cLcdSpi::writeCommandMultipleData (const uint8_t command, const uint8_t* dataPtr, const int len) {

  writeCommand (command);

  if (mUseSequence) {
    // we manage the ce, send data start
    uint8_t dataSequenceStart = 0x72;
    gpioWrite (mChipEnableGpio, 0);
    spiWrite (mSpiHandle, (char*)(&dataSequenceStart), 1);
    }

  // send data
  int bytesLeft = len;
  auto ptr = (char*)dataPtr;
  while (bytesLeft > 0) {
   int sendBytes = (bytesLeft > 0xFFFF) ? 0xFFFF : bytesLeft;
    spiWrite (mSpiHandle, ptr, sendBytes);
    ptr += sendBytes;
    bytesLeft -= sendBytes;
    }

  if (mUseSequence)
    gpioWrite (mChipEnableGpio, 1);
  }
//}}}

// cLcdParallel16 - override with littleEndian frameBuf
//{{{
void cLcdParallel16::rect (const uint16_t colour, const int xorg, const int yorg, const int xlen, const int ylen) {

  int xmax = min (xorg+xlen, (int)getWidth());
  int ymax = min (yorg+ylen, (int)getHeight());

  for (int y = yorg; y < ymax; y++) {
    uint16_t* ptr = mFrameBuf + y*getWidth() + xorg;
    for (int x = xorg; x < xmax; x++)
      *ptr++ = colour;
      }

  mChanged = true;
  }
//}}}
//{{{
void cLcdParallel16::pixel (const uint16_t colour, const int x, const int y) {

  mFrameBuf[(y*getWidth()) + x] = colour;
  mChanged = true;
  }
//}}}
//{{{
void cLcdParallel16::blendPixel (const uint16_t colour, const uint8_t alpha, const int x, const int y) {
// magical rgb565 alpha composite
// - linear interp background * (1.0 - alpha) + foreground * alpha
//   - factorized into: result = background + (foreground - background) * alpha
//   - alpha is in Q1.5 format, so 0.0 is represented by 0, and 1.0 is represented by 32
// - Converts  0000000000000000rrrrrggggggbbbbb
// -     into  00000gggggg00000rrrrr000000bbbbb

  if ((alpha >= 0) && (x >= 0) && (y > 0) && (x < getWidth()) && (y < getHeight())) {
    // clip opaque and offscreen
    if (alpha == 0xFF)
      // simple case - set bigEndianColour frameBuf pixel to littleEndian colour
      mFrameBuf[(y*getWidth()) + x] = colour;
    else {
      // get bigEndianColour frame buffer into littleEndian background
      uint32_t background = mFrameBuf[(y*getWidth()) + x];

      // composite littleEndian colour
      uint32_t foreground = colour;
      foreground = (foreground | (foreground << 16)) & 0x07e0f81f;
      background = (background | (background << 16)) & 0x07e0f81f;
      background += (((foreground - background) * ((alpha + 4) >> 3)) >> 5) & 0x07e0f81f;

      // set bigEndianColour frameBuf pixel to littleEndian background result
      mFrameBuf[(y*getWidth()) + x] = background | (background >> 16);
      }

    mChanged = true;
    }
  }
//}}}
//{{{
void cLcdParallel16::writeCommand (const uint8_t command) {

  gpioWrite (mDataCommandGpio, 0);

  gpioWrite_Bits_0_31_Set (command);                 // set hi data bits
  gpioWrite_Bits_0_31_Clear (~command & mWrClrMask); // clear lo data bits + kWrGpio bit lo
  gpioWrite (mWrGpio, 1);                            // set kWrGpio hi to complete write

  gpioWrite (mDataCommandGpio, 1);
  }
//}}}
//{{{
void cLcdParallel16::writeCommandData (const uint8_t command, const uint16_t data) {

  writeCommand (command);

  gpioWrite_Bits_0_31_Set (data);                 // set hi data bits
  gpioWrite_Bits_0_31_Clear (~data & mWrClrMask); // clear lo data bits + kWrGpio bit lo
  gpioWrite (mWrGpio, 1);                         // set kWrGpio hi to complete write
  }
//}}}
//{{{
void cLcdParallel16::writeCommandMultipleData (const uint8_t command, const uint8_t* dataPtr, const int len) {

  writeCommand (command);

  // send data
  uint16_t* ptr = (uint16_t*)dataPtr;
  uint16_t* ptrEnd = (uint16_t*)dataPtr + len/2;

  while (ptr++ < ptrEnd) {
    gpioWrite_Bits_0_31_Set (*ptr);                 // set hi data bits
    gpioWrite_Bits_0_31_Clear (~*ptr & mWrClrMask); // clear lo data bits + kWrGpio bit lo
    gpioWrite (mWrGpio, 1);                         // set kWrGpio hi to complete write
    }
  }
//}}}

// specific device classes
//{{{  cLcd7735
constexpr uint16_t kWidth7735 = 128;
constexpr uint16_t kHeight7735 = 160;
constexpr static const int kSpiClock7735 = 24000000;

cLcd7735::cLcd7735() : cLcdSpi (kWidth7735, kHeight7735, kSpiClock7735, true,
                                kResetGpio, kDataCommandGpio, 0xFF) {}

bool cLcd7735::initialise() {
  if (initResources()) {
    initResetPin();
    initChipEnablePin();
    initDataCommandPin();
    initSpi();

    //{{{  command constexpr
    constexpr uint8_t k7335_SLPOUT  = 0x11; // no data
    constexpr uint8_t k7335_DISPON  = 0x29; // no data
    //constexpr uint8_t k7335_DISPOFF = 0x28; // no data

    constexpr uint8_t k7335_CASET = 0x2A;
    constexpr uint8_t k7335_caSetData[4] = { 0, 0, 0, kWidth7735 - 1 };

    constexpr uint8_t k7335_RASET = 0x2B;
    constexpr uint8_t k7335_raSetData[4] = { 0, 0, 0, kHeight7735 - 1 };

    constexpr uint8_t k7335_RAMWR = 0x2C; // followed by frameBuffer data

    constexpr uint8_t k7335_MADCTL = 0x36;
    constexpr uint8_t k7735_MADCTLData[1] = { 0xc0 };

    constexpr uint8_t k7335_COLMOD  = 0x3A;
    constexpr uint8_t k7335_COLMODData[1] = { 0x05 };

    constexpr uint8_t k7335_FRMCTR1 = 0xB1;
    constexpr uint8_t k7335_FRMCTR2 = 0xB2;
    constexpr uint8_t k7335_FRMCTR3 = 0xB3;
    constexpr uint8_t k7335_FRMCTRData[6] = { 0x01, 0x2c, 0x2d, 0x01, 0x2c, 0x2d };

    constexpr uint8_t k7335_INVCTR  = 0xB4;
    constexpr uint8_t k7335_INVCTRData[1] = { 0x07 };

    constexpr uint8_t k7335_PWCTR1  = 0xC0;
    constexpr uint8_t k7335_PowerControlData1[3] = { 0xA2, 0x02 /* -4.6V */, 0x84 /* AUTO mode */ };
    constexpr uint8_t k7335_PWCTR2  = 0xC1;
    constexpr uint8_t k7335_PowerControlData2[1] = { 0xc5 }; // VGH25 = 2.4C VGSEL =-10 VGH = 3*AVDD
    constexpr uint8_t k7335_PWCTR3  = 0xC2;
    constexpr uint8_t k7335_PowerControlData3[2] = { 0x0A /* Opamp current small */, 0x00 /* Boost freq */ };
    constexpr uint8_t k7335_PWCTR4  = 0xC3;
    constexpr uint8_t k7335_PowerControlData4[2] = { 0x8A /* BCLK/2, Opamp current small/medium low */, 0x2A };
    constexpr uint8_t k7335_PWCTR5  = 0xC4;
    constexpr uint8_t k7335_PowerControlData5[2] = { 0x8A /* BCLK/2, Opamp current small/medium low */, 0xEE };

    constexpr uint8_t k7335_VMCTR1  = 0xC5;
    constexpr uint8_t k7335_VMCTR1Data[1] = { 0x0E };

    constexpr uint8_t k7335_GMCTRP1 = 0xE0;
    constexpr uint8_t k7335_GMCTRP1Data[16] = { 0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d,
                                                0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10 } ;

    constexpr uint8_t k7335_GMCTRN1 = 0xE1;
    constexpr uint8_t k7335_GMCTRN1Data[16] = { 0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
                                                0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10 } ;
    //}}}
    writeCommand (k7335_SLPOUT);
    delayUs (120000);

    writeCommandMultipleData (k7335_FRMCTR1, k7335_FRMCTRData, 3); // frameRate normal mode
    writeCommandMultipleData (k7335_FRMCTR2, k7335_FRMCTRData, 3); // frameRate idle mode
    writeCommandMultipleData (k7335_FRMCTR3, k7335_FRMCTRData, 6); // frameRate partial mode
    writeCommandMultipleData (k7335_INVCTR, k7335_INVCTRData, sizeof(k7335_INVCTRData)); // Inverted mode off

    writeCommandMultipleData (k7335_PWCTR1, k7335_PowerControlData1, sizeof(k7335_PowerControlData1)); // POWER CONTROL 1
    writeCommandMultipleData (k7335_PWCTR2, k7335_PowerControlData2, sizeof(k7335_PowerControlData2)); // POWER CONTROL 2
    writeCommandMultipleData (k7335_PWCTR3, k7335_PowerControlData3, sizeof(k7335_PowerControlData3)); // POWER CONTROL 3
    writeCommandMultipleData (k7335_PWCTR4, k7335_PowerControlData4, sizeof(k7335_PowerControlData4)); // POWER CONTROL 4
    writeCommandMultipleData (k7335_PWCTR5, k7335_PowerControlData5, sizeof(k7335_PowerControlData5)); // POWER CONTROL 5

    writeCommandMultipleData (k7335_VMCTR1, k7335_VMCTR1Data, sizeof(k7335_VMCTR1Data)); // POWER CONTROL 6
    writeCommandMultipleData (k7335_MADCTL, k7735_MADCTLData, sizeof(k7735_MADCTLData)); // ORIENTATION
    writeCommandMultipleData (k7335_COLMOD, k7335_COLMODData, sizeof(k7335_COLMODData)); // COLOR MODE - 16bit per pixel

    writeCommandMultipleData (k7335_GMCTRP1, k7335_GMCTRP1Data, sizeof(k7335_GMCTRP1Data)); // gamma GMCTRP1
    writeCommandMultipleData (k7335_GMCTRN1, k7335_GMCTRN1Data, sizeof(k7335_GMCTRN1Data)); // Gamma GMCTRN1

    writeCommandMultipleData (k7335_CASET, k7335_caSetData, sizeof(k7335_caSetData));
    writeCommandMultipleData (k7335_RASET, k7335_raSetData, sizeof(k7335_raSetData));

    writeCommand (k7335_DISPON); // display ON

    launchUpdateThread (k7335_RAMWR);
    return true;
    }

  return false;
  }
//}}}
//{{{  cLcd9320
constexpr uint16_t kWidth9320 = 240;
constexpr uint16_t kHeight9320 = 320;
constexpr int kSpiClock9320 = 24000000;

cLcd9320::cLcd9320() : cLcdSpi(kWidth9320, kHeight9320, kSpiClock9320, false,
                               kResetGpio, 0xFF, kSpiCe0Gpio) {}

bool cLcd9320::initialise() {
  if (initResources()) {
    initResetPin();
    initChipEnablePin();
    initDataCommandPin();
    initSpi();

    writeCommandData (0xE5, 0x8000); // Set the Vcore voltage
    writeCommandData (0x00, 0x0000); // start oscillation - stopped?
    writeCommandData (0x01, 0x0100); // Driver Output Control 1 - SS=1 and SM=0
    writeCommandData (0x02, 0x0700); // LCD Driving Control - set line inversion
    writeCommandData (0x03, 0x1030); // Entry Mode - BGR, HV inc, vert write,
    writeCommandData (0x04, 0x0000); // Resize Control
    writeCommandData (0x08, 0x0202); // Display Control 2
    writeCommandData (0x09, 0x0000); // Display Control 3
    writeCommandData (0x0a, 0x0000); // Display Control 4 - frame marker
    writeCommandData (0x0c, 0x0001); // RGB Display Interface Control 1
    writeCommandData (0x0d, 0x0000); // Frame Marker Position
    writeCommandData (0x0f, 0x0000); // RGB Display Interface Control 2
    delayUs (40000);

    writeCommandData (0x07, 0x0101); // Display Control 1
    delayUs (40000);

    writeCommandData (0x10, 0x10C0); // Power Control 1
    writeCommandData (0x11, 0x0007); // Power Control 2
    writeCommandData (0x12, 0x0110); // Power Control 3
    writeCommandData (0x13, 0x0b00); // Power Control 4
    writeCommandData (0x29, 0x0000); // Power Control 7
    writeCommandData (0x2b, 0x4010); // Frame Rate and Color Control

    // 0x30 - 0x3d gamma
    writeCommandData (0x60, 0x2700); // Driver Output Control 2
    writeCommandData (0x61, 0x0001); // Base Image Display Control
    writeCommandData (0x6a, 0x0000); // Vertical Scroll Control

    writeCommandData (0x80, 0x0000); // Partial Image 1 Display Position
    writeCommandData (0x81, 0x0000); // Partial Image 1 Area Start Line
    writeCommandData (0x82, 0x0000); // Partial Image 1 Area End Line
    writeCommandData (0x83, 0x0000); // Partial Image 2 Display Position
    writeCommandData (0x84, 0x0000); // Partial Image 2 Area Start Line
    writeCommandData (0x85, 0x0000); // Partial Image 2 Area End Line

    writeCommandData (0x90, 0x0010); // Panel Interface Control 1
    writeCommandData (0x92, 0x0000); // Panel Interface Control 2
    writeCommandData (0x93, 0x0001); // Panel Interface Control 3
    writeCommandData (0x95, 0x0110); // Panel Interface Control 4
    writeCommandData (0x97, 0x0000); // Panel Interface Control 5
    writeCommandData (0x98, 0x0000); // Panel Interface Control 6

    writeCommandData (0x07, 0x0133); // Display Control 1
    delayUs (40000);

    writeCommandData (0x50, 0); // Horizontal Start Position
    writeCommandData (0x51, getWidth()-1); // Horizontal End Position

    writeCommandData (0x52, 0); // Vertical Start Position
    writeCommandData (0x53, getHeight()-1); // Vertical End Position

    writeCommandData (0x20, 0); // Horizontal GRAM Address Set
    writeCommandData (0x21, 0); // Vertical GRAM Address Set

    launchUpdateThread (0x22);
    return true;
    }

  return false;
  }
//}}}
//{{{  cLcd9225b
constexpr uint16_t kWidth9225b = 176;
constexpr uint16_t kHeight9225b = 220;
constexpr int kSpiClock9225b = 16000000;

cLcd9225b::cLcd9225b() : cLcdSpi(kWidth9225b, kHeight9225b, kSpiClock9225b, true,
                                 kResetGpio, kDataCommandGpio, 0xFF) {}

bool cLcd9225b::initialise() {
  if (initResources()) {
    initResetPin();
    initChipEnablePin();
    initDataCommandPin();
    initSpi();

    writeCommandData (0x01, 0x011C); // set SS and NL bit

    writeCommandData (0x02, 0x0100); // set 1 line inversion
    writeCommandData (0x03, 0x1030); // set GRAM write direction and BGR=1
    writeCommandData (0x08, 0x0808); // set BP and FP
    writeCommandData (0x0C, 0x0000); // RGB interface setting R0Ch=0x0110 for RGB 18Bit and R0Ch=0111for RGB16
    writeCommandData (0x0F, 0x0b01); // Set frame rate//0b01

    writeCommandData (0x20, 0x0000); // Set GRAM Address
    writeCommandData (0x21, 0x0000); // Set GRAM Address
    delayUs (50000);

    //{{{  power On sequence
    writeCommandData (0x10,0x0a00); // Set SAP,DSTB,STB//0800
    writeCommandData (0x11,0x1038); // Set APON,PON,AON,VCI1EN,VC
    delayUs (50000);
    //}}}
    writeCommandData (0x12, 0x1121); // Internal reference voltage= Vci;
    writeCommandData (0x13, 0x0063); // Set GVDD
    writeCommandData (0x14, 0x4b44); // Set VCOMH/VCOML voltage//3944

    //{{{  set GRAM area
    writeCommandData (0x30,0x0000);
    writeCommandData (0x31,0x00DB);
    writeCommandData (0x32,0x0000);
    writeCommandData (0x33,0x0000);
    writeCommandData (0x34,0x00DB);
    writeCommandData (0x35,0x0000);
    writeCommandData (0x36,0x00AF);
    writeCommandData (0x37,0x0000);
    writeCommandData (0x38,0x00DB);
    writeCommandData (0x39,0x0000);
    //}}}
    //{{{  set Gamma Curve
    writeCommandData (0x50,0x0003);
    writeCommandData (0x51,0x0900);
    writeCommandData (0x52,0x0d05);
    writeCommandData (0x53,0x0900);
    writeCommandData (0x54,0x0407);
    writeCommandData (0x55,0x0502);
    writeCommandData (0x56,0x0000);
    writeCommandData (0x57,0x0005);
    writeCommandData (0x58,0x1700);
    writeCommandData (0x59,0x001F);
    delayUs (50000);
    //}}}

    writeCommandData (0x07, 0x1017);
    //{{{  set ram area
    writeCommandData (0x36, getWidth()-1);
    writeCommandData (0x37, 0);
    writeCommandData (0x38, getHeight()-1);
    writeCommandData (0x39, 0);
    writeCommandData (0x20, 0);
    writeCommandData (0x21, 0);
    //}}}

    launchUpdateThread (0x22);
    return true;
    }
  return false;
  }
//}}}
//{{{  cLcd1289
constexpr uint8_t kWrGpio = 16;
constexpr uint8_t kRsGpio = 17;
constexpr uint8_t kRdGpio = 18;

constexpr uint16_t kWidth1289 = 240;
constexpr uint16_t kHeight1289 = 320;
cLcd1289::cLcd1289() : cLcdParallel16(kWidth1289, kHeight1289, kResetGpio, kRsGpio, 0xFF, kWrGpio, kRdGpio) {}

bool cLcd1289::initialise() {
  if (initResources()) {
    initResetPin();
    initDataCommandPin();

    // wr
    if (mWrGpio != 0xFF) {
      gpioSetMode (mWrGpio, PI_OUTPUT);
      gpioWrite (mWrGpio, 1);
      }

    // rd unused
    if (mRdGpio != 0xFF) {
      gpioSetMode (mRdGpio, PI_OUTPUT);
      gpioWrite (mRdGpio, 1);
      }

    // parallel d0-15
    for (int i = 0; i < 16; i++) {
      gpioSetMode (i, PI_OUTPUT);
      gpioWrite (i, 0);
      }

    // startup commands
    writeCommandData (0x00, 0x0001); // SSD1289_REG_OSCILLATION
    writeCommandData (0x03, 0xA8A4); // SSD1289_REG_POWER_CTRL_1
    writeCommandData (0x0c, 0x0000); // SSD1289_REG_POWER_CTRL_2
    writeCommandData (0x0d, 0x080C); // SSD1289_REG_POWER_CTRL_3
    writeCommandData (0x0e, 0x2B00); // SSD1289_REG_POWER_CTRL_4
    writeCommandData (0x1e, 0x00B7); // SSD1289_REG_POWER_CTRL_5

    //write_reg(0x01, (1 << 13) | (par->bgr << 11) | (1 << 9) | (HEIGHT - 1));
    writeCommandData (0x01, 0x2B3F); // SSD1289_REG_DRIVER_OUT_CTRL
    writeCommandData (0x02, 0x0600); // SSD1289_REG_LCD_DRIVE_AC
    writeCommandData (0x10, 0x0000); // SSD1289_REG_SLEEP_MODE
    writeCommandData (0x11, 0x6070); // SSD1289_REG_ENTRY_MODE

    writeCommandData (0x07, 0x0233); // SSD1289_REG_DISPLAY_CTRL
    writeCommandData (0x0b, 0x0000); // SSD1289_REG_FRAME_CYCLE
    writeCommandData (0x0f, 0x0000); // SSD1289_REG_GATE_SCAN_START

    writeCommandData (0x23, 0x0000); // SSD1289_REG_WR_DATA_MASK_1
    writeCommandData (0x24, 0x0000); // SSD1289_REG_WR_DATA_MASK_2
    writeCommandData (0x25, 0x8000); // SSD1289_REG_FRAME_FREQUENCY

    writeCommandData (0x30, 0x0707); // SSD1289_REG_GAMMA_CTRL_1
    writeCommandData (0x31, 0x0204); // SSD1289_REG_GAMMA_CTRL_2
    writeCommandData (0x32, 0x0204); // SSD1289_REG_GAMMA_CTRL_3
    writeCommandData (0x33, 0x0502); // SSD1289_REG_GAMMA_CTRL_4
    writeCommandData (0x34, 0x0507); // SSD1289_REG_GAMMA_CTRL_5
    writeCommandData (0x35, 0x0204); // SSD1289_REG_GAMMA_CTRL_6
    writeCommandData (0x36, 0x0204); // SSD1289_REG_GAMMA_CTRL_7
    writeCommandData (0x37, 0x0502); // SSD1289_REG_GAMMA_CTRL_8
    writeCommandData (0x3a, 0x0302); // SSD1289_REG_GAMMA_CTRL_9
    writeCommandData (0x3b, 0x0302); // SSD1289_REG_GAMMA_CTRL_10

    writeCommandData (0x41, 0x0000); // SSD1289_REG_V_SCROLL_CTRL_1
    //write_reg(0x42, 0x0000);
    writeCommandData (0x48, 0x0000); // SSD1289_REG_FIRST_WIN_START
    writeCommandData (0x49, 0x013F); // SSD1289_REG_FIRST_WIN_END

    writeCommandData (0x44, ((getWidth()-1) << 8) | 0); // SSD1289_REG_H_RAM_ADR_POS
    writeCommandData (0x45, 0x0000); // SSD1289_REG_V_RAM_ADR_START
    writeCommandData (0x46, getHeight()-1);  // SSD1289_REG_V_RAM_ADR_END
    writeCommandData (0x4e, 0x0000); // SSD1289_REG_GDDRAM_X_ADDR
    writeCommandData (0x4f, 0x0000); // SSD1289_REG_GDDRAM_Y_ADDR

    //{{{  rotates
    //switch (par->info->var.rotate) {
      ///* R4Eh - Set GDDRAM X address counter */
      ///* R4Fh - Set GDDRAM Y address counter */
      //case 0:
        //write_reg(par, 0x4e, xs);
        //write_reg(par, 0x4f, ys);
        //write_reg(par, 0x11, reg11 | 0b110000);
        //break;
      //case 180:
        //write_reg(par, 0x4e, par->info->var.xres - 1 - xs);
        //write_reg(par, 0x4f, par->info->var.yres - 1 - ys);
        //write_reg(par, 0x11, reg11 | 0b000000);
        //break;
      //case 270:
        //write_reg(par, 0x4e, par->info->var.yres - 1 - ys);
        //write_reg(par, 0x4f, xs);
        //write_reg(par, 0x11, reg11 | 0b101000);
        //break;
      //case 90:
        //write_reg(par, 0x4e, ys);
        //write_reg(par, 0x4f, par->info->var.xres - 1 - xs);
        //write_reg(par, 0x11, reg11 | 0b011000);
        //break;
      //}
    //}}}
    launchUpdateThread (0x22); // SSD1289_REG_GDDRAM_DATA
    return true;
    }

  return false;
  }
//}}}
