// lcdTest.cpp
#include <cstdint>
#include <thread>
#include <pigpio.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "FreeSansBold.h"
#include "../shared/utils/utils.h"
#include "../shared/utils/cLog.h"

using namespace std;

//{{{  colours
static const uint16_t Black       =  0x0000;  //   0,   0,   0
static const uint16_t Blue        =  0x001F;  //   0,   0, 255
static const uint16_t Green       =  0x07E0;  //   0, 255,   0
static const uint16_t Cyan        =  0x07FF;  //   0, 255, 255
static const uint16_t Red         =  0xF800;  // 255,   0,   0
static const uint16_t Magenta     =  0xF81F;  // 255,   0, 255
static const uint16_t Yellow      =  0xFFE0;  // 255, 255,   0
static const uint16_t White       =  0xFFFF;  // 255, 255, 255

static const uint16_t Navy        =  0x000F;  //   0,   0, 128
static const uint16_t DarkGreen   =  0x03E0;  //   0, 128,   0
static const uint16_t DarkCyan    =  0x03EF;  //   0, 128, 128
static const uint16_t Maroon      =  0x7800;  // 128,   0,   0
static const uint16_t Purple      =  0x780F;  // 128,   0, 128
static const uint16_t Olive       =  0x7BE0;  // 128, 128,   0

static const uint16_t LightGrey   =  0xC618;  // 192, 192, 192
static const uint16_t DarkGrey    =  0x7BEF;  // 128, 128, 128

static const uint16_t Orange      =  0xFD20;  // 255, 165,   0
static const uint16_t GreenYellow =  0xAFE5;  // 173, 255,  47
//}}}
//{{{
class cLcd {
public:
  cLcd (const uint8_t width, const uint8_t height) : mWidth(width), mHeight(height) {}
  //{{{
  virtual ~cLcd() {
    spiClose (mHandle);
    gpioTerminate();
    }
  //}}}

  //{{{
  virtual bool initialise() {

    unsigned hardwareRevision = gpioHardwareRevision();
    unsigned version = gpioVersion();
    cLog::log (LOGINFO, "pigpio hwRev:%x version:%d", hardwareRevision, version);

    if (gpioInitialise() >= 0) {
      mFrameBuf = (uint16_t*)malloc (getWidth() * getHeight() * 2);
      return true;
      }

    return false;
    }
  //}}}
  const uint8_t getWidth() { return mWidth; }
  const uint8_t getHeight() { return mHeight; }

