// cLcd - rgb565 display classes
#pragma once
//{{{  includes
#include <cstdint>
#include <string>
#include "cPointRect.h"

struct sSpan;
class cDrawAA;
class cFrameDiff;
class cSnapshot;
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

//{{{
class cLcd {
public:
  enum eRotate { e0, e90, e180, e270 };
  enum eInfo { eNone, eOverlay };
  enum eMode { eAll, eSingle, eCoarse, eExact };

  cLcd (const int16_t width, const int16_t height, const eRotate rotate, const eInfo info, const eMode mode);
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
  void reset();

  virtual void writeCommand (const uint8_t command) = 0;
  virtual void writeDataWord (const uint16_t data) {}
  virtual void writeMultiData (const uint8_t* data, int count) {}

  //{{{
  void writeCommandData (const uint8_t command, const uint16_t data) {
    writeCommand (command);
    writeDataWord (data);
    }
  //}}}
  //{{{
  void writeCommandMultiData (const uint8_t command, uint8_t* data, int length) {
    writeCommand (command);
    writeMultiData (data, length);
    }
  //}}}

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
//}}}

// spi header classes
//{{{
class cLcdSpiHeader : public cLcd {
public:
  cLcdSpiHeader (const int16_t width, const int16_t height, const eRotate rotate, const eInfo info, const eMode mode);
  virtual ~cLcdSpiHeader();

protected:
  virtual void writeCommand (const uint8_t command);
  virtual void writeDataWord (const uint16_t data);

  int mSpiHandle = 0;
  };
//}}}
//{{{
class cLcd9320 : public cLcdSpiHeader {
// 2.8 inch 1240x320 - HY28A
public:
  cLcd9320 (cLcd::eRotate rotate, cLcd::eInfo info, eMode mode);
  virtual ~cLcd9320() {}

  virtual void setBacklight (bool on);

  virtual bool initialise();

protected:
  virtual uint32_t updateLcd (sSpan* spans);
  };
//}}}

// spi rs pin classes
//{{{
class cLcdSpi : public cLcd {
public:
  cLcdSpi (int16_t width, int16_t height, eRotate rotate, eInfo info, eMode mode, int spiSpeed, int registerGpio);
  virtual ~cLcdSpi();

protected:
  virtual void writeCommand (const uint8_t command);
  virtual void writeDataWord (const uint16_t data);
  virtual void writeMultiData (uint8_t* data, int length);

  const int mSpiSpeed = 0;
  const int mRegisterGpio = 0;

  int mSpiHandle = 0;
  };
//}}}
//{{{
class cLcd7735 : public cLcdSpi {
// 1.8 inch 128x160
public:
  cLcd7735 (cLcd::eRotate rotate, cLcd::eInfo info, eMode mode, int spiSpeed);
  virtual ~cLcd7735() {}

  virtual bool initialise();

protected:
  virtual uint32_t updateLcd (sSpan* spans);
  };
//}}}
//{{{
class cLcd9225 : public cLcdSpi {
// 2.2 inch 186x220
public:
  cLcd9225 (cLcd::eRotate rotate, cLcd::eInfo info, eMode mode, int spiSpeed);
  virtual ~cLcd9225() {}

  virtual bool initialise();

protected:
  virtual uint32_t updateLcd (sSpan* spans);
  };
//}}}
//{{{
class cLcd9341 : public cLcdSpi {
public:
  cLcd9341 (cLcd::eRotate rotate, cLcd::eInfo info, eMode mode, int spiSpeed);
  virtual ~cLcd9341() {}

  virtual bool initialise();

protected:
  virtual uint32_t updateLcd (sSpan* spans);
  };
//}}}

// parallel classes
//{{{
class cLcd7601 : public cLcd {
public:
  cLcd7601 (eRotate rotate, eInfo info, eMode mode);
  virtual ~cLcd7601() {}

  virtual bool initialise();
  virtual void setBacklight (bool on);

protected:
  virtual void writeCommand (const uint8_t command);
  virtual void writeDataWord (const uint16_t data);

  virtual uint32_t updateLcd (sSpan* spans);
  };
//}}}
//{{{
class cLcd1289 : public cLcd {
public:
  cLcd1289 (eRotate rotate, eInfo info, eMode mode);
  virtual ~cLcd1289() {}

protected:
  virtual void writeCommand (const uint8_t command);
  virtual void writeDataWord (const uint16_t data);

  virtual bool initialise();
  virtual uint32_t updateLcd (sSpan* spans);
  };
//}}}
//{{{
class cLcd9341p8 : public cLcd {
public:
  cLcd9341p8 (eRotate rotate, eInfo info, eMode mode);
  virtual ~cLcd9341p8() {}

  virtual bool initialise();

protected:
  virtual void writeCommand (const uint8_t command);
  virtual void writeMultiData (const uint8_t* data, int count);
  void writeMultiWordData (const uint16_t* data, int count);

  virtual uint32_t updateLcd (sSpan* spans);
  };
//}}}
//{{{
class cLcd9341p16 : public cLcd {
public:
  cLcd9341p16 (eRotate rotate, eInfo info, eMode mode);
  virtual ~cLcd9341p16() {}

  virtual bool initialise();

protected:
  virtual void writeCommand (const uint8_t command);
  virtual void writeMultiData (const uint8_t* data, int count);
  void writeMultiWordData (const uint16_t* data, int count);

  virtual uint32_t updateLcd (sSpan* spans);
  };
//}}}
