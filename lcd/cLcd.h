// cLcd - rgb565 display classes
#pragma once
#include <cstdint>
#include <string>
#include "cPointRect.h"
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

struct sSpan;
class cDrawAA;
class cFrameDiff;
class cSnapshot;
class cLcd {
public:
  enum eRotate { e0, e90, e180, e270 };
  enum eInfo { eNone, eOverlay };
  enum eMode { eAll, eSingle, eCoarse, eExact };

  //{{{
  cLcd (const int16_t width, const int16_t height, const eRotate rotate, const eInfo info, const eMode mode)
      : mRotate(rotate), mInfo(info), mMode(mode),
        mWidth(((rotate == e90) || (rotate == e270)) ? height : width),
        mHeight(((rotate == e90) || (rotate == e270)) ? width : height),
        mSnapshotEnabled(true), mTypeEnabled(true) {}
  //}}}
  virtual ~cLcd();

  virtual bool initialise();

  constexpr uint16_t getWidth() { return mWidth; }
  constexpr uint16_t getHeight() { return mHeight; }
  constexpr uint32_t getNumPixels() { return mWidth * mHeight; }
  cPoint getSize() { return cPoint(mWidth, mHeight); }
  cRect getRect() { return cRect(0,0, mWidth,mHeight); }

  //{{{  backlight
  virtual void setBacklight (bool on) {}
  void setBacklightOn() { setBacklight (true); }
  void setBacklightOff() { setBacklight (false); }
  //}}}

  // present
  void clear (const uint16_t colour = kBlack);
  void snapshot();
  bool present();

  void pix (const uint16_t colour, const uint8_t alpha, const cPoint& p);
  void copy (const uint16_t* src, cRect& srcRect, const uint16_t srcStride, const cPoint& dstPoint);

  // gradient
  void hGrad (const uint16_t colourL, const uint16_t colourR, const cRect& r);
  void vGrad (const uint16_t colourT, const uint16_t colourB, const cRect& r);
  void grad (const uint16_t colourTL ,const uint16_t colourTR,
             const uint16_t colourBL, const uint16_t colourBR, const cRect& r);
  // draw
  void rect (const uint16_t colour, const cRect& r);
  void rect (const uint16_t colour, const uint8_t alpha, const cRect& r);
  void rectOutline (const uint16_t colour, const cRect& r);
  void ellipse (const uint16_t colour, const uint8_t alpha, cPoint centre, cPoint radius);
  void ellipseOutline (const uint16_t colour, cPoint centre, cPoint radius);
  void line (const uint16_t colour, cPoint p1, cPoint p2);

  // aa draw
  void moveToAA (const cPointF& p);
  void lineToAA (const cPointF& p);
  void renderAA (const uint16_t colour, bool fillNonZero);

  // aa draw helpers
  void wideLineAA (const cPointF& p1, const cPointF& p2, float width);
  void pointedLineAA (const cPointF& p1, const cPointF& p2, float width);
  void ellipseAA (const cPointF& centre, const cPointF& radius, int steps);
  void ellipseOutlineAA (const cPointF& centre, const cPointF& radius, float width, int steps);

  int text (const uint16_t colour, const cPoint& p, const int height, const std::string& str);

  void delayUs (const int us);
  double timeUs();

//{{{
protected:
  virtual void writeCommand (const uint8_t command) = 0;
  virtual void writeDataWord (const uint16_t data) = 0;
  //{{{
  virtual void writeCommandData (const uint8_t command, const uint16_t data) {

    writeCommand (command);
    writeDataWord (data);
    }
  //}}}

  virtual uint16_t readId() = 0;
  virtual uint16_t readStatus() = 0;

  virtual uint32_t updateLcd (sSpan* spans) = 0;

  // vars
  const eRotate mRotate;
  const eInfo mInfo;
  const eMode mMode = eSingle;

  const uint16_t mWidth;
  const uint16_t mHeight;

  uint32_t mUpdatePixels = 0;
  int mUpdateUs = 0;

  // uint16_t colour frameBufs
  uint16_t* mFrameBuf = nullptr;

  // span for whole screen
  sSpan* mSpanAll = nullptr;
//}}}
//{{{
private:
  // get info
  int getUpdatePixels() { return mUpdatePixels; }
  int getUpdateUs() { return mUpdateUs; }
  int getDiffUs() { return mDiffUs; }

  std::string getInfoString();
  std::string getPaddedInfoString();

  void setFont (const uint8_t* font, const int fontSize);

