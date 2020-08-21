// st7735.cpp
#include <cstdint>
#include <thread>
#include "../shared/utils/cLog.h"
//#include "pigpio/pigpio.h"
#include <pigpio.h>
#include <ft2build.h>

#include FT_FREETYPE_H
#include "FreeSansBold.c"

static FT_Library library;
static FT_Face face;
static FT_GlyphSlot slot;

using namespace std;

//{{{  static const uint16_t colours
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
          if (mChanged) {
            mChanged = false;
            command (ST7735_RAMWR, (const uint8_t*)mFrameBuf, kWidth * kHeight * 2);
            }
          gpioDelay (10000);
          }
        } ).detach();
      }
    }
  //}}}
  uint8_t getWidth() { return kWidth; }
  uint8_t getHeight() { return kHeight; }

  //{{{
  void clear (uint16_t colour) {
    rect (colour, 0,0, kWidth, kHeight);
    }
  //}}}
  //{{{
  void rect (uint16_t colour, int16_t xorg, int16_t yorg, uint16_t xlen, uint16_t ylen) {

    uint16_t colourReversed = (colour >> 8) | (colour << 8);

    for (int y = yorg; (y < yorg+ylen) && (y < kHeight); y++)
      for (int x = xorg; (x < xorg+xlen) && (x < kWidth); x++)
        mFrameBuf[(y*kWidth) + x] = colourReversed;

    mChanged = true;
    }
  //}}}
  //{{{
  void pixel (uint16_t colour, int16_t x, int16_t y) {

    mFrameBuf[(y*kWidth) + x] = (colour >> 8) | (colour << 8);
    mChanged = true;
    }
  //}}}
  //{{{
  void blendPixel (uint16_t colour, uint8_t alpha, int16_t x, int16_t y) {
  // magical rgb565 alpha composite

    if (alpha == 0) {
      }
    else if (alpha == 0xFF)
      mFrameBuf[(y*kWidth) + x] = (colour >> 8) | (colour << 8);
    else {
      // Converts  0000000000000000rrrrrggggggbbbbb
      //     into  00000gggggg00000rrrrr000000bbbbb
      uint32_t fg = colour;
      fg = (fg | fg << 16) & 0x07e0f81f;

      uint16_t value = mFrameBuf[(y*kWidth) + x];
      uint32_t bg = (value >> 8) | (value << 8);
      bg = (bg | bg << 16) & 0x07e0f81f;

      // This implements the linear interpolation formula: result = bg * (1.0 - alpha) + fg * alpha
      // This can be factorized into: result = bg + (fg - bg) * alpha
      // alpha is in Q1.5 format, so 0.0 is represented by 0, and 1.0 is represented by 32
      uint32_t alpha1 = (alpha + 4) >> 3;
      bg += (fg - bg) * alpha1 >> 5;
      bg &= 0x07e0f81f;
      bg = bg | (bg >> 16);

      mFrameBuf[(y*kWidth) + x] = (bg >> 8) | (bg << 8);
      }

    mChanged = true;
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
  uint16_t mFrameBuf [kWidth * kHeight * 2];
  };
//}}}

//{{{
int textChar (cLcd& lcd, uint16_t colour, char ch, int height, int dx, int dy) {

  FT_Set_Pixel_Sizes (face, 0, height);
  FT_Load_Char (face, ch, FT_LOAD_RENDER);

  dx += slot->bitmap_left;
  dy += height - slot->bitmap_top;

  if (slot->bitmap.buffer) {
    for (unsigned y = 0; y < slot->bitmap.rows; y++)
      for (unsigned x = 0; x < slot->bitmap.width; x++)
        lcd.blendPixel (colour, slot->bitmap.buffer[(y * slot->bitmap.pitch) + x], dx+x, dy+y);
    return slot->advance.x / 64;
    }

  return 0;
  }
//}}}

int main() {

  cLog::init (LOGINFO, false, "", "gpio");

  cLcd lcd;
  lcd.initialise();

  FT_Init_FreeType (&library);
  FT_New_Memory_Face (library, (FT_Byte*)freeSansBold, freeSansBold_len, 0, &face);
  slot = face->glyph;

  while (true) {
    int height = 5;
    while (height++ < 130) {
      int x = 0;
      int y = 0;
      lcd.clear (DarkCyan);
      for (char ch = 'A'; ch < 0x7f; ch++) {
        x += textChar (lcd, White, ch, height, x, y);
        if (x > lcd.getWidth()) {
          x = 0;
          y += height;
          if (y > 160)
            break;
          }
        }
      gpioDelay (40000);
      }
    }

  lcd.clear (Yellow);
  gpioDelay (200000);

  lcd.clear (Green);
  gpioDelay (200000);

  for (int i = 0; i < lcd.getWidth(); i++)
    lcd.rect (Blue, 0, i, i, 1);
  gpioDelay (200000);

  for (int i = 0; i < lcd.getWidth(); i++)
    lcd.rect (Cyan, 0, i, i, 1);
  gpioDelay (200000);

  for (int i = 0; i < lcd.getWidth(); i++)
    lcd.rect (White, 0, i, i, 1);
  gpioDelay (200000);

  return 0;
  }
