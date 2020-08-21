// st7735.cpp
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
// ST7735 tft lcd
public:
  cLcd() {}
  //{{{
  ~cLcd() {
    //turnOff();
    spiClose (mHandle);
    gpioTerminate();
    }
  //}}}

  //{{{
  void initialise() {

    unsigned hardwareRevision = gpioHardwareRevision();
    unsigned version = gpioVersion();
    cLog::log (LOGINFO, "pigpio hwRev:%x version:%d", hardwareRevision, version);

    if (gpioInitialise() >= 0) {
      // setup data/command pin to data (hi)
      gpioSetMode (kDcPin, PI_OUTPUT);
      gpioWrite (kDcPin, 1);

      //{{{  setup and pulse reset pin
      gpioSetMode (kResetPin, PI_OUTPUT);

      gpioWrite (kResetPin, 0);
      gpioDelay (10000);

      gpioWrite (kResetPin, 1);
      gpioDelay (120000);
      //}}}

      // setup spi0, use CE0 active lo as CS
      mHandle = spiOpen (0, kSpiClock, 0);

      command (ST7735_SLPOUT);
      gpioDelay (120);

      command (ST7735_FRMCTR1, kFRMCTRData, 3); // frameRate normal mode
      command (ST7735_FRMCTR2, kFRMCTRData, 3); // frameRate idle mode
      command (ST7735_FRMCTR3, kFRMCTRData, 6); // frameRate partial mode
      command (ST7735_INVCTR, kINVCTRData, sizeof(kINVCTRData)); // Inverted mode off

      command (ST7735_PWCTR1, kPowerControlData1, sizeof(kPowerControlData1)); // POWER CONTROL 1
      command (ST7735_PWCTR2, kPowerControlData2, sizeof(kPowerControlData2)); // POWER CONTROL 2
      command (ST7735_PWCTR3, kPowerControlData3, sizeof(kPowerControlData3)); // POWER CONTROL 3
      command (ST7735_PWCTR4, kPowerControlData4, sizeof(kPowerControlData4)); // POWER CONTROL 4
      command (ST7735_PWCTR5, kPowerControlData5, sizeof(kPowerControlData5)); // POWER CONTROL 5

      command (ST7735_VMCTR1, kVMCTR1Data, sizeof(kVMCTR1Data)); // POWER CONTROL 6
      command (ST7735_MADCTL, kMADCTData, sizeof(kMADCTData)); // ORIENTATION
      command (ST7735_COLMOD, kCOLMODData, sizeof(kCOLMODData)); // COLOR MODE - 16bit per pixel

      command (ST7735_GMCTRP1, kGMCTRP1Data, sizeof(kGMCTRP1Data)); // gamma GMCTRP1
      command (ST7735_GMCTRN1, kGMCTRN1Data, sizeof(kGMCTRN1Data)); // Gamma GMCTRN1

      command (ST7735_CASET, caSetData, sizeof(caSetData));
      command (ST7735_RASET, raSetData, sizeof(raSetData));

      command (ST7735_DISPON); // display ON

      thread ([=]() {
        // write frameBuffer to lcd ram thread if changed
        while (true) {
          if (mUpdate || (mAutoUpdate && mChanged)) {
            mChanged = false;
            mUpdate = false;
            command (ST7735_RAMWR, (const uint8_t*)mFrameBuf, kWidth * kHeight * 2);
            }
          gpioDelay (16000);
          }
        } ).detach();
      }
    }
  //}}}
  uint8_t getWidth() { return kWidth; }
  uint8_t getHeight() { return kHeight; }

  //{{{
  void rect (uint16_t colour, int xorg, int yorg, int xlen, int ylen) {

    uint16_t colourReversed = (colour >> 8) | (colour << 8);

    for (int y = yorg; (y < yorg+ylen) && (y < kHeight); y++)
      for (int x = xorg; (x < xorg+xlen) && (x < kWidth); x++)
        mFrameBuf[(y*kWidth) + x] = colourReversed;

    mChanged = true;
    }
  //}}}
  //{{{
  void clear (uint16_t colour) {
    rect (colour, 0,0, kWidth, kHeight);
    }
  //}}}

  //{{{
  void pixel (uint16_t colour, int x, int y) {

    mFrameBuf[(y*kWidth) + x] = (colour >> 8) | (colour << 8);
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
      if ((x >= 0) && (y > 0) && (x < kWidth) && (y < kHeight)) {
        if (alpha == 0xFF) {
          mFrameBuf[(y*kWidth) + x] = (colour >> 8) | (colour << 8);
          }
        else {
          uint16_t value = mFrameBuf[(y*kWidth) + x];
          uint32_t bg = (value >> 8) | (value << 8);

          uint32_t fg = colour;
          fg = (fg | fg << 16) & 0x07e0f81f;
          bg = (bg | bg << 16) & 0x07e0f81f;
          bg += (((fg - bg) * ((alpha + 4) >> 3)) >> 5) & 0x07e0f81f;
          bg |= bg >> 16;

          mFrameBuf[(y*kWidth) + x] = (bg >> 8) | (bg << 8);
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
  void update() {
    mUpdate = true;
    }
  //}}}
  //{{{
  void setAutoUpdate() {
    mAutoUpdate = true;
    }
  //}}}

private:
  static const uint8_t kWidth = 128;
  static const uint8_t kHeight = 160;

  // J8 header pins
  // - 3.3v                               J8 pin17
  static const uint8_t kDcPin  = 24;   // J8 pin18
  // - SPI0 - MOSI                        J8 pin19
  // - 0v                                 J8 pin20
  // - SPI0 - SCLK                        J8 pin21
  static const uint8_t kResetPin = 25; // J8 pin22
  // - SPI0 - CE0                         J8 pin24
  static const int kSpiClock = 24000000;

  //{{{  command const
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
  void command (uint8_t command, const uint8_t* data = nullptr, int len = 0) {

    gpioWrite (kDcPin, 0);
    spiWrite (mHandle, (char*)(&command), 1);
    gpioWrite (kDcPin, 1);

    if (data)
      spiWrite (mHandle, (char*)data, len);
    }
  //}}}

  int mHandle = 0;
  bool mChanged = true;
  bool mUpdate = false;
  bool mAutoUpdate = false;
  uint16_t mFrameBuf [kWidth * kHeight * 2]; // RGB565
  };
//}}}

int main() {

  cLog::init (LOGINFO, false, "", "gpio");

  cLcd lcd;
  lcd.initialise();

  FT_Library library;
  FT_Init_FreeType (&library);

  FT_Face face;
  FT_New_Memory_Face (library, (FT_Byte*)getFreeSansBold(), getFreeSansBoldSize(), 0, &face);

  while (true) {
    for (int i = 0; i < 100; i++) {
      lcd.clear (Black);
      lcd.text (White, 0,0, face, 110, dec(i,2));
      lcd.update();
      gpioDelay (100000);
      }

    int height = 8;
    while (height++ < lcd.getHeight()) {
      int x = 0;
      int y = 0;
      lcd.clear (Magenta);
      for (char ch = 'A'; ch < 0x7f; ch++) {
        x = lcd.text (White, x, y, face, height, string(1,ch));
        if (x > lcd.getWidth()) {
          x = 0;
          y += height;
          if (y > lcd.getHeight())
            break;
          }
        }
      lcd.text (Yellow, 0,0, face, 20, "Hello Colin");
      lcd.update();
      gpioDelay (40000);
      }
    }

  return 0;
  }
