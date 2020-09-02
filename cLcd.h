// cLcd - uint16 rgb565 display classes
#pragma once
//{{{  includes
#include <cstdint>
#include <string>

#include <bcm_host.h>
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
  cPoint operator - (const cPoint& point) const {
    return cPoint (x - point.x, y - point.y);
    }
  //}}}
  //{{{
  cPoint operator + (const cPoint& point) const {
    return cPoint (x + point.x, y + point.y);
    }
  //}}}
  //{{{
  cPoint operator * (const int16_t f) const {
    return cPoint (x * f, y * f);
    }
  //}}}
  //{{{
  cPoint operator * (const cPoint& point) const {
    return cPoint (x * point.x, y * point.y);
    }
  //}}}
  //{{{
  cPoint operator / (const int16_t f) const {
    return cPoint (x / f, y / f);
    }
  //}}}

  //{{{
  const cPoint& operator += (const cPoint& point)  {
    x += point.x;
    y += point.y;
    return *this;
    }
  //}}}
  //{{{
  const cPoint& operator -= (const cPoint& point)  {
    x -= point.x;
    y -= point.y;
    return *this;
    }
  //}}}

  //{{{
  bool inside (const cPoint& pos) const {
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
  cRect operator + (const cPoint& point) const {
    return cRect (left + point.x, top + point.y, right + point.x, bottom + point.y);
    }
  //}}}

  int16_t getWidth() const { return right - left; }
  int16_t getHeight() const { return bottom - top; }
  int getNumPixels() const { return getWidth() * getHeight(); }

  cPoint getTL() const { return cPoint(left, top); }
  cPoint getTL (int16_t offset) const { return cPoint(left+offset, top+offset); }
  cPoint getTR() const { return cPoint(right, top); }
  cPoint getBL() const { return cPoint(left, bottom); }
  cPoint getBR() const { return cPoint(right, bottom); }

  cPoint getSize() const { return cPoint(right-left, bottom-top); }
  cPoint getCentre() const { return cPoint(getCentreX(), getCentreY()); }
  int16_t getCentreX() const { return (left + right)/2; }
  int16_t getCentreY() const { return (top + bottom)/2; }

  //{{{
  bool inside (const cPoint& pos) const {
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
  cLcd (const int16_t width, const int16_t height, const int rotate);
  virtual ~cLcd();

  virtual bool initialise();

  constexpr int16_t getWidth() { return mWidth; }
  constexpr int16_t getHeight() { return mHeight; }
  const cPoint getSize() { return cPoint(mWidth, mHeight); }
  const int getNumPixels() { return mWidth * mHeight; }
  const int getUpdateUs() { return mUpdateUs; }

  virtual void rect (const uint16_t colour, const cRect& r);
  virtual void pixel (const uint16_t colour, const cPoint& p);
  virtual void blendPixel (const uint16_t colour, const uint8_t alpha, const cPoint& p);
  virtual void copy (const uint16_t* src, const cRect& srcRect, const cPoint& dstPoint);
  virtual void copy (const uint16_t* src);

  void rectOutline (const uint16_t colour, const cRect& r);
  void rect (const uint16_t colour, const uint8_t alpha, const cRect& r);
  int text (const uint16_t colour, const cPoint& p, const int height, const std::string& str);

  void clear (const uint16_t colour) { rect (colour, cRect(0,0, getWidth(), getHeight())); }

  void update() { mUpdate = true; }
  void setAutoUpdate() { mAutoUpdate = true; }

  void delayUs (const int us);
  double time();

  // main display screen copy
  const int getDiffUs() { return mDiffUs; }
  const int getNumSpans() { return mNumDiffSpans; }
  bool snap();

//{{{
protected:
  virtual void writeCommand (const uint8_t command) = 0;
  virtual void writeCommandData (const uint8_t command, const uint16_t data) = 0;
  virtual void writeCommandMultiData (const uint8_t command, const uint8_t* dataPtr, const int len) = 0;

  virtual void updateLcd (const uint16_t* buf, const cRect& r) = 0;

  void launchUpdateThread();

  int mRotate = 0;
  bool mChanged = false;
  int mUpdateUs = 0;

  uint16_t* mFrameBuf = nullptr;  // uint16 colour pixels
//}}}
//{{{
private:
  void setFont (const uint8_t* font, const int fontSize);

  sSpan* diffSingle();
  sSpan* diffCoarse();
  sSpan* diffExact();
  sSpan* merge (int pixelThreshold);

  static int coarseLinearDiff (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd);
  static int coarseLinearDiffBack (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd);

  const int16_t mWidth;
  const int16_t mHeight;

  bool mUpdate = false;
  bool mAutoUpdate = false;
  int mDiffUs = 0;

  bool mExit = false;
  bool mExited = false;

  // main display screen buffers
  uint16_t* mBuf = nullptr;
  uint16_t* mPrevBuf = nullptr;

  // dispmanx
  DISPMANX_DISPLAY_HANDLE_T mDisplay;
  DISPMANX_MODEINFO_T mModeInfo;
  DISPMANX_RESOURCE_HANDLE_T mScreenGrab;
  uint32_t mImagePrt;
  VC_RECT_T mVcRect;

  // diff spans
  sSpan* mDiffSpans;
  int mNumDiffSpans;
//}}}
  };

//{{{
class cLcd16 : public cLcd {
public:
  cLcd16 (const int16_t width, const int16_t height, const int rotate) : cLcd (width, height, rotate) {}
  virtual ~cLcd16() {}

protected:
  virtual void writeCommand (const uint8_t command);
  virtual void writeCommandData (const uint8_t command, const uint16_t data);
  virtual void writeCommandMultiData (const uint8_t command, const uint8_t* data, const int len);
  };
//}}}
//{{{
class cLcdSpi : public cLcd {
public:
  cLcdSpi (const int16_t width, const int16_t height, const int rotate) : cLcd (width, height, rotate) {}
  virtual ~cLcdSpi();

protected:
  int mSpiHandle = 0;
  };
//}}}
//{{{
class cLcdSpiHeaderSelect : public cLcdSpi {
public:
  cLcdSpiHeaderSelect (const int16_t width, const int16_t height, const int rotate)
    : cLcdSpi (width, height, rotate) {}
  virtual ~cLcdSpiHeaderSelect() {}

protected:
  virtual void writeCommand (const uint8_t command);
  virtual void writeCommandData (const uint8_t command, const uint16_t data);
  virtual void writeCommandMultiData (const uint8_t command, const uint8_t* dataPtr, const int len);
  };
//}}}
//{{{
class cLcdSpiRegisterSelect : public cLcdSpi {
public:
  cLcdSpiRegisterSelect (const int16_t width, const int16_t height, const int rotate)
    : cLcdSpi (width, height, rotate) {}

  virtual ~cLcdSpiRegisterSelect() {}

protected:
  virtual void writeCommand (const uint8_t command);
  virtual void writeCommandData (const uint8_t command, const uint16_t data);
  virtual void writeCommandMultiData (const uint8_t command, const uint8_t* dataPtr, const int len);
  };
//}}}

// parallel 16bit screens
//{{{
class cLcdTa7601 : public cLcd16 {
public:
  cLcdTa7601 (const int rotate = 0);
  virtual ~cLcdTa7601() {}

  virtual bool initialise();
  virtual void updateLcd (const uint16_t* buf, const cRect& r);
  };
//}}}
//{{{
class cLcdSsd1289 : public cLcd16 {
public:
  cLcdSsd1289 (const int rotate = 0);
  virtual ~cLcdSsd1289() {}

  virtual bool initialise();
  virtual void updateLcd (const uint16_t* buf, const cRect& r);
  };
//}}}

// spi screens
//{{{
class cLcdIli9320 : public cLcdSpiHeaderSelect {
// 2.8 inch 1240x320 - HY28A
public:
  cLcdIli9320 (const int rotate = 0);
  virtual ~cLcdIli9320() {}

  virtual bool initialise();
  virtual void updateLcd (const uint16_t* buf, const cRect& r);
  };
//}}}
//{{{
class cLcdSt7735r : public cLcdSpiRegisterSelect {
// 1.8 inch 128x160
public:
  cLcdSt7735r (const int rotate = 0);
  virtual ~cLcdSt7735r() {}

  virtual bool initialise();
  virtual void updateLcd (const uint16_t* buf, const cRect& r);
  };
//}}}
//{{{
class cLcdIli9225b : public cLcdSpiRegisterSelect {
// 2.2 inch 186x220
public:
  cLcdIli9225b (const int rotate = 0);
  virtual ~cLcdIli9225b() {}

  virtual bool initialise();
  virtual void updateLcd (const uint16_t* buf, const cRect& r);
  };
//}}}
