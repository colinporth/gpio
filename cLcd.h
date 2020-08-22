// cLcd uint16 rgb565 display classes
#pragma once
#include <cstdint>
#include <string>

// colours - uint16 RGB565
constexpr uint16_t kBlue        =  0x001F;  //   0,   0, 255
constexpr uint16_t kNavy        =  0x000F;  //   0,   0, 128
constexpr uint16_t kGreen       =  0x07E0;  //   0, 255,   0
constexpr uint16_t kDarkGreen   =  0x03E0;  //   0, 128,   0
constexpr uint16_t kCyan        =  0x07FF;  //   0, 255, 255
constexpr uint16_t kDarkCyan    =  0x03EF;  //   0, 128, 128
constexpr uint16_t kRed         =  0xF800;  // 255,   0,   0
constexpr uint16_t kMaroon      =  0x7800;  // 128,   0,   0
constexpr uint16_t kMagenta     =  0xF81F;  // 255,   0, 255
constexpr uint16_t kPurple      =  0x780F;  // 128,   0, 128
constexpr uint16_t kOrange      =  0xFD20;  // 255, 165,   0
constexpr uint16_t kYellow      =  0xFFE0;  // 255, 255,   0
constexpr uint16_t kOlive       =  0x7BE0;  // 128, 128,   0
constexpr uint16_t kGreenYellow =  0xAFE5;  // 173, 255,  47

constexpr uint16_t kBlack       =  0x0000;  //   0,   0,   0
constexpr uint16_t kLightGrey   =  0xC618;  // 192, 192, 192
constexpr uint16_t kDarkGrey    =  0x7BEF;  // 128, 128, 128
constexpr uint16_t kWhite       =  0xFFFF;  // 255, 255, 255

class cLcd {
public:
  //{{{
  cLcd (const uint16_t width, const uint16_t height,
        const int spiClock, const bool spiMode0,
        const uint8_t resetGpio, const uint8_t dataCommandGpio, const uint8_t chipEnableGpio)
    : mWidth(width), mHeight(height),
      mSpiClock(spiClock), mSpiMode0(spiMode0),
      mUseSequence((dataCommandGpio == 0xFF) && (chipEnableGpio != 0xFF)),
      mResetGpio(resetGpio), mDataCommandGpio(dataCommandGpio), mChipEnableGpio(chipEnableGpio) {
    }
  //}}}
  virtual ~cLcd();
  virtual bool initialise() = 0;

  constexpr uint16_t getWidth() { return mWidth; }
  constexpr uint16_t getHeight() { return mHeight; }

  void rect (const uint16_t colour, const int xorg, const int yorg, const int xlen, const int ylen);
  void pixel (const uint16_t colour, const int x, const int y);
  void blendPixel (const uint16_t colour, const uint8_t alpha, const int x, const int y);
  int text (const uint16_t colour, const int strX, const int strY, const int height, const std::string& str);
  void clear (const uint16_t colour) { rect (colour, 0,0, getWidth(), getHeight()); }

  void update() { mUpdate = true; }
  void setAutoUpdate() { mAutoUpdate = true; }
  void delayUs (const int us);

//{{{
protected:
  bool initResources();

  void writeCommand (const uint8_t command);
  void writeCommandData (const uint8_t command, const uint16_t data);
  void writeCommandMultipleData (const uint8_t command, const uint8_t* data, const int len);

  void launchUpdateThread (const uint8_t command);
//}}}
//{{{
private:
  void setFont (const uint8_t* font, const int fontSize);

  const uint16_t mWidth;
  const uint16_t mHeight;

  bool mUpdate = false;
  bool mChanged = false;
  bool mAutoUpdate = false;
  uint16_t* mFrameBuf = nullptr;  // bigEndian RGB565 uint16 colour pixels

  const int mSpiClock;
  const bool mSpiMode0;

  const bool mUseSequence;
  const uint8_t mResetGpio;
  const uint8_t mDataCommandGpio;
  const uint8_t mChipEnableGpio;

  int mHandle = 0;
//}}}
  };

//{{{
class cLcd7735 : public cLcd {
public:
  cLcd7735();
  virtual ~cLcd7735() {}
  virtual bool initialise();
  };
//}}}
//{{{
class cLcd9225b : public cLcd {
public:
  cLcd9225b();
  virtual ~cLcd9225b() {}
  virtual bool initialise();
  };
//}}}
//{{{
class cLcd9320 : public cLcd {
public:
  cLcd9320();
  virtual ~cLcd9320() {}
  virtual bool initialise();
  };
//}}}
