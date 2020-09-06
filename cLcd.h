// cLcd - rgb565 display classes
#pragma once
//{{{  includes
#include <cstdint>
#include <string>

#include <bcm_host.h>

#include "../shared/utils/utils.h"
//}}}

//{{{
struct cPoint {
public:
  //{{{
  cPoint()  {
    x = 0;
    y = 0;
    }
  //}}}
  //{{{
  cPoint (const int16_t x, const int16_t y) {
    this->x = x;
    this->y = y;
    }
  //}}}

  //{{{
  cPoint operator - (const cPoint& point) {
    return cPoint (x - point.x, y - point.y);
    }
  //}}}
  //{{{
  cPoint operator + (const cPoint& point) {
    return cPoint (x + point.x, y + point.y);
    }
  //}}}
  //{{{
  cPoint operator * (const int16_t f) {
    return cPoint (x * f, y * f);
    }
  //}}}
  //{{{
  cPoint operator * (const cPoint& point) {
    return cPoint (x * point.x, y * point.y);
    }
  //}}}
  //{{{
  cPoint operator / (const int16_t f) {
    return cPoint (x / f, y / f);
    }
  //}}}

  //{{{
  cPoint& operator += (const cPoint& point)  {
    x += point.x;
    y += point.y;
    return *this;
    }
  //}}}
  //{{{
  cPoint& operator -= (const cPoint& point)  {
    x -= point.x;
    y -= point.y;
    return *this;
    }
  //}}}

  //{{{
  bool inside (const cPoint& pos) {
  // return pos inside rect formed by us as size
    return pos.x >= 0 && pos.x < x && pos.y >= 0 && pos.y < y;
    }
  //}}}

  int16_t x;
  int16_t y;
  };
//}}}
//{{{
struct cRect {
public:
  //{{{
  cRect() {
    left = 0;
    bottom = 0;
    right = 0;
    bottom = 0;
    }
  //}}}
  //{{{
  cRect (const int16_t l, const int16_t t, const int16_t r, const int16_t b)  {
    left = l;
    top = t;
    right = r;
    bottom = b;
    }
  //}}}
  //{{{
  cRect (const cPoint& topLeft, const cPoint& bottomRight)  {
    left = topLeft.x;
    top = topLeft.y;
    right = bottomRight.x;
    bottom = bottomRight.y;
    }
  //}}}

  //{{{
  cRect operator + (const cPoint& point) {
    return cRect (left + point.x, top + point.y, right + point.x, bottom + point.y);
    }
  //}}}

  int16_t getWidth() { return right - left; }
  int16_t getHeight() { return bottom - top; }
  int getNumPixels() { return getWidth() * getHeight(); }

  cPoint getTL() { return cPoint(left, top); }
  cPoint getTL (const int16_t offset) { return cPoint(left+offset, top+offset); }
  cPoint getTR() { return cPoint(right, top); }
  cPoint getBL() { return cPoint(left, bottom); }
  cPoint getBR() { return cPoint(right, bottom); }

  cPoint getSize() { return cPoint(right-left, bottom-top); }
  cPoint getCentre() { return cPoint(getCentreX(), getCentreY()); }
  int16_t getCentreX() { return (left + right)/2; }
  int16_t getCentreY() { return (top + bottom)/2; }

  //{{{
  std::string getString() {
    return "l:" + dec(left) + " r:" + dec(right) + " t:" + dec(top) + " b:" + dec(bottom);
    }
  //}}}
  //{{{
  std::string getYfirstString() {
    return "t:" + dec(top) + " b:" + dec(bottom) + " l:" + dec(left) + " r:" + dec(right);
    }
  //}}}
  //{{{
  bool inside (const cPoint& pos) {
  // return pos inside rect
    return (pos.x >= left) && (pos.x < right) && (pos.y >= top) && (pos.y < bottom);
    }
  //}}}

  int16_t left;
  int16_t top;
  int16_t right;
  int16_t bottom;
  };
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

struct sSpan;
class cLcd {
public:
  enum eRotate { e0, e90, e180, e270 };
  enum eInfo { eNone, eOverlay };
  enum eMode { eAll, eSingle, eCoarse, eExact };

  //{{{
  cLcd (const int16_t width, const int16_t height, const eRotate rotate, const eInfo info, const eMode mode)
      : mRotate(rotate), mInfo(info),
        mSnapshotEnabled(true), mTypeEnabled(true), mMode(mode),
        mWidth(((rotate == e90) || (rotate == e270)) ? height : width),
        mHeight(((rotate == e90) || (rotate == e270)) ? width : height) {}
  //}}}
  virtual ~cLcd();

  virtual bool initialise();

  constexpr uint16_t getWidth() { return mWidth; }
  constexpr uint16_t getHeight() { return mHeight; }
  constexpr uint32_t getNumPixels() { return mWidth * mHeight; }
  cPoint getSize() { return cPoint(mWidth, mHeight); }
  cRect getRect() { return cRect(0,0, mWidth,mHeight); }

  std::string getInfoString();
  std::string getPaddedInfoString();

  virtual void setBacklight (bool on) {}
  void setBacklightOn() { setBacklight (true); }
  void setBacklightOff() { setBacklight (false); }

  void clear (const uint16_t colour = kBlack);
  void clearSnapshot();
  bool present();