  //{{{
  void rect (uint16_t colour, int xorg, int yorg, int xlen, int ylen) {

    uint16_t colourReversed = (colour >> 8) | (colour << 8);

    for (int y = yorg; (y < yorg+ylen) && (y < getHeight()); y++)
      for (int x = xorg; (x < xorg+xlen) && (x < getWidth()); x++)
        mFrameBuf[(y*getWidth()) + x] = colourReversed;

    mChanged = true;
    }
  //}}}
  //{{{
  void pixel (uint16_t colour, int x, int y) {

    mFrameBuf[(y*getWidth()) + x] = (colour >> 8) | (colour << 8);
    mChanged = true;
    }
  //}}}
  //{{{
  void blendPixel (uint16_t colour, uint8_t alpha, int x, int y) {
  // magical rgb565 alpha composite
  // - linear interp bg * (1.0 - alpha) + fg * alpha
  //   - factorized into: result = bg + (fg - bg) * alpha
  //   - alpha is in Q1.5 format, so 0.0 is represented by 0, and 1.0 is represented by 32
  // - Converts  0000000000000000rrrrrggggggbbbbb
  // -     into  00000gggggg00000rrrrr000000bbbbb

    if (alpha >= 0) {
      if ((x >= 0) && (y > 0) && (x < getWidth()) && (y < getHeight())) {
        if (alpha == 0xFF) {
          mFrameBuf[(y*getWidth()) + x] = (colour >> 8) | (colour << 8);
          }
        else {
          uint16_t value = mFrameBuf[(y*getWidth()) + x];
          uint32_t bg = (value >> 8) | (value << 8);

          uint32_t fg = colour;
          fg = (fg | fg << 16) & 0x07e0f81f;
          bg = (bg | bg << 16) & 0x07e0f81f;
          bg += (((fg - bg) * ((alpha + 4) >> 3)) >> 5) & 0x07e0f81f;
          bg |= bg >> 16;

          mFrameBuf[(y*getWidth()) + x] = (bg >> 8) | (bg << 8);
          }
        mChanged = true;
        }
      }
    }
  //}}}
  //{{{
  int text (uint16_t colour, int strX, int strY, FT_Face& face, int height, string str) {

    FT_Set_Pixel_Sizes (face, 0, height);

    for (unsigned i = 0; i < str.size(); i++) {
      FT_Load_Char (face, str[i], FT_LOAD_RENDER);
      FT_GlyphSlot slot = face->glyph;

      int x = strX + slot->bitmap_left;
      int y = strY + height - slot->bitmap_top;

      if (slot->bitmap.buffer) {
        for (unsigned bitmapY = 0; bitmapY < slot->bitmap.rows; bitmapY++) {
          auto bitmapPtr = slot->bitmap.buffer + (bitmapY * slot->bitmap.pitch);
          for (unsigned bitmapX = 0; bitmapX < slot->bitmap.width; bitmapX++)
            blendPixel (colour, *bitmapPtr++, x + bitmapX, y + bitmapY);
          }

        }
      strX += slot->advance.x / 64;
      }

    return strX;
    }
  //}}}
  //{{{
  void clear (uint16_t colour) {
    rect (colour, 0,0, getWidth(), getHeight());
    }
  //}}}

  //{{{
  void update() {
    mUpdate = true;
    }
  //}}}
  //{{{
  void setAutoUpdate() {
    mAutoUpdate = true;
    }
  //}}}

  int mHandle = 0;
  bool mChanged = false;
  bool mUpdate = false;
  bool mAutoUpdate = false;

protected:
  //{{{
  void reset (const uint8_t pin) {
    // setup and pulse reset pin
    gpioSetMode (pin, PI_OUTPUT);

    gpioWrite (pin, 0);
    gpioDelay (10000);

    gpioWrite (pin, 1);
    gpioDelay (120000);
    }
  //}}}
  //{{{
  void initDcPin (const uint8_t pin) {
    // setup data/command pin to data (hi)
    gpioSetMode (pin, PI_OUTPUT);
    gpioWrite (pin, 1);
    }
  //}}}
  //{{{
  void initSpi (const int clockSpeed) {
    // setup spi0, use CE0 active lo as CS
    mHandle = spiOpen (0, clockSpeed, 0);
   }
  //}}}

  uint16_t* mFrameBuf = nullptr;

private:
  const uint8_t mWidth;
  const uint8_t mHeight;
  };
//}}}
//{{{
class cLcd7735 : public cLcd {
public:
  cLcd7735() : cLcd (kWidth, kHeight) {}
  virtual ~cLcd7735() {}

  //{{{
  virtual bool initialise() {

    if (cLcd::initialise()) {
      reset (kResetPin);
      initDcPin (kDcPin);
      initSpi (kSpiClock);

      commandData (ST7735_SLPOUT);
      gpioDelay (120);

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

      commandData (ST7735_DISPON); // display ON

      thread ([=]() {
        // write frameBuffer to lcd ram thread if changed
        while (true) {
          if (mUpdate || (mAutoUpdate && mChanged)) {
            mChanged = false;
            mUpdate = false;
            commandData (ST7735_RAMWR, (const uint8_t*)mFrameBuf, getWidth() * getHeight() * 2);
            }
          gpioDelay (16000);
          }
        } ).detach();
      return true;
      }

    return false;
    }
  //}}}

private:
  constexpr static uint8_t kWidth = 128;
  constexpr static uint8_t kHeight = 160;
  constexpr static const int kSpiClock = 24000000;