  // vars
  const bool mSnapshotEnabled;
  const bool mTypeEnabled;

  cDrawAA* mDrawAA = nullptr;
  uint8_t mGamma[256];

  cFrameDiff* mFrameDiff = nullptr;
  int mDiffUs = 0;

  cSnapshot* mSnapshot = nullptr;
//}}}
  };

// parallel 16bit screens
//{{{
class cLcd7601 : public cLcd {
public:
  cLcd7601 (const eRotate rotate = e0, const eInfo info = eNone, const eMode mode = eCoarse);
  virtual ~cLcd7601() {}

  virtual bool initialise();
  virtual void setBacklight (bool on);

protected:
  virtual void writeCommand (const uint8_t command);
  virtual void writeDataWord (const uint16_t data);
  virtual uint16_t readId() { return 0xFAFF; }
  virtual uint16_t readStatus() { return 0xAA; }

  virtual uint32_t updateLcd (sSpan* spans);
  };
//}}}
//{{{
class cLcd1289 : public cLcd {
public:
  cLcd1289 (const eRotate rotate = e0, const eInfo info = eNone, const eMode mode = eCoarse);
  virtual ~cLcd1289() {}

protected:
  virtual void writeCommand (const uint8_t command);
  virtual void writeDataWord (const uint16_t data);
  virtual uint16_t readId() { return 0xFAFF; }
  virtual uint16_t readStatus() { return 0xAA; }

  virtual bool initialise();
  virtual uint32_t updateLcd (sSpan* spans);
  };
//}}}

// spi data/command register pin screens
//{{{
class cLcdSpiRegister : public cLcd {
public:
  cLcdSpiRegister (const int16_t width, const int16_t height, const eRotate rotate, const eInfo info, const eMode mode)
    : cLcd (width, height, rotate, info, mode) {}
  virtual ~cLcdSpiRegister();

protected:
  virtual void writeCommand (const uint8_t command);
  virtual void writeDataWord (const uint16_t data);
  void writeCommandMultiData (const uint8_t command, const uint8_t* dataPtr, const int len);

  virtual uint16_t readId() { return 0xFAFF; }
  virtual uint16_t readStatus() { return 0xAA; }

  int mSpiHandle = 0;
  };
//}}}
//{{{
class cLcd7735 : public cLcdSpiRegister {
// 1.8 inch 128x160
public:
  cLcd7735 (const cLcd::eRotate rotate = e0, const cLcd::eInfo info = eNone, const eMode mode = eCoarse);
  virtual ~cLcd7735() {}

  virtual bool initialise();

protected:
  virtual uint32_t updateLcd (sSpan* spans);
  };
//}}}
//{{{
class cLcd9225 : public cLcdSpiRegister {
// 2.2 inch 186x220
public:
  cLcd9225 (const cLcd::eRotate rotate = e0, const cLcd::eInfo info = eNone, const eMode mode = eCoarse);
  virtual ~cLcd9225() {}

  virtual bool initialise();

protected:
  virtual uint32_t updateLcd (sSpan* spans);
  };
//}}}
//{{{
class cLcd9341 : public cLcdSpiRegister {
// 2.2 inch 186x220
public:
  cLcd9341 (const cLcd::eRotate rotate = e0, const cLcd::eInfo info = eNone, const eMode mode = eCoarse);
  virtual ~cLcd9341() {}

  virtual bool initialise();

protected:
  virtual uint32_t updateLcd (sSpan* spans);
  };
//}}}

// spi no data/command register pin screens
//{{{
class cLcdSpiHeader : public cLcd {
public:
  cLcdSpiHeader (const int16_t width, const int16_t height, const eRotate rotate, const eInfo info, const eMode mode)
    : cLcd (width, height, rotate, info, mode) {}
  virtual ~cLcdSpiHeader();

protected:
  virtual void writeCommand (const uint8_t command);
  virtual void writeDataWord (const uint16_t data);

  virtual uint16_t readId();
  virtual uint16_t readStatus();

  int mSpiHandle = 0;
  };
//}}}
//{{{
class cLcd9320 : public cLcdSpiHeader {
// 2.8 inch 1240x320 - HY28A
public:
  cLcd9320 (const cLcd::eRotate rotate = e0, const cLcd::eInfo info = eNone, const eMode mode = eCoarse);
  virtual ~cLcd9320() {}

  virtual void setBacklight (bool on);

  virtual bool initialise();

protected:
  virtual uint32_t updateLcd (sSpan* spans);
  };
//}}}
