// lcd.h
#pragma once

#include <cstdint>
#include <string>

//{{{  colours
// bigEndian colours
constexpr uint16_t Black       =  0x0000;  //   0,   0,   0
constexpr uint16_t Blue        =  0x001F;  //   0,   0, 255
constexpr uint16_t Green       =  0x07E0;  //   0, 255,   0
constexpr uint16_t Cyan        =  0x07FF;  //   0, 255, 255
constexpr uint16_t Red         =  0xF800;  // 255,   0,   0
constexpr uint16_t Magenta     =  0xF81F;  // 255,   0, 255
constexpr uint16_t Yellow      =  0xFFE0;  // 255, 255,   0
constexpr uint16_t White       =  0xFFFF;  // 255, 255, 255

constexpr uint16_t Navy        =  0x000F;  //   0,   0, 128
constexpr uint16_t DarkGreen   =  0x03E0;  //   0, 128,   0
constexpr uint16_t DarkCyan    =  0x03EF;  //   0, 128, 128
constexpr uint16_t Maroon      =  0x7800;  // 128,   0,   0
constexpr uint16_t Purple      =  0x780F;  // 128,   0, 128
constexpr uint16_t Olive       =  0x7BE0;  // 128, 128,   0

constexpr uint16_t LightGrey   =  0xC618;  // 192, 192, 192
constexpr uint16_t DarkGrey    =  0x7BEF;  // 128, 128, 128

constexpr uint16_t Orange      =  0xFD20;  // 255, 165,   0
constexpr uint16_t GreenYellow =  0xAFE5;  // 173, 255,  47
//}}}
//{{{
class cLcd {
public:
  cLcd (const uint8_t width, const uint8_t height, const uint8_t dcPin) :
    mWidth(width), mHeight(height), mDcPin(dcPin) {}
  virtual ~cLcd();

  virtual bool initialise() = 0;
  void setFont (const uint8_t* font, int fontSize);

  constexpr uint8_t getWidth() { return mWidth; }
  constexpr uint8_t getHeight() { return mHeight; }

  void rect (uint16_t colour, int xorg, int yorg, int xlen, int ylen);
  void pixel (uint16_t colour, int x, int y);
  void blendPixel (uint16_t colour, uint8_t alpha, int x, int y);
  int text (uint16_t colour, int strX, int strY, int height, std::string str);

  void clear (uint16_t colour) { rect (colour, 0,0, getWidth(), getHeight()); }

  void update() { mUpdate = true; }
  void setAutoUpdate() { mAutoUpdate = true; }

  void delayMs (int ms);

protected:
  bool initResources();
  void reset (const uint8_t pin);
  void initDcPin (const uint8_t pin);
  void initSpi (const int clockSpeed);

  void command (uint8_t command);
  void commandData (uint8_t command, const uint16_t data);
  void commandData (uint8_t command, const uint8_t* data, int len);

  void launchUpdateThread (uint8_t command);

private:
  const uint8_t mWidth;
  const uint8_t mHeight;

  bool mUpdate = false;
  bool mChanged = false;
  bool mAutoUpdate = false;
  uint16_t* mFrameBuf = nullptr;

  const uint8_t mDcPin;
  int mHandle = 0;
  };
//}}}

class cLcd7735 : public cLcd {
public:
  cLcd7735();
  virtual ~cLcd7735() {}
  virtual bool initialise();
  };

class cLcd9225b : public cLcd {
public:
  cLcd9225b();
  virtual ~cLcd9225b() {}
  virtual bool initialise();
  };