  // J8 header pins
  // - 3.3v                                         J8 pin17
  constexpr static const uint8_t kDcPin  = 24;   // J8 pin18
  // - SPI0 - MOSI                                  J8 pin19
  // - 0v                                           J8 pin20
  // - SPI0 - SCLK                                  J8 pin21
  constexpr static const uint8_t kResetPin = 25; // J8 pin22
  // - SPI0 - CE0                                   J8 pin24

  //{{{  command constexpr
  constexpr static uint8_t ST7735_SLPOUT  = 0x11; // no data
  constexpr static uint8_t ST7735_DISPOFF = 0x28; // no data
  constexpr static uint8_t ST7735_DISPON  = 0x29; // no data

  constexpr static uint8_t ST7735_CASET = 0x2A;
  constexpr static uint8_t caSetData[4] = { 0, 0, 0, kWidth - 1 };

  constexpr static uint8_t ST7735_RASET = 0x2B;
  constexpr static uint8_t raSetData[4] = { 0, 0, 0, kHeight - 1 };

  constexpr static uint8_t ST7735_RAMWR = 0x2C; // followed by frameBuffer data

  constexpr static uint8_t ST7735_MADCTL = 0x36;
  constexpr static uint8_t kMADCTData[1] = { 0xc0 };

  constexpr static uint8_t ST7735_COLMOD  = 0x3A;
  constexpr static uint8_t kCOLMODData[1] = { 0x05 };

  constexpr static uint8_t ST7735_FRMCTR1 = 0xB1;
  constexpr static uint8_t ST7735_FRMCTR2 = 0xB2;
  constexpr static uint8_t ST7735_FRMCTR3 = 0xB3;
  constexpr static uint8_t kFRMCTRData[6] = { 0x01, 0x2c, 0x2d, 0x01, 0x2c, 0x2d };

  constexpr static uint8_t ST7735_INVCTR  = 0xB4;
  constexpr static uint8_t kINVCTRData[1] = { 0x07 };

  constexpr static uint8_t ST7735_PWCTR1  = 0xC0;
  constexpr static uint8_t kPowerControlData1[3] = { 0xA2, 0x02 /* -4.6V */, 0x84 /* AUTO mode */ };
  constexpr static uint8_t ST7735_PWCTR2  = 0xC1;
  constexpr static uint8_t kPowerControlData2[1] = { 0xc5 }; // VGH25 = 2.4C VGSEL =-10 VGH = 3*AVDD
  constexpr static uint8_t ST7735_PWCTR3  = 0xC2;
  constexpr static uint8_t kPowerControlData3[2] = { 0x0A /* Opamp current small */, 0x00 /* Boost freq */ };
  constexpr static uint8_t ST7735_PWCTR4  = 0xC3;
  constexpr static uint8_t kPowerControlData4[2] = { 0x8A /* BCLK/2, Opamp current small/medium low */, 0x2A };
  constexpr static uint8_t ST7735_PWCTR5  = 0xC4;
  constexpr static uint8_t kPowerControlData5[2] = { 0x8A /* BCLK/2, Opamp current small/medium low */, 0xEE };

  constexpr static uint8_t ST7735_VMCTR1  = 0xC5;
  constexpr static uint8_t kVMCTR1Data[1] = { 0x0E };

  constexpr static uint8_t ST7735_GMCTRP1 = 0xE0;
  constexpr static uint8_t kGMCTRP1Data[16] = { 0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d,
                                                0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10 } ;

  constexpr static uint8_t ST7735_GMCTRN1 = 0xE1;
  constexpr static uint8_t kGMCTRN1Data[16] = { 0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
                                                0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10 } ;
  //}}}
  //{{{
  void commandData (uint8_t command, const uint8_t* data = nullptr, int len = 0) {

    gpioWrite (kDcPin, 0);
    spiWrite (mHandle, (char*)(&command), 1);
    gpioWrite (kDcPin, 1);

    if (data)
      spiWrite (mHandle, (char*)data, len);
    }
  //}}}
  };
//}}}
//{{{
class cLcd9225b : public cLcd {
public:
  cLcd9225b() : cLcd(kWidth, kHeight) {}
  virtual ~cLcd9225b() {}

