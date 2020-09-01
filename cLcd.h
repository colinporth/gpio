// cLcd - uint16 rgb565 display classes
#pragma once
//{{{  includes
#include <cstdint>
#include <string>
//}}}

//{{{  colours - uint16 RGB565
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
//}}}

class cLcd {
public:
  cLcd (const uint16_t width, const uint16_t height, const int rotate)
    : mRotate(rotate),
      mWidth(((rotate == 90) || (rotate == 270)) ? height : width),
      mHeight(((rotate == 90) || (rotate == 270)) ? width : height) {}
  virtual ~cLcd();

  virtual bool initialise() = 0;

  constexpr uint16_t getWidth() { return mWidth; }
  constexpr uint16_t getHeight() { return mHeight; }
  const int getUpdateUs() { return mUpdateUs; }

  virtual void setRotate (int rotate) {}

  virtual void rect (const uint16_t colour, const int xorg, const int yorg, const int xlen, const int ylen);
  virtual void pixel (const uint16_t colour, const int x, const int y);
  virtual void blendPixel (const uint16_t colour, const uint8_t alpha, const int x, const int y);

  int text (const uint16_t colour, const int strX, const int strY, const int height, const std::string& str);
  void clear (const uint16_t colour) { rect (colour, 0,0, getWidth(), getHeight()); }
  void rectOutline (const uint16_t colour, const int xorg, const int yorg, const int xlen, const int ylen);

  virtual void copy (const uint16_t* fromPtr, const int xlen, const int ylen);
  virtual void copyRotate (const uint16_t* fromPtr, const int xlen, const int ylen);

  void update() { mUpdate = true; }
  void setAutoUpdate() { mAutoUpdate = true; }
  void delayUs (const int us);
  double time();

//{{{
protected:
  bool initResources();
  void reset();
  void initChipEnablePin();

  virtual void writeCommand (const uint8_t command) = 0;
  virtual void writeCommandData (const uint8_t command, const uint16_t data) = 0;
  virtual void writeCommandMultiData (const uint8_t command, const uint8_t* dataPtr, const int len) = 0;

  void launchUpdateThread (const uint8_t command);

  int mRotate = 0;

  bool mChanged = false;
  uint16_t* mFrameBuf = nullptr;  // uint16 colour pixels
//}}}
//{{{
private:
  void setFont (const uint8_t* font, const int fontSize);

  const uint16_t mWidth;
  const uint16_t mHeight;

  bool mUpdate = false;
  bool mAutoUpdate = false;
  int mUpdateUs = 0;

  bool mExit = false;
  bool mExited = false;
//}}}
  };

//{{{
class cLcd16 : public cLcd {
public:
  cLcd16 (const uint16_t width, const uint16_t height, const int rotate) : cLcd (width, height, rotate) {}
  virtual ~cLcd16() {}

protected:
  virtual void rect (const uint16_t colour, const int xorg, const int yorg, const int xlen, const int ylen);
  virtual void pixel (const uint16_t colour, const int x, const int y);
  virtual void blendPixel (const uint16_t colour, const uint8_t alpha, const int x, const int y);

  virtual void writeCommand (const uint8_t command);
  virtual void writeCommandData (const uint8_t command, const uint16_t data);
  virtual void writeCommandMultiData (const uint8_t command, const uint8_t* data, const int len);
  };
//}}}
//{{{
class cLcdTa7601 : public cLcd16 {
public:
  cLcdTa7601 (const int rotate = 0);
  virtual ~cLcdTa7601() {}

  virtual bool initialise();
  };
//}}}
//{{{
class cLcdSsd1289 : public cLcd16 {
public:
  cLcdSsd1289 (const int rotate = 0);
  virtual ~cLcdSsd1289() {}

  virtual bool initialise();
  };
//}}}

//{{{
class cLcdSpiRegisterSelect : public cLcd {
public:
  cLcdSpiRegisterSelect (const uint16_t width, const uint16_t height, const int rotate) : cLcd (width, height, rotate) {}

  virtual ~cLcdSpiRegisterSelect();

protected:
  virtual void writeCommand (const uint8_t command);
  virtual void writeCommandData (const uint8_t command, const uint16_t data);
  virtual void writeCommandMultiData (const uint8_t command, const uint8_t* dataPtr, const int len);

  int mSpiHandle = 0;
  uint8_t mChipEnable = 0xFF;
  };
//}}}
//{{{
class cLcdSt7735r : public cLcdSpiRegisterSelect {
// 1.8 inch 128x160
public:
  cLcdSt7735r (const int rotate = 0);
  virtual ~cLcdSt7735r() {}

  virtual bool initialise();
  };
//}}}
//{{{
class cLcdIli9225b : public cLcdSpiRegisterSelect {
// 2.2 inch 186x220
public:
  cLcdIli9225b (const int rotate = 0);
  virtual ~cLcdIli9225b() {}
  virtual bool initialise();
  };
//}}}

//{{{
class cLcdSpiHeaderSelect : public cLcd {
public:
  cLcdSpiHeaderSelect (const uint16_t width, const uint16_t height, const int rotate) : cLcd (width, height, rotate) {}

  virtual ~cLcdSpiHeaderSelect();

protected:
  virtual void writeCommand (const uint8_t command);
  virtual void writeCommandData (const uint8_t command, const uint16_t data);
  virtual void writeCommandMultiData (const uint8_t command, const uint8_t* dataPtr, const int len);

  int mSpiHandle = 0;
  uint8_t mChipEnable = 0xFF;
  };
//}}}
//{{{
class cLcdIli9320 : public cLcdSpiHeaderSelect {
// 2.8 inch 1240x320 - HY28A
public:
  cLcdIli9320 (const int rotate = 0);
  virtual ~cLcdIli9320() {}

  virtual bool initialise();
  };
//}}}