  void rect (const uint16_t colour, const cRect& r);
  void rect (const uint16_t colour, const uint8_t alpha, const cRect& r);
  void rectOutline (const uint16_t colour, const cRect& r);
  void pix (const uint16_t colour, const uint8_t alpha, const cPoint& p);
  void copy (const uint16_t* src);
  void copy (const uint16_t* src, cRect& srcRect, const cPoint& dstPoint);
  int text (const uint16_t colour, const cPoint& p, const int height, const std::string& str);

  void delayUs (const int us);
  double timeUs();

//{{{
protected:
  virtual void writeCommand (const uint8_t command) = 0;
  virtual void writeDataWord (const uint16_t data) = 0;
  //{{{
  void writeCommandData (const uint8_t command, const uint16_t data) {
    writeCommand (command);
    writeDataWord (data);
    }
  //}}}

  virtual uint32_t updateLcd (sSpan* spans) = 0;
  uint32_t updateLcdAll();

  const eRotate mRotate;
  const eInfo mInfo;

  int mUpdateUs = 0;
  uint32_t mUpdatePixels = 0;

  // uint16 colour pixels
  uint16_t* mFrameBuf = nullptr;
  uint16_t* mPrevFrameBuf = nullptr;
//}}}
//{{{
private:
  // get info
  int getUpdatePixels() { return mUpdatePixels; }
  int getUpdateUs() { return mUpdateUs; }
  int getDiffUs() { return mDiffUs; }
  int getNumSpans() { return mNumSpans; }

  void setFont (const uint8_t* font, const int fontSize);

  // diff
  int diffExact (sSpan* spans);
  int diffCoarse (sSpan* spans);
  int diffSingle (sSpan* spans);

  static sSpan* merge (sSpan* spans, int pixelThreshold);
  static int coarseLinearDiff (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd);
  static int coarseLinearDiffBack (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd);

  // vars
  const bool mSnapshotEnabled;
  const bool mTypeEnabled;

  const eMode mMode = eSingle;
  const uint16_t mWidth;
  const uint16_t mHeight;

  int mDiffUs = 0;

  // diff spans
  sSpan* mSpans = nullptr;
  int mNumSpans = 0;

  // dispmanx
  DISPMANX_DISPLAY_HANDLE_T mDisplay;
  DISPMANX_MODEINFO_T mModeInfo;
  DISPMANX_RESOURCE_HANDLE_T mSnapshot;
  VC_RECT_T mVcRect;
//}}}
  };

// parallel 16bit screens
//{{{
class cLcdTa7601 : public cLcd {
public:
  cLcdTa7601 (const eRotate rotate = e0, const eInfo info = eNone, const eMode mode = eCoarse);
  virtual ~cLcdTa7601() {}

  virtual bool initialise();

protected:
  virtual void writeCommand (const uint8_t command);
  virtual void writeDataWord (const uint16_t data);

  virtual uint32_t updateLcd (sSpan* spans);
  };
//}}}
//{{{
class cLcdSsd1289 : public cLcd {
public:
  cLcdSsd1289 (const eRotate rotate = e0, const eInfo info = eNone, const eMode mode = eCoarse);
  virtual ~cLcdSsd1289() {}

protected:
  virtual void writeCommand (const uint8_t command);
  virtual void writeDataWord (const uint16_t data);

  virtual bool initialise();
  virtual uint32_t updateLcd (sSpan* spans);

private:
  void writeCommandMultiData (const uint8_t command, const uint8_t* data, const int len);
  };
//}}}

// spi - data/command register pin screens
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

  int mSpiHandle = 0;
  };
//}}}
//{{{
class cLcdSt7735r : public cLcdSpiRegister {
// 1.8 inch 128x160
public:
  cLcdSt7735r (const cLcd::eRotate rotate = e0, const cLcd::eInfo info = eNone, const eMode mode = eAll);
  virtual ~cLcdSt7735r() {}

  virtual bool initialise();

protected:
  virtual uint32_t updateLcd (sSpan* spans);
  };
//}}}
//{{{
class cLcdIli9225b : public cLcdSpiRegister {
// 2.2 inch 186x220
public:
  cLcdIli9225b (const cLcd::eRotate rotate = e0, const cLcd::eInfo info = eNone, const eMode mode = eAll);
  virtual ~cLcdIli9225b() {}

  virtual bool initialise();

protected:
  virtual uint32_t updateLcd (sSpan* spans);
  };
//}}}

// spi - no data/command register pin screens
//{{{
class cLcdSpiHeader : public cLcd {
public:
  cLcdSpiHeader (const int16_t width, const int16_t height, const eRotate rotate, const eInfo info, const eMode mode)
    : cLcd (width, height, rotate, info, mode) {}
  virtual ~cLcdSpiHeader();

protected:
  virtual void writeCommand (const uint8_t command);
  virtual void writeDataWord (const uint16_t data);

  int mSpiHandle = 0;
  };
//}}}
//{{{
class cLcdIli9320 : public cLcdSpiHeader {
// 2.8 inch 1240x320 - HY28A
public:
  cLcdIli9320 (const cLcd::eRotate rotate = e0, const cLcd::eInfo info = eNone, const eMode mode = eAll);
  virtual ~cLcdIli9320() {}

  virtual void setBacklight (bool on);

  virtual bool initialise();

protected:
  virtual uint32_t updateLcd (sSpan* spans);
  };
//}}}