  //{{{
  virtual bool initialise() {

    if (cLcd::initialise()) {
      reset (kResetPin);
      initDcPin (kDcPin);
      initSpi (kSpiClock);

      commandData (0x01,0x011C); // set SS and NL bit

      commandData (0x02,0x0100); // set 1 line inversion
      commandData (0x03,0x1030); // set GRAM write direction and BGR=1
      commandData (0x08,0x0808); // set BP and FP
      commandData (0x0C,0x0000); // RGB interface setting R0Ch=0x0110 for RGB 18Bit and R0Ch=0111for RGB16
      commandData (0x0F,0x0b01); // Set frame rate//0b01

      commandData (0x20,0x0000); // Set GRAM Address
      commandData (0x21,0x0000); // Set GRAM Address
      gpioDelay (50000);

      //{{{  power On sequence
      commandData (0x10,0x0a00); // Set SAP,DSTB,STB//0800
      commandData (0x11,0x1038); // Set APON,PON,AON,VCI1EN,VC
      gpioDelay (50000);
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
      gpioDelay (50000);
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

      thread ([=]() {
        // thread lambda - write frameBuffer to lcd ram if updated
        char command = 0x22;
        while (true) {
          if (mUpdate || (mAutoUpdate && mChanged)) {
            mUpdate = false;
            mChanged = false;

            gpioWrite (kDcPin, 0);
            spiWrite (mHandle, &command, 1);
            gpioWrite (kDcPin, 1);

            spiWrite (mHandle, (char*)mFrameBuf, getWidth() * getHeight());
            spiWrite (mHandle, (char*)(mFrameBuf) + (getWidth() * getHeight()), getWidth() * getHeight());
            }
          gpioDelay (16000);
          }
        } ).detach();
      return true;
      }

    return false;
    }
  //}}}

private:
  constexpr static uint8_t kWidth = 176;
  constexpr static uint8_t kHeight = 220;
  constexpr static int kSpiClock = 16000000;

  // J8 header pins
  // - 3.3v                                         J8 pin17
  constexpr static const uint8_t kDcPin  = 24;   // J8 pin18
  // - SPI0 - MOSI                                  J8 pin19
  // - 0v                                           J8 pin20
  // - SPI0 - SCLK                                  J8 pin21
  constexpr static const uint8_t kResetPin = 25; // J8 pin22
  // - SPI0 - CE0                                   J8 pin24

  //{{{
  void commandData (uint8_t command, const uint16_t data) {

    gpioWrite (kDcPin, 0);
    spiWrite (mHandle, (char*)(&command), 1);
    gpioWrite (kDcPin, 1);

    char dataBytes[2] = { (char)(data >> 8), (char)(data & 0xff) };
    spiWrite (mHandle, (char*)dataBytes, 2);
    }
  //}}}
  };
//}}}

int main() {

  cLog::init (LOGINFO, false, "", "gpio");

  cLcd* lcd = new cLcd9225b();
  lcd->initialise();

  FT_Library library;
  FT_Init_FreeType (&library);

  FT_Face face;
  FT_New_Memory_Face (library, (FT_Byte*)getFreeSansBold(), getFreeSansBoldSize(), 0, &face);

  while (true) {
    for (int i = 0; i < 1000; i += 3) {
      lcd->clear (Black);
      lcd->text (White, 0,0, face, 100, dec(i,3));
      lcd->update();
      gpioDelay (40000);
      }

    int height = 8;
    while (height++ < lcd->getHeight()) {
      int x = 0;
      int y = 0;
      lcd->clear (Magenta);
      for (char ch = 'A'; ch < 0x7f; ch++) {
        x = lcd->text (White, x, y, face, height, string(1,ch));
        if (x > lcd->getWidth()) {
          x = 0;
          y += height;
          if (y > lcd->getHeight())
            break;
          }
        }
      lcd->text (Yellow, 0,0, face, 20, "Hello Colin");
      lcd->update();
      gpioDelay (40000);
      }
    }

  delete lcd;
  return 0;
  }
