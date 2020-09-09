// cLcd.cpp - rgb565 lcd
//{{{  includes
#include "cLcd.h"

#include "cDrawAA.h"
#include "cFrameDiff.h"
#include "cSnapshot.h"

#include <byteswap.h>

#include "../pigpio/pigpio.h"

#include "../../shared/utils/utils.h"
#include "../../shared/utils/cLog.h"
#include "../fonts/FreeSansBold.h"

using namespace std;
//}}}
//{{{  include static freetype - assumes singleton cLcd
#include <ft2build.h>
#include FT_FREETYPE_H

static FT_Library mLibrary;
static FT_Face mFace;
//}}}
//{{{  raspberry pi J8 connnector pins
// parallel 16bit J8
//      3.3v led -  1  2  - 5v
//     d2  gpio2 -  3  4  - 5v
//     d3  gpio3 -  5  6  - 0v
//     d4  gpio4 -  7  8  - gpio14 d14
//            0v -  9  10 - gpio15 d15
//     wr gpio17 - 11  12 - gpio18 unused
//   back gpio27 - 13  14 - 0v cs
//     rd gpio22 - 15  16 - gpio23 cs
//          3.3v - 17  18 - gpio24 rs
//    d10 gpio10 - 19  20 - 0v
//     d9  gpio9 - 21  22 - gpio25 reset
//    d11 gpio11 - 23  24 - gpio8  d8
//            0v - 25  26 - gpio7  d7
//     d0  gpio0 - 27  28 - gpio1  d1
//     d5  gpio5 - 29  30 - 0v
//     d6  gpio6 - 31  32 - gpio12 d12
//    d13 gpio13 - 33  34 - 0v
// unused gpio19 - 35  36 - gpio16 wr
// unused gpio26 - 37  38 - gpio20 unused
//            0v - 39  40 - gpio21 unused

// spi J8
//          3.3v - 17  18 - gpio24 rs / back
//          mosi - 19  20 - 0v
//          miso - 21  22 - gpio25 reset
//          sclk - 23  24 - Ce0

constexpr uint8_t kRegisterGpio = 24;  // parallel and spi
constexpr uint8_t kResetGpio = 25;     // parallel and spi

constexpr uint8_t k16WriteGpio      = 17;
constexpr uint8_t k16ReadGpio       = 22;
constexpr uint8_t k16ChipSelectGpio = 23;
constexpr uint8_t k16BacklightGpio  = 27;

constexpr uint32_t k16DataMask     = 0xFFFF;
constexpr uint32_t k16WriteMask    = 1 << k16WriteGpio;
constexpr uint32_t k16WriteClrMask = k16WriteMask | k16DataMask;

constexpr uint8_t kSpiBacklightGpio = 24;
//}}}

// cLcd public
//{{{
cLcd::~cLcd() {

  gpioTerminate();

  free (mFrameBuf);
  free (mSpanAll);

  delete mDrawAA;
  delete mFrameDiff;
  }
//}}}

//{{{
bool cLcd::initialise() {

  cLog::log (LOGINFO, "initialise hwRev:" + hex (gpioHardwareRevision(),8) +
                      " version:" + dec (gpioVersion()) +
                      (mRotate == cLcd::e0 ? "" : dec (mRotate*90)) +
                      (mInfo == cLcd::eOverlay ? " overlay" : "") +
                      (mMode == cLcd::eAll ? " all" :
                         mMode == cLcd::eSingle ? " single" :
                           mMode == cLcd::eCoarse ? " coarse" : " exact"));

  if (gpioInitialise() <= 0)
    return false;

  // reset lcd
  gpioSetMode (kResetGpio, PI_OUTPUT);
  gpioWrite (kResetGpio, 0);
  gpioDelay (10000);
  gpioWrite (kResetGpio, 1);
  gpioDelay (120000);

  // allocate and clear frameBufs, align to data cache
  mFrameBuf = (uint16_t*)aligned_alloc (128, getNumPixels() * 2);
  clear();

  mDrawAA = new cDrawAA();

  // gamma table, !!! duplicate of cDrawAA !!!
  for (unsigned i = 0; i < 256; i++)
    mGamma[i] = (uint8_t)(pow(double(i) / 255.0, 1.6) * 255.0);

  // allocate and init sSpan for all of screen
  mSpanAll = (sSpan*)malloc (sizeof (sSpan));
  *mSpanAll = { getRect(), mWidth, getNumPixels(), nullptr};

  // allocate frameDiff
  switch (mMode) {
    case eAll:
      mFrameDiff = new cAllFrameDiff (mWidth, mHeight);
      break;
    case eSingle:
      mFrameDiff = new cSingleFrameDiff (mWidth, mHeight);
      break;
    case eCoarse:
      mFrameDiff = new cCoarseFrameDiff (mWidth, mHeight);
      break;
    case eExact:
      mFrameDiff = new cExactFrameDiff (mWidth, mHeight);
      break;
    }

  if (mSnapshotEnabled)
    mSnapshot = new cSnapshot (mWidth, mHeight);

  if (mTypeEnabled)
    setFont (getFreeSansBold(), getFreeSansBoldSize());

  return true;
  }
//}}}

//{{{
void cLcd::clear (const uint16_t colour) {
// start update

  uint64_t colour64 = colour;
  colour64 |= (colour64 << 48) | (colour64 << 32) | (colour64 << 16);

  uint64_t* ptr = (uint64_t*)mFrameBuf;
  for (uint32_t i = 0; i < getNumPixels()/4; i++)
    *ptr++ = colour64;
  }
//}}}
//{{{
void cLcd::snapshot() {
// start update, snapshot main display to frameBuffer

  if (mSnapshotEnabled)
    mSnapshot->snap (mFrameBuf);
  else
    cLog::log (LOGERROR, "snapahot not enabled");
  }
//}}}
//{{{
bool cLcd::present() {
// present update

  double diffStartTime = timeUs();
  sSpan* spans = mFrameDiff->diff (mFrameBuf);
  mDiffUs = int((timeUs() - diffStartTime) * 1000000.0);

  if (!spans) {
    // nothing changed
    mUpdateUs = 0;
    return false;
    }

  if (mInfo == eOverlay) // copy frameBuf to prevFrameBuf without overlays
    mFrameDiff->copy (mFrameBuf);

  // updateLcd with diff spans list
  double updateStartTime = timeUs();
  mUpdatePixels = updateLcd (spans);
  mUpdateUs = int((timeUs() - updateStartTime) * 1000000.0);

  if (mInfo == eOverlay) {
    // draw span and info overlays
    sSpan* it = spans;
    while (it) {
      rect (kGreen, 100, it->r);
      it = it->next;
      }
    text (kWhite, cPoint(0,0), 20, getPaddedInfoString());

    // update whole screen with overlays, its saved without them in prevFrameBuffer
    updateLcd (mSpanAll);
    }

  cLog::log (LOGINFO1, getInfoString());

  if (mInfo != eOverlay)
    mFrameBuf = mFrameDiff->swap (mFrameBuf);

  return true;
  }
//}}}

//{{{
void cLcd::pix (const uint16_t colour, const uint8_t alpha, const cPoint& p) {
// blend with clip
// magical rgb565 alpha composite
// - linear interp back * (1.0 - alpha) + fore * alpha
//   - factorized into: result = back + (fore - back) * alpha
//   - alpha is in Q1.5 format, so 0.0 is represented by 0, and 1.0 is represented by 32
// - Converts  0000000000000000rrrrrggggggbbbbb
// -     into  00000gggggg00000rrrrr000000bbbbb

  if ((alpha > 0) && (p.x >= 0) && (p.y >= 0) && (p.x < mWidth) && (p.y < mHeight)) {
    // clip opaque and offscreen
    if (alpha == 0xFF)
      // simple case - set frameBuf pixel to colour
      mFrameBuf[(p.y*mWidth) + p.x] = colour;
    else {
      // get frameBuf back
      uint32_t back = mFrameBuf[(p.y*mWidth) + p.x];

      // composite colour
      uint32_t fore = colour;
      fore = (fore | (fore << 16)) & 0x07e0f81f;
      back = (back | (back << 16)) & 0x07e0f81f;
      back += (((fore - back) * ((alpha + 4) >> 3)) >> 5) & 0x07e0f81f;

      // set frameBuf pixel to back result
      mFrameBuf[(p.y*mWidth) + p.x] = back | (back >> 16);
      }
    }
  }
//}}}
//{{{
void cLcd::copy (const uint16_t* src, cRect& srcRect, const uint16_t srcStride, const cPoint& dstPoint) {
// copy line by line

  for (int y = 0; y < srcRect.getHeight(); y++)
    memcpy (mFrameBuf + ((dstPoint.y + y) * mWidth) + dstPoint.x,
                  src + ((srcRect.top + y) * srcStride) + srcRect.left,
            srcRect.getWidth() * 2);
  }
//}}}
//{{{  grad
//{{{
void cLcd::hGrad (const uint16_t colourL, const uint16_t colourR, const cRect& r) {
// clip right, bottom, should do left top

  int16_t xmax = min (r.right, (int16_t)mWidth);
  int16_t ymax = min (r.bottom, (int16_t)mHeight);

  // draw a line
  int16_t y = r.top;
  uint16_t* dst = mFrameBuf + (y * mWidth) + r.left;
  for (uint16_t x = r.left; x < xmax; x++) {
    uint32_t fore = colourR;
    fore = (fore | (fore << 16)) & 0x07e0f81f;

    uint32_t back = colourL;
    back = (back | (back << 16)) & 0x07e0f81f;

    uint8_t alpha = (x * 0xFF) / (r.right - r.left);
    back += (((fore - back) * ((mGamma[alpha] + 4) >> 3)) >> 5) & 0x07e0f81f;
    back |= back >> 16;

    *dst++ = back;
    }
  y++;

  // copy line to subsequnt lines
  uint16_t* src = mFrameBuf + (r.top * mWidth) + r.left;
  for (; y < ymax; y++) {
    uint16_t* dst = mFrameBuf + (y * mWidth) + r.left;
    memcpy (dst, src, (xmax - r.left) * 2);
    dst += mWidth;
    }
  }
//}}}
//{{{
void cLcd::vGrad (const uint16_t colourT, const uint16_t colourB, const cRect& r) {

  int16_t xmax = min (r.right, (int16_t)mWidth);
  int16_t ymax = min (r.bottom, (int16_t)mHeight);

  for (uint16_t y = r.top; y < ymax; y++) {
    uint16_t* dst = mFrameBuf + (y * mWidth) + r.left;

    uint32_t fore = colourB;
    fore = (fore | (fore << 16)) & 0x07e0f81f;

    uint32_t back = colourT;
    back = (back | (back << 16)) & 0x07e0f81f;

    uint8_t alpha = (y * 0xFF) / (r.bottom - r.top);
    back += (((fore - back) * ((mGamma[alpha] + 4) >> 3)) >> 5) & 0x07e0f81f;
    back |= back >> 16;

    for (uint16_t x = r.left; x < xmax; x++)
      *dst++ = back;
    }
  }
//}}}
//{{{
void cLcd::grad (const uint16_t colourTL ,const uint16_t colourTR,
                 const uint16_t colourBL, const uint16_t colourBR, const cRect& r) {
// !!! check for losing colour res, is the double gamma right ???

  int16_t xmax = min (r.right, (int16_t)mWidth);
  int16_t ymax = min (r.bottom, (int16_t)mHeight);

  for (uint16_t y = r.top; y < ymax; y++) {
    uint16_t* dst = mFrameBuf + (y * mWidth) + r.left;

    for (uint16_t x = r.left; x < xmax; x++) {
      uint32_t colour32TL = colourTL;
      colour32TL = (colour32TL | (colour32TL << 16)) & 0x07e0f81f;
      uint32_t colour32TR = colourTR;
      colour32TR = (colour32TR | (colour32TR << 16)) & 0x07e0f81f;
      uint8_t alphaLR = (x * 0xFF) / (r.right - r.left);
      colour32TL += (((colour32TR - colour32TL) * ((mGamma[alphaLR] + 4) >> 3)) >> 5) & 0x07e0f81f;

      uint32_t colour32BL = colourBL;
      colour32BL = (colour32BL | (colour32BL << 16)) & 0x07e0f81f;
      uint32_t colour32BR = colourBR;
      colour32BR = (colour32BR | (colour32BR << 16)) & 0x07e0f81f;
      colour32BL += (((colour32BR - colour32BL) * ((mGamma[alphaLR] + 4) >> 3)) >> 5) & 0x07e0f81f;

      uint8_t alphaTB = (y * 0xFF) / (r.bottom - r.top);
      colour32TL += (((colour32BL - colour32TL) * ((mGamma[alphaTB] + 4) >> 3)) >> 5) & 0x07e0f81f;
      colour32TL |= colour32TL >> 16;

      *dst++ = colour32TL;
      }
    }
  }
//}}}
//}}}
//{{{  draw
//{{{
void cLcd::rect (const uint16_t colour, const cRect& r) {
// rect with right,bottom clip

  int16_t xmax = min (r.right, (int16_t)mWidth);
  int16_t ymax = min (r.bottom, (int16_t)mHeight);

  for (int16_t y = r.top; y < ymax; y++) {
    uint16_t* ptr = mFrameBuf + y*mWidth + r.left;
    for (int16_t x = r.left; x < xmax; x++)
      *ptr++ = colour;
    }
  }
//}}}
//{{{
void cLcd::rect (const uint16_t colour, const uint8_t alpha, const cRect& r) {

  uint16_t xmax = min (r.right, (int16_t)mWidth);
  uint16_t ymax = min (r.bottom, (int16_t)mHeight);

  for (int16_t y = r.top; y < ymax; y++)
    for (int16_t x = r.left; x < xmax; x++)
      pix (colour, alpha, cPoint(x, y));
  }
//}}}
//{{{
void cLcd::rectOutline (const uint16_t colour, const cRect& r) {

  rect (colour, cRect (r.left, r.top, r.right, r.top+1));
  rect (colour, cRect (r.left, r.bottom-1, r.right, r.bottom));
  rect (colour, cRect (r.left, r.top, r.left+1, r.bottom));
  rect (colour, cRect (r.right-1, r.top, r.right, r.bottom));
  }
//}}}

//{{{
void cLcd::ellipse (const uint16_t colour, const uint8_t alpha, cPoint centre, cPoint radius) {

  if (!radius.x)
    return;
  if (!radius.y)
    return;

  int x1 = 0;
  int y1 = -radius.x;
  int err = 2 - 2*radius.x;
  float k = (float)radius.y / radius.x;

  do {
    rect (colour, alpha, cRect (centre.x-(uint16_t)(x1 / k), centre.y + y1,
                                centre.x-(uint16_t)(x1 / k) + 2*(uint16_t)(x1 / k) + 1, centre.y  + y1 + 1));
    rect (colour, alpha, cRect (centre.x-(uint16_t)(x1 / k), centre.y  - y1,
                                centre.x-(uint16_t)(x1 / k) + 2*(uint16_t)(x1 / k) + 1, centre.y  - y1 + 1));

    int e2 = err;
    if (e2 <= x1) {
      err += ++x1 * 2 + 1;
      if (-y1 == centre.x && e2 <= y1)
        e2 = 0;
      }
    if (e2 > y1)
      err += ++y1*2 + 1;
    } while (y1 <= 0);
  }
//}}}
//{{{
void cLcd::ellipseOutline (const uint16_t colour, cPoint centre, cPoint radius) {

  int x = 0;
  int y = -radius.y;

  int err = 2 - 2 * radius.x;
  float k = (float)radius.y / (float)radius.x;

  do {
    pix (colour, 0xFF, centre + cPoint (-(int16_t)(x / k), y));
    pix (colour, 0xFF, centre + cPoint ((int16_t)(x / k), y));
    pix (colour, 0xFF, centre + cPoint ((int16_t)(x / k), -y));
    pix (colour, 0xFF, centre + cPoint (- (int16_t)(x / k), - y));

    int e2 = err;
    if (e2 <= x) {
      err += ++x * 2 + 1;
      if (-y == x && e2 <= y)
        e2 = 0;
      }

    if (e2 > y)
      err += ++y *2 + 1;
    } while (y <= 0);

  }
//}}}

//{{{
void cLcd::line (const uint16_t colour, cPoint p1, cPoint p2) {

  int16_t deltax = abs(p2.x - p1.x); // The difference between the x's
  int16_t deltay = abs(p2.y - p1.y); // The difference between the y's

  cPoint p = p1;
  cPoint inc1 ((p2.x >= p1.x) ? 1 : -1, (p2.y >= p1.y) ? 1 : -1);
  cPoint inc2 = inc1;

  int16_t numAdd = (deltax >= deltay) ? deltay : deltax;
  int16_t den = (deltax >= deltay) ? deltax : deltay;
  if (deltax >= deltay) { // There is at least one x-value for every y-value
    inc1.x = 0;            // Don't change the x when numerator >= denominator
    inc2.y = 0;            // Don't change the y for every iteration
    }
  else {                  // There is at least one y-value for every x-value
    inc2.x = 0;            // Don't change the x for every iteration
    inc1.y = 0;            // Don't change the y when numerator >= denominator
    }

  int16_t num = den / 2;
  int16_t numPixels = den;
  for (int16_t pixel = 0; pixel <= numPixels; pixel++) {
    pix (colour, 0xFF, p);
    num += numAdd;     // Increase the numerator by the top of the fraction
    if (num >= den) {   // Check if numerator >= denominator
      num -= den;       // Calculate the new numerator value
      p += inc1;
      }
    p += inc2;
    }
  }
//}}}
//}}}
//{{{  drawAA
//{{{
void cLcd::moveToAA (const cPointF& p) {
  mDrawAA->moveTo (int(p.x * 256.f), int(p.y * 256.f));
  }
//}}}
//{{{
void cLcd::lineToAA (const cPointF& p) {
  mDrawAA->lineTo (int(p.x * 256.f), int(p.y * 256.f));
  }
//}}}
//{{{
void cLcd::renderAA (const uint16_t colour, bool fillNonZero) {
  mDrawAA->render (colour, fillNonZero, mFrameBuf, mWidth, mHeight);
  }
//}}}

// helpers
//{{{
void cLcd::wideLineAA (const cPointF& p1, const cPointF& p2, float width) {

  cPointF perp = (p2 - p1).perp() * width;
  moveToAA (p1 + perp);
  lineToAA (p2 + perp);
  lineToAA (p2 - perp);
  lineToAA (p1 - perp);
  }
//}}}
//{{{
void cLcd::pointedLineAA (const cPointF& p1, const cPointF& p2, float width) {

  cPointF perp = (p2 - p1).perp() * width;
  moveToAA (p1 + perp);
  lineToAA (p2);
  lineToAA (p1 - perp);
  }
//}}}

//{{{
void cLcd::ellipseAA (const cPointF& centre, const cPointF& radius, int steps) {

  // anticlockwise ellipse
  float angle = 0.f;
  float fstep = 360.f / steps;
  moveToAA (centre + cPointF(radius.x, 0.f));

  angle += fstep;
  while (angle < 360.f) {
    auto radians = angle * 3.1415926f / 180.0f;
    lineToAA (centre + cPointF (cos(radians) * radius.x, sin(radians) * radius.y));
    angle += fstep;
    }
  }
//}}}
//{{{
void cLcd::ellipseOutlineAA (const cPointF& centre, const cPointF& radius, float width, int steps) {

  float angle = 0.f;
  float fstep = 360.f / steps;
  moveToAA (centre + cPointF(radius.x, 0.f));

  // clockwise ellipse
  angle += fstep;
  while (angle < 360.f) {
    auto radians = angle * 3.1415926f / 180.0f;
    lineToAA (centre + cPointF (cos(radians) * radius.x, sin(radians) * radius.y));
    angle += fstep;
    }

  // anti clockwise ellipse
  moveToAA (centre + cPointF(radius.x - width, 0.f));

  angle -= fstep;
  while (angle > 0.f) {
    auto radians = angle * 3.1415926f / 180.0f;
    lineToAA (centre + cPointF (cos(radians) * (radius.x - width), sin(radians) * (radius.y - width)));
    angle -= fstep;
    }
  }
//}}}
//}}}
//{{{
int cLcd::text (const uint16_t colour, const cPoint& p, const int height, const string& str) {

  if (mTypeEnabled) {
    FT_Set_Pixel_Sizes (mFace, 0, height);

    int curX = p.x;
    for (unsigned i = 0; (i < str.size()) && (curX < mWidth); i++) {
      FT_Load_Char (mFace, str[i], FT_LOAD_RENDER);
      FT_GlyphSlot slot = mFace->glyph;

      int x = curX + slot->bitmap_left;
      int y = p.y + height - slot->bitmap_top;

      if (slot->bitmap.buffer) {
        for (unsigned bitmapY = 0; bitmapY < slot->bitmap.rows; bitmapY++) {
          auto bitmapPtr = slot->bitmap.buffer + (bitmapY * slot->bitmap.pitch);
          for (unsigned bitmapX = 0; bitmapX < slot->bitmap.width; bitmapX++)
            pix (colour, *bitmapPtr++, cPoint (x + bitmapX, y + bitmapY));
          }
        }
      curX += slot->advance.x / 64;
      }

    return curX;
    }

  cLog::log (LOGERROR, "type not enabled");

  return 0;
  }
//}}}

//{{{
void cLcd::delayUs (const int us) {
// delay in microSeconds

  gpioDelay (us);
  }
//}}}
//{{{
double cLcd::timeUs() {
// return time in double microSeconds

  return time_time();
  }
//}}}

// cLcd private
//{{{
string cLcd::getInfoString() {
// return info string for log display

  return dec(getUpdatePixels()) + "px took:" +
         dec(getUpdateUs()) + "uS diff took" +
         dec(getDiffUs()) + "uS";
  }
//}}}
//{{{
string cLcd::getPaddedInfoString() {
// return info string with padded format for on screen display

  return dec(getUpdatePixels(),5,'0') + " " +
         dec(getUpdateUs(),5,'0') + "uS diff:" +
         dec(getDiffUs(),3,'0') + "uS";
  }
//}}}

//{{{
void cLcd::setFont (const uint8_t* font, const int fontSize)  {

  FT_Init_FreeType (&mLibrary);
  FT_New_Memory_Face (mLibrary, (FT_Byte*)font, fontSize, 0, &mFace);
  }
//}}}

// parallel 16bit classes
//{{{  cLcdTa7601 : public cLcd
// 16 bit parallel, rs pin, no gram write HV swap, gram H start/end must be even
// - could use direction bits for 180 but not worth it
// - slow down writes, clocks ok but data not ready
constexpr int16_t kWidthTa7601 = 320;
constexpr int16_t kHeightTa7601 = 480;

cLcdTa7601::cLcdTa7601 (const cLcd::eRotate rotate, const cLcd::eInfo info, const cLcd::eMode mode)
  : cLcd(kWidthTa7601, kHeightTa7601, rotate, info, mode) {}

//{{{
bool cLcdTa7601::initialise() {

  if (!cLcd::initialise())
    return false;

  // wr
  gpioSetMode (k16WriteGpio, PI_OUTPUT);
  gpioWrite (k16WriteGpio, 1);

  // rd - unused
  gpioSetMode (k16ReadGpio, PI_OUTPUT);
  gpioWrite (k16ReadGpio, 1);

  // rs
  gpioSetMode (kRegisterGpio, PI_OUTPUT);
  gpioWrite (kRegisterGpio, 1);

  // chipSelect
  gpioSetMode (k16ChipSelectGpio, PI_OUTPUT);
  gpioWrite (k16ChipSelectGpio, 0);

  // backlight
  gpioSetMode (k16BacklightGpio, PI_OUTPUT);
  gpioWrite (k16BacklightGpio, 0);

  // 16 d0-d15
  for (int i = 0; i < 16; i++)
    gpioSetMode (i, PI_OUTPUT);
  gpioWrite_Bits_0_31_Clear (k16DataMask);

  // portrait mode (0,0) top left, top is side opposite the connector.
  writeCommandData (0x01, 0x023C); // gate_scan & display boundary
  writeCommandData (0x02, 0x0100); // inversion
  writeCommandData (0x03, 0x1030); // GRAM access - BGR - MDT normal - HV inc
  writeCommandData (0x08, 0x0808); // Porch period
  writeCommandData (0x0A, 0x0200); // oscControl, clock number per 1H - 0x0500 = 70Hz, 0x200 = 100Hz 0x0000 = 120Hz
  writeCommandData (0x0B, 0x0000); // interface & display clock
  writeCommandData (0x0C, 0x0770); // source and gate timing control
  writeCommandData (0x0D, 0x0000); // gate scan position
  writeCommandData (0x0E, 0x0001); // tearing effect prevention
  //{{{  power control
  writeCommandData (0x11, 0x0406); // power control
  writeCommandData (0x12, 0x000E); // power control
  writeCommandData (0x13, 0x0222); // power control
  writeCommandData (0x14, 0x0015); // power control
  writeCommandData (0x15, 0x4277); // power control
  writeCommandData (0x16, 0x0000); // power control
  //}}}
  //{{{  gamma
  writeCommandData (0x30, 0x6A50); // gamma
  writeCommandData (0x31, 0x00C9); // gamma
  writeCommandData (0x32, 0xC7BE); // gamma
  writeCommandData (0x33, 0x0003); // gamma
  writeCommandData (0x36, 0x3443); // gamma
  writeCommandData (0x3B, 0x0000); // gamma
  writeCommandData (0x3C, 0x0000); // gamma
  writeCommandData (0x2C, 0x6A50); // gamma
  writeCommandData (0x2D, 0x00C9); // gamma
  writeCommandData (0x2E, 0xC7BE); // gamma
  writeCommandData (0x2F, 0x0003); // gamma
  writeCommandData (0x35, 0x3443); // gamma
  writeCommandData (0x39, 0x0000); // gamma
  writeCommandData (0x3A, 0x0000); // gamma
  writeCommandData (0x28, 0x6A50); // gamma
  writeCommandData (0x29, 0x00C9); // gamma
  writeCommandData (0x2A, 0xC7BE); // gamma
  writeCommandData (0x2B, 0x0003); // gamma
  writeCommandData (0x34, 0x3443); // gamma
  writeCommandData (0x37, 0x0000); // gamma
  writeCommandData (0x38, 0x0000); // gamma
  delayUs (10000);
  //}}}
  //{{{  more power control
  writeCommandData (0x12, 0x200E);  // power control
  delayUs (10000);
  writeCommandData (0x12, 0x2003);  // power control
  delayUs (10000);
  //}}}

  writeCommandData (0x07, 0x0012);  // partial, 8-color, display ON
  delayUs (10000);
  writeCommandData (0x07, 0x0017);  // partial, 8-color, display ON
  delayUs (10000);

  updateLcd (mSpanAll);

  return true;
  }
//}}}
//{{{
void cLcdTa7601::setBacklight (bool on) {
  gpioWrite (k16BacklightGpio, on ? 1 : 0);
  }
//}}}

//{{{
void cLcdTa7601::writeCommand (const uint8_t command) {
// slow down write

  gpioWrite (kRegisterGpio, 0);

  gpioWrite_Bits_0_31_Clear (~command & k16WriteClrMask); // clear lo data bits + k16WrGpio bit lo
  gpioWrite_Bits_0_31_Clear (~command & k16WriteClrMask); // clear lo data bits + k16WrGpio bit lo
  gpioWrite_Bits_0_31_Clear (~command & k16WriteClrMask); // clear lo data bits + k16WrGpio bit lo

  gpioWrite_Bits_0_31_Set (command);                      // set hi data bits
  gpioWrite_Bits_0_31_Set (command);                      // set hi data bits
  gpioWrite_Bits_0_31_Set (command);                      // set hi data bits

  gpioWrite_Bits_0_31_Set (k16WriteMask);                 // write on k16WrGpio rising edge
  gpioWrite_Bits_0_31_Set (k16WriteMask);                 // write on k16WrGpio rising edge

  gpioWrite (kRegisterGpio, 1);
  }
//}}}
//{{{
void cLcdTa7601::writeDataWord (const uint16_t data) {
// slow down write

  fastGpioWrite_Bits_0_31_Clear (~data & k16WriteClrMask); // clear lo data bits + k16WrGpio bit lo
  fastGpioWrite_Bits_0_31_Clear (~data & k16WriteClrMask); // clear lo data bits + k16WrGpio bit lo
  fastGpioWrite_Bits_0_31_Clear (~data & k16WriteClrMask); // clear lo data bits + k16WrGpio bit lo

  fastGpioWrite_Bits_0_31_Set (data);                      // set hi data bits
  fastGpioWrite_Bits_0_31_Set (data);                      // set hi data bits
  fastGpioWrite_Bits_0_31_Set (data);                      // set hi data bits

  fastGpioWrite_Bits_0_31_Set (k16WriteMask);              // write on k16WrGpio rising edge
  fastGpioWrite_Bits_0_31_Set (k16WriteMask);              // write on k16WrGpio rising edge
  }
//}}}

//{{{
uint32_t cLcdTa7601::updateLcd (sSpan* spans) {

  uint32_t numPixels = 0;

  switch (mRotate) {
    //{{{
    case e0: {
      for (sSpan* it = spans; it; it = it->next) {
        // ensure GRAM even H start, end addressses
        cRect r = it->r;
        r.left &= 0xFFFE;
        r.right = (r.right + 1) & 0xFFFE;

        writeCommandData (0x45, r.left);     // GRAM V start address window
        writeCommandData (0x44, r.right-1);  // GRAM V   end address window
        writeCommandData (0x47, r.top);      // GRAM H start address window - even
        writeCommandData (0x46, r.bottom-1); // GRAM H   end address window - even

        writeCommandData (0x20, r.top);      // GRAM V start address
        writeCommandData (0x21, r.left);     // GRAM H start address
        writeCommand (0x22);                 // GRAM write

        uint16_t* ptr = mFrameBuf + (r.top * kWidthTa7601) + r.left;
        for (int16_t y = r.top; y < r.bottom; y++) {
          for (int16_t x = r.left; x < r.right; x++)
            writeDataWord (*ptr++);
          ptr += kWidthTa7601 - r.getWidth();
          }

        numPixels += r.getNumPixels();
        }

      break;
      }
    //}}}
    //{{{
    case e90: {
      for (sSpan* it = spans; it; it = it->next) {
        // ensure GRAM even H start, end addressses
        cRect r = it->r;
        r.top &= 0xFFFE;
        r.bottom = (r.bottom + 1) & 0xFFFE;

        writeCommandData (0x45, r.top);                    // GRAM H start address window - even
        writeCommandData (0x44, r.bottom-1);               // GRAM H   end address window - even
        writeCommandData (0x47, kHeightTa7601 - r.right);  // GRAM V start address window
        writeCommandData (0x46, kHeightTa7601-1 - r.left); // GRAM V   end address window

        writeCommandData (0x20, kHeightTa7601 - r.right);  // GRAM V start address
        writeCommandData (0x21, r.top);                    // GRAM H start address
        writeCommand (0x22);                               // GRAM write

        for (int16_t x = r.right-1; x >= r.left; x--) {
          uint16_t* ptr = mFrameBuf + (r.top * kHeightTa7601) + x;
          for (int16_t y = r.top; y < r.bottom; y++) {
            writeDataWord (*ptr);
            ptr += kHeightTa7601;
            }
          }

        numPixels += r.getNumPixels();
        }

      break;
      }
    //}}}
    //{{{
    case e180: {
      for (sSpan* it = spans; it; it = it->next) {
        // ensure GRAM even H start, end addressses
        cRect r = it->r;
        r.left &= 0xFFFE;
        r.right = (r.right + 1) & 0xFFFE;

        writeCommandData (0x45, kWidthTa7601 - r.right);   // GRAM V start address window
        writeCommandData (0x44, kWidthTa7601-1 - r.left);  // GRAM V   end address window
        writeCommandData (0x47, kHeightTa7601 - r.bottom); // GRAM H start address window - even
        writeCommandData (0x46, kHeightTa7601-1- r.top);   // GRAM H   end address window - even

        writeCommandData (0x20, kHeightTa7601 - r.bottom); // GRAM V start address
        writeCommandData (0x21, kWidthTa7601 - r.right);   // GRAM H start address
        writeCommand (0x22);                               // GRAM write

        uint16_t* ptr = mFrameBuf + ((r.bottom-1) * kWidthTa7601) + r.right-1;
        for (int16_t y = r.bottom-1; y >= r.top; y--) {
          for (int16_t x = r.right-1; x >= r.left; x--)
            writeDataWord (*ptr--);
          ptr -= kWidthTa7601 - r.getWidth();
          }

        numPixels += r.getNumPixels();
        }

      break;
      }
    //}}}
    //{{{
    case e270: {
      for (sSpan* it = spans; it; it = it->next) {
        // ensure GRAM even H start, end addressses
        cRect r = it->r;
        r.top &= 0xFFFE;
        r.bottom = (r.bottom + 1) & 0xFFFE;

        writeCommandData (0x45, kWidthTa7601 - r.bottom); // GRAM H start address window - even
        writeCommandData (0x44, kWidthTa7601-1 - r.top);  // GRAM H   end address window - even
        writeCommandData (0x47, r.left);                  // GRAM V start address window
        writeCommandData (0x46, r.right-1);               // GRAM V   end address window

        writeCommandData (0x20, r.left);                  // GRAM V start address
        writeCommandData (0x21, kWidthTa7601 - r.bottom); // GRAM H start address
        writeCommand (0x22);                              // GRAM write

        for (int16_t x = r.left; x < r.right; x++) {
          uint16_t* ptr = mFrameBuf + ((r.bottom-1) * kHeightTa7601) + x;
          for (int16_t y = r.bottom-1; y >= r.top; y--) {
            writeDataWord (*ptr);
            ptr -= kHeightTa7601;
            }
          }

        numPixels += r.getNumPixels();
        }

      break;
      }
    //}}}
    }

  return numPixels;
  }
//}}}
//}}}
//{{{  cLcdSsd1289 : public cLcd
constexpr int16_t kWidth1289 = 240;
constexpr int16_t kHeight1289 = 320;

cLcdSsd1289::cLcdSsd1289 (const cLcd::eRotate rotate, const cLcd::eInfo info, const cLcd::eMode mode)
  : cLcd(kWidth1289, kHeight1289, rotate, info, mode) {}

//{{{
bool cLcdSsd1289::initialise() {

  if (!cLcd::initialise())
    return false;

  // wr
  gpioSetMode (k16WriteGpio, PI_OUTPUT);
  gpioWrite (k16WriteGpio, 1);

  // rd - unused
  gpioSetMode (k16ReadGpio, PI_OUTPUT);
  gpioWrite (k16ReadGpio, 1);

  // rs
  gpioSetMode (kRegisterGpio, PI_OUTPUT);
  gpioWrite (kRegisterGpio, 1);

  // 16 d0-d15
  for (int i = 0; i < 16; i++)
    gpioSetMode (i, PI_OUTPUT);
  gpioWrite_Bits_0_31_Clear (k16DataMask);

  // startup commands
  writeCommandData (0x00, 0x0001); // SSD1289_REG_OSCILLATION
  //{{{  power control
  writeCommandData (0x03, 0xA8A4); // SSD1289_REG_POWER_CTRL_1
  writeCommandData (0x0c, 0x0000); // SSD1289_REG_POWER_CTRL_2
  writeCommandData (0x0d, 0x080C); // SSD1289_REG_POWER_CTRL_3
  writeCommandData (0x0e, 0x2B00); // SSD1289_REG_POWER_CTRL_4
  writeCommandData (0x1e, 0x00B7); // SSD1289_REG_POWER_CTRL_5
  //}}}

  //write_reg(0x01, (1 << 13) | (par->bgr << 11) | (1 << 9) | (HEIGHT - 1));
  writeCommandData (0x01, 0x2B3F); // SSD1289_REG_DRIVER_OUT_CTRL
  writeCommandData (0x02, 0x0600); // SSD1289_REG_LCD_DRIVE_AC
  writeCommandData (0x10, 0x0000); // SSD1289_REG_SLEEP_MODE

  writeCommandData (0x07, 0x0233); // SSD1289_REG_DISPLAY_CTRL
  writeCommandData (0x0b, 0x0000); // SSD1289_REG_FRAME_CYCLE
  writeCommandData (0x0f, 0x0000); // SSD1289_REG_GATE_SCAN_START

  writeCommandData (0x23, 0x0000); // SSD1289_REG_WR_DATA_MASK_1
  writeCommandData (0x24, 0x0000); // SSD1289_REG_WR_DATA_MASK_2
  writeCommandData (0x25, 0x8000); // SSD1289_REG_FRAME_FREQUENCY

  //{{{  gamma
  writeCommandData (0x30, 0x0707); // SSD1289_REG_GAMMA_CTRL_1
  writeCommandData (0x31, 0x0204); // SSD1289_REG_GAMMA_CTRL_2
  writeCommandData (0x32, 0x0204); // SSD1289_REG_GAMMA_CTRL_3
  writeCommandData (0x33, 0x0502); // SSD1289_REG_GAMMA_CTRL_4
  writeCommandData (0x34, 0x0507); // SSD1289_REG_GAMMA_CTRL_5
  writeCommandData (0x35, 0x0204); // SSD1289_REG_GAMMA_CTRL_6
  writeCommandData (0x36, 0x0204); // SSD1289_REG_GAMMA_CTRL_7
  writeCommandData (0x37, 0x0502); // SSD1289_REG_GAMMA_CTRL_8
  writeCommandData (0x3a, 0x0302); // SSD1289_REG_GAMMA_CTRL_9
  writeCommandData (0x3b, 0x0302); // SSD1289_REG_GAMMA_CTRL_10
  //}}}

  writeCommandData (0x41, 0x0000); // SSD1289_REG_V_SCROLL_CTRL_1
  //write_reg(0x42, 0x0000);
  writeCommandData (0x48, 0x0000); // SSD1289_REG_FIRST_WIN_START
  writeCommandData (0x49, 0x013F); // SSD1289_REG_FIRST_WIN_END

  writeCommandData (0x44, ((kWidth1289-1) << 8) | 0); // SSD1289_REG_H_RAM_ADR_POS
  writeCommandData (0x45, 0x0000);                    // SSD1289_REG_V_RAM_ADR_START
  writeCommandData (0x46, kHeight1289-1);             // SSD1289_REG_V_RAM_ADR_END
  writeCommandData (0x47, 0x0000);                    // SSD1289_REG_H_RAM_ADR_START

  int xstart = 0;
  int ystart = 0;
  int xres = kWidth1289-1;
  int yres = kHeight1289-1;
  switch (mRotate) {
    case e0:
      writeCommandData (0x11, 0x6040 | 0b110000); // 0x11 REG_ENTRY_MODE
      writeCommandData (0x4e, xstart);            // 0x4E GDDRAM X address counter
      writeCommandData (0x4f, ystart);            // 0x4F GDDRAM Y address counter
      break;

    case e90:
      writeCommandData (0x11, 0x6040 | 0b011000); // 0x11 REG_ENTRY_MODE
      writeCommandData (0x4e, ystart);            // 0x4E GDDRAM X address counter
      writeCommandData (0x4f, xres - xstart);     // 0x4F GDDRAM Y address counter
      break;

    case e180:
      writeCommandData (0x11, 0x6040 | 0b000000); // 0x11 REG_ENTRY_MODE
      writeCommandData (0x4e, xres - xstart);     // 0x4E GDDRAM X address counter
      writeCommandData (0x4f, yres - ystart);     // 0x4F GDDRAM Y address counter
      break;

    case e270:
      writeCommandData (0x11, 0x6040 | 0b101000); // 0x11 REG_ENTRY_MODE
      writeCommandData (0x4e, yres - ystart);     // 0x4E GDDRAM X address counter
      writeCommandData (0x4f, xstart);            // 0x4F GDDRAM Y address counter
      break;
    }

  updateLcd (mSpanAll);

  return true;
  }
//}}}

//{{{
void cLcdSsd1289::writeCommand (const uint8_t command) {

  gpioWrite (kRegisterGpio, 0);

  gpioWrite_Bits_0_31_Clear (~command & k16WriteClrMask); // clear lo data bits + k16WrGpio bit lo
  gpioWrite_Bits_0_31_Set (command);                      // set hi data bits
  gpioWrite_Bits_0_31_Set (k16WriteMask);                 // write on k16WrGpio rising edge

  gpioWrite (kRegisterGpio, 1);
  }
//}}}
//{{{
void cLcdSsd1289::writeDataWord (const uint16_t data) {

  fastGpioWrite_Bits_0_31_Clear (~data & k16WriteClrMask); // clear lo data bits + k16WrGpio bit lo
  fastGpioWrite_Bits_0_31_Set (data);                      // set hi data bits
  fastGpioWrite_Bits_0_31_Set (k16WriteMask);              // write on k16WrGpio rising edge
  }
//}}}
//{{{
void cLcdSsd1289::writeCommandMultiData (const uint8_t command, const uint8_t* dataPtr, const int len) {

  writeCommand (command);

  // send data
  uint16_t* ptr = (uint16_t*)dataPtr;
  uint16_t* ptrEnd = (uint16_t*)dataPtr + len/2;
  while (ptr < ptrEnd)
    writeDataWord (*ptr++);
  }
//}}}

//{{{
uint32_t cLcdSsd1289::updateLcd (sSpan* spans) {

  writeCommandMultiData (0x22, (const uint8_t*)mFrameBuf, getNumPixels() * 2);
  return getNumPixels();
  }
//}}}
//}}}

// spi data/command pin classes
//{{{  cLcdSpiRegister : public cLcdSpi
//{{{
cLcdSpiRegister::~cLcdSpiRegister() {
  spiClose (mSpiHandle);
  }
//}}}

//{{{
void cLcdSpiRegister::writeCommand (const uint8_t command) {

  gpioWrite (kRegisterGpio, 0);
  spiWrite (mSpiHandle, (char*)(&command), 1);
  gpioWrite (kRegisterGpio, 1);
  }
//}}}
//{{{
void cLcdSpiRegister::writeDataWord (const uint16_t data) {
// send data

  uint16_t swappedData = bswap_16 (data);
  spiWrite (mSpiHandle, (char*)(&swappedData), 2);
  }
//}}}

//{{{
void cLcdSpiRegister::writeCommandMultiData (const uint8_t command, const uint8_t* dataPtr, const int len) {

  writeCommand (command);

  // send data
  int bytesLeft = len;
  auto ptr = (char*)dataPtr;
  while (bytesLeft > 0) {
   int sendBytes = (bytesLeft > 0xFFFF) ? 0xFFFF : bytesLeft;
    spiWrite (mSpiHandle, ptr, sendBytes);
    ptr += sendBytes;
    bytesLeft -= sendBytes;
    }
  }
//}}}
//}}}
//{{{  cLcdSt7735r : public cLcdSpiRegister
constexpr int16_t kWidth7735 = 128;
constexpr int16_t kHeight7735 = 160;
constexpr int kSpiClock7735 = 24000000;

cLcdSt7735r::cLcdSt7735r (const cLcd::eRotate rotate, const cLcd::eInfo info, const eMode mode)
  : cLcdSpiRegister (kWidth7735, kHeight7735, rotate, info, mode) {}

//{{{
bool cLcdSt7735r::initialise() {

  if (!cLcd::initialise())
    return false;

  // rs
  gpioSetMode (kRegisterGpio, PI_OUTPUT);
  gpioWrite (kRegisterGpio, 1);

  // mode 0, spi manages ce0 active lo
  mSpiHandle = spiOpen (0, kSpiClock7735, 0);

  //{{{  command constexpr
  constexpr uint8_t k7335_SLPOUT  = 0x11; // no data
  constexpr uint8_t k7335_DISPON  = 0x29; // no data
  //constexpr uint8_t k7335_DISPOFF = 0x28; // no data

  constexpr uint8_t k7335_CASET = 0x2A;
  constexpr uint8_t k7335_caSetData[4] = { 0, 0, 0, kWidth7735 - 1 };

  constexpr uint8_t k7335_RASET = 0x2B;
  constexpr uint8_t k7335_raSetData[4] = { 0, 0, 0, kHeight7735 - 1 };

  constexpr uint8_t k7335_MADCTL = 0x36;
  constexpr uint8_t k7735_MADCTLData[1] = { 0xc0 };

  constexpr uint8_t k7335_COLMOD  = 0x3A;
  constexpr uint8_t k7335_COLMODData[1] = { 0x05 };

  constexpr uint8_t k7335_FRMCTR1 = 0xB1;
  constexpr uint8_t k7335_FRMCTR2 = 0xB2;
  constexpr uint8_t k7335_FRMCTR3 = 0xB3;
  constexpr uint8_t k7335_FRMCTRData[6] = { 0x01, 0x2c, 0x2d, 0x01, 0x2c, 0x2d };

  constexpr uint8_t k7335_INVCTR  = 0xB4;
  constexpr uint8_t k7335_INVCTRData[1] = { 0x07 };

  constexpr uint8_t k7335_PWCTR1  = 0xC0;
  constexpr uint8_t k7335_PowerControlData1[3] = { 0xA2, 0x02 /* -4.6V */, 0x84 /* AUTO mode */ };
  constexpr uint8_t k7335_PWCTR2  = 0xC1;
  constexpr uint8_t k7335_PowerControlData2[1] = { 0xc5 }; // VGH25 = 2.4C VGSEL =-10 VGH = 3*AVDD
  constexpr uint8_t k7335_PWCTR3  = 0xC2;
  constexpr uint8_t k7335_PowerControlData3[2] = { 0x0A /* Opamp current small */, 0x00 /* Boost freq */ };
  constexpr uint8_t k7335_PWCTR4  = 0xC3;
  constexpr uint8_t k7335_PowerControlData4[2] = { 0x8A /* BCLK/2, Opamp current small/medium low */, 0x2A };
  constexpr uint8_t k7335_PWCTR5  = 0xC4;
  constexpr uint8_t k7335_PowerControlData5[2] = { 0x8A /* BCLK/2, Opamp current small/medium low */, 0xEE };

  constexpr uint8_t k7335_VMCTR1  = 0xC5;
  constexpr uint8_t k7335_VMCTR1Data[1] = { 0x0E };

  constexpr uint8_t k7335_GMCTRP1 = 0xE0;
  constexpr uint8_t k7335_GMCTRP1Data[16] = { 0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d,
                                              0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10 } ;

  constexpr uint8_t k7335_GMCTRN1 = 0xE1;
  constexpr uint8_t k7335_GMCTRN1Data[16] = { 0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
                                              0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10 } ;
  //}}}
  writeCommand (k7335_SLPOUT);
  delayUs (120000);

  writeCommandMultiData (k7335_FRMCTR1, k7335_FRMCTRData, 3); // frameRate normal mode
  writeCommandMultiData (k7335_FRMCTR2, k7335_FRMCTRData, 3); // frameRate idle mode
  writeCommandMultiData (k7335_FRMCTR3, k7335_FRMCTRData, 6); // frameRate partial mode
  writeCommandMultiData (k7335_INVCTR, k7335_INVCTRData, sizeof(k7335_INVCTRData)); // Inverted mode off

  writeCommandMultiData (k7335_PWCTR1, k7335_PowerControlData1, sizeof(k7335_PowerControlData1)); // POWER CONTROL 1
  writeCommandMultiData (k7335_PWCTR2, k7335_PowerControlData2, sizeof(k7335_PowerControlData2)); // POWER CONTROL 2
  writeCommandMultiData (k7335_PWCTR3, k7335_PowerControlData3, sizeof(k7335_PowerControlData3)); // POWER CONTROL 3
  writeCommandMultiData (k7335_PWCTR4, k7335_PowerControlData4, sizeof(k7335_PowerControlData4)); // POWER CONTROL 4
  writeCommandMultiData (k7335_PWCTR5, k7335_PowerControlData5, sizeof(k7335_PowerControlData5)); // POWER CONTROL 5

  writeCommandMultiData (k7335_VMCTR1, k7335_VMCTR1Data, sizeof(k7335_VMCTR1Data)); // POWER CONTROL 6
  writeCommandMultiData (k7335_MADCTL, k7735_MADCTLData, sizeof(k7735_MADCTLData)); // ORIENTATION
  writeCommandMultiData (k7335_COLMOD, k7335_COLMODData, sizeof(k7335_COLMODData)); // COLOR MODE - 16bit per pixel

  writeCommandMultiData (k7335_GMCTRP1, k7335_GMCTRP1Data, sizeof(k7335_GMCTRP1Data)); // gamma GMCTRP1
  writeCommandMultiData (k7335_GMCTRN1, k7335_GMCTRN1Data, sizeof(k7335_GMCTRN1Data)); // Gamma GMCTRN1

  writeCommandMultiData (k7335_CASET, k7335_caSetData, sizeof(k7335_caSetData));
  writeCommandMultiData (k7335_RASET, k7335_raSetData, sizeof(k7335_raSetData));

  writeCommand (k7335_DISPON); // display ON

  updateLcd (mSpanAll);

  return true;
  }
//}}}

//{{{
uint32_t cLcdSt7735r::updateLcd (sSpan* spans) {
// ignore spans, just send everything for now

  uint16_t swappedFrameBuf [kWidth7735 * kHeight7735];

  uint16_t* src = mFrameBuf;
  uint16_t* dst = swappedFrameBuf;
  for (uint32_t i = 0; i < getNumPixels(); i++)
    *dst++ = bswap_16 (*src++);

  writeCommandMultiData (0x2C, (const uint8_t*)swappedFrameBuf, getNumPixels() * 2); // RAMRW command
  return getNumPixels();
  }
//}}}
//}}}
//{{{  cLcdIli9225b : public cLcdSpiRegister
constexpr int16_t kWidth9225b = 176;
constexpr int16_t kHeight9225b = 220;
constexpr int kSpiClock9225b = 16000000;

cLcdIli9225b::cLcdIli9225b (const cLcd::eRotate rotate, const cLcd::eInfo info, const eMode mode)
  : cLcdSpiRegister(kWidth9225b, kHeight9225b, rotate, info, mode) {}

//{{{
bool cLcdIli9225b::initialise() {

  if (!cLcd::initialise())
    return false;

  // rs
  gpioSetMode (kRegisterGpio, PI_OUTPUT);
  gpioWrite (kRegisterGpio, 1);

  // mode 0, spi manages ce0 active lo
  mSpiHandle = spiOpen (0, kSpiClock9225b, 0);

  writeCommandData (0x01, 0x011C); // set SS and NL bit

  writeCommandData (0x02, 0x0100); // set 1 line inversion
  writeCommandData (0x03, 0x1030); // set GRAM write direction and BGR=1
  writeCommandData (0x08, 0x0808); // set BP and FP
  writeCommandData (0x0C, 0x0000); // RGB interface setting R0Ch=0x0110 for RGB 18Bit and R0Ch=0111for RGB16
  writeCommandData (0x0F, 0x0b01); // Set frame rate//0b01

  writeCommandData (0x20, 0x0000); // Set GRAM Address
  writeCommandData (0x21, 0x0000); // Set GRAM Address
  delayUs (50000);

  //{{{  power On sequence
  writeCommandData (0x10,0x0a00); // Set SAP,DSTB,STB//0800
  writeCommandData (0x11,0x1038); // Set APON,PON,AON,VCI1EN,VC
  delayUs (50000);
  //}}}
  writeCommandData (0x12, 0x1121); // Internal reference voltage= Vci;
  writeCommandData (0x13, 0x0063); // Set GVDD
  writeCommandData (0x14, 0x4b44); // Set VCOMH/VCOML voltage//3944

  //{{{  set GRAM area
  writeCommandData (0x30,0x0000);
  writeCommandData (0x31,0x00DB);
  writeCommandData (0x32,0x0000);
  writeCommandData (0x33,0x0000);
  writeCommandData (0x34,0x00DB);
  writeCommandData (0x35,0x0000);
  writeCommandData (0x36,0x00AF);
  writeCommandData (0x37,0x0000);
  writeCommandData (0x38,0x00DB);
  writeCommandData (0x39,0x0000);
  //}}}
  //{{{  set Gamma Curve
  writeCommandData (0x50,0x0003);
  writeCommandData (0x51,0x0900);
  writeCommandData (0x52,0x0d05);
  writeCommandData (0x53,0x0900);
  writeCommandData (0x54,0x0407);
  writeCommandData (0x55,0x0502);
  writeCommandData (0x56,0x0000);
  writeCommandData (0x57,0x0005);
  writeCommandData (0x58,0x1700);
  writeCommandData (0x59,0x001F);
  delayUs (50000);
  //}}}

  writeCommandData (0x07, 0x1017);
  //{{{  set ram area
  writeCommandData (0x36, mWidth-1);
  writeCommandData (0x37, 0);
  writeCommandData (0x38, mHeight-1);
  writeCommandData (0x39, 0);
  writeCommandData (0x20, 0);
  writeCommandData (0x21, 0);
  //}}}

  updateLcd (mSpanAll);

  return true;
  }
//}}}

//{{{
uint32_t cLcdIli9225b::updateLcd (sSpan* spans) {
// ignore spans, just send everything for now

  uint16_t swappedFrameBuf [kWidth9225b * kHeight9225b];

  uint16_t* src = mFrameBuf;
  uint16_t* dst = swappedFrameBuf;
  for (uint32_t i = 0; i < getNumPixels(); i++)
    *dst++ = bswap_16 (*src++);

  writeCommandMultiData (0x22, (const uint8_t*)swappedFrameBuf, getNumPixels() * 2); // RAMRW command
  return getNumPixels();
  }
//}}}
//}}}

// spi no data/command pin classes
//{{{  cLcdSpiHeader : public cLcdSpi
//{{{
cLcdSpiHeader::~cLcdSpiHeader() {
  spiClose (mSpiHandle);
  }
//}}}

//{{{
void cLcdSpiHeader::writeCommand (const uint8_t command) {

  // send command
  const uint8_t kCommand[3] = { 0x70, 0, command };
  spiWrite (mSpiHandle, (char*)kCommand, 3);
  }
//}}}
//{{{
void cLcdSpiHeader::writeDataWord (const uint16_t data) {
// send data

  const uint8_t kData[3] = { 0x72, uint8_t(data >> 8), uint8_t(data & 0xFF) };
  spiWrite (mSpiHandle, (char*)kData, 3);
  }
//}}}
//}}}
//{{{  cLcdIli9320 : public cLcdSpiHeader
// 2.8 inch 240x320 - HY28A - touchcreen XT2046P
// spi - no rs - use headers, backlight
constexpr int16_t kWidth9320 = 240;
constexpr int16_t kHeight9320 = 320;
constexpr int kSpiClock9320 = 24000000;

cLcdIli9320::cLcdIli9320 (const cLcd::eRotate rotate, const cLcd::eInfo info, const eMode mode)
  : cLcdSpiHeader(kWidth9320, kHeight9320, rotate, info, mode) {}

//{{{
bool cLcdIli9320::initialise() {

  if (!cLcd::initialise())
    return false;

  // backlight off - active hi
  gpioSetMode (kSpiBacklightGpio, PI_OUTPUT);
  gpioWrite (kSpiBacklightGpio, 0);

  // spi mode 3, spi manages ce0 active lo
  mSpiHandle = spiOpen (0, kSpiClock9320, 3);

  writeCommandData (0xE5, 0x8000); // Set the Vcore voltage
  writeCommandData (0x00, 0x0000); // start oscillation - stopped?
  writeCommandData (0x01, 0x0100); // Driver Output Control 1 - SS=1 and SM=0
  writeCommandData (0x02, 0x0700); // LCD Driving Control - set line inversion
  writeCommandData (0x04, 0x0000); // Resize Control
  writeCommandData (0x08, 0x0202); // Display Control 2
  writeCommandData (0x09, 0x0000); // Display Control 3
  writeCommandData (0x0a, 0x0000); // Display Control 4 - frame marker
  writeCommandData (0x0c, 0x0001); // RGB Display Interface Control 1
  writeCommandData (0x0d, 0x0000); // Frame Marker Position
  writeCommandData (0x0f, 0x0000); // RGB Display Interface Control 2
  delayUs (40000);

  writeCommandData (0x07, 0x0101); // Display Control 1
  delayUs (40000);

  //{{{  power control
  writeCommandData (0x10, 0x10C0); // Power Control 1
  writeCommandData (0x11, 0x0007); // Power Control 2
  writeCommandData (0x12, 0x0110); // Power Control 3
  writeCommandData (0x13, 0x0b00); // Power Control 4
  writeCommandData (0x29, 0x0000); // Power Control 7
  //}}}
  writeCommandData (0x2b, 0x4010); // Frame Rate and Color Control
  writeCommandData (0x60, 0x2700); // Driver Output Control 2
  writeCommandData (0x61, 0x0001); // Base Image Display Control
  writeCommandData (0x6a, 0x0000); // Vertical Scroll Control
  //{{{  partial image
  writeCommandData (0x80, 0x0000); // Partial Image 1 Display Position
  writeCommandData (0x81, 0x0000); // Partial Image 1 Area Start Line
  writeCommandData (0x82, 0x0000); // Partial Image 1 Area End Line
  writeCommandData (0x83, 0x0000); // Partial Image 2 Display Position
  writeCommandData (0x84, 0x0000); // Partial Image 2 Area Start Line
  writeCommandData (0x85, 0x0000); // Partial Image 2 Area End Line
  //}}}
  //{{{  panel interface
  writeCommandData (0x90, 0x0010); // Panel Interface Control 1
  writeCommandData (0x92, 0x0000); // Panel Interface Control 2
  writeCommandData (0x93, 0x0001); // Panel Interface Control 3
  writeCommandData (0x95, 0x0110); // Panel Interface Control 4
  writeCommandData (0x97, 0x0000); // Panel Interface Control 5
  writeCommandData (0x98, 0x0000); // Panel Interface Control 6
  //}}}

  writeCommandData (0x07, 0x0133); // Display Control 1
  delayUs (40000);

  switch (mRotate) {
    case e0:
      writeCommandData (0x03, 0x1030); // Entry Mode - BGR, HV inc - AM H
      break;
    case e90:
      writeCommandData (0x03, 0x1018); // Entry Mode - BGR, H inc V dec - AM V
      break;
    case e180:
      writeCommandData (0x03, 0x1000); // Entry Mode - BGR, HV dec - AM H
      break;
    case e270:
      writeCommandData (0x03, 0x1028); // Entry Mode - BGR, H dec V inc - AM V
      break;
    }

  updateLcd (mSpanAll);

  return true;
  }
//}}}
//{{{
void cLcdIli9320::setBacklight (bool on) {
  gpioWrite (kSpiBacklightGpio, on ? 1 : 0);
  }
//}}}

//{{{
uint32_t cLcdIli9320::updateLcd (sSpan* spans) {

  uint16_t dataHeaderBuf [320+1];
  dataHeaderBuf[0] = 0x7272;

  int numPixels = 0;

  sSpan* it = spans;
  while (it) {
    switch (mRotate) {
      //{{{
      case e0:
        writeCommandData (0x50, it->r.left);     // H GRAM start address
        writeCommandData (0x51, it->r.right-1);  // H GRAM end address

        writeCommandData (0x52, it->r.top);      // V GRAM start address
        writeCommandData (0x53, it->r.bottom-1); // V GRAM end address

        writeCommandData (0x20, it->r.left);     // H GRAM start
        writeCommandData (0x21, it->r.top);      // V GRAM start

        break;
      //}}}
      //{{{
      case e90:
        writeCommandData (0x50, it->r.top);     // H GRAM start address
        writeCommandData (0x51, it->r.bottom-1);  // H GRAM end address

        writeCommandData (0x52, kHeight9320 - it->r.right);      // V GRAM start address
        writeCommandData (0x53, kHeight9320-1 - it->r.left); // V GRAM end address

        writeCommandData (0x20, it->r.top);     // H GRAM start
        writeCommandData (0x21, kHeight9320 - 1 - it->r.left);      // V GRAM start

        break;
      //}}}
      //{{{
      case e180:
        writeCommandData (0x50, kWidth9320 - it->r.right);    // H GRAM start address
        writeCommandData (0x51, kWidth9320 - 1 - it->r.left); // H GRAM end address

        writeCommandData (0x52, kHeight9320- it->r.bottom);   // V GRAM start address
        writeCommandData (0x53, kHeight9320-1 - it->r.top);   // V GRAM end address

        writeCommandData (0x20, kWidth9320-1 - it->r.left);   // H GRAM start
        writeCommandData (0x21, kHeight9320-1 - it->r.top);   // V GRAM start

        break;
      //}}}
      //{{{
      case e270:
        writeCommandData (0x50, kWidth9320 - it->r.bottom);     // H GRAM start address
        writeCommandData (0x51, kWidth9320-1 - it->r.top);  // H GRAM end address

        writeCommandData (0x52, it->r.left);      // V GRAM start address
        writeCommandData (0x53, it->r.right-1); // V GRAM end address

        writeCommandData (0x20, kWidth9320-1 - it->r.top);     // H GRAM start
        writeCommandData (0x21, it->r.left);      // V GRAM start

        break;
      //}}}
      }

    writeCommand (0x22);  // GRAM write
    uint16_t* src = mFrameBuf + (it->r.top * getWidth()) + it->r.left;
    for (int y = it->r.top; y < it->r.bottom; y++) {
      // 2 header bytes, alignment for data bswap_16, send spi data from second header byte
      uint16_t* dst = dataHeaderBuf + 1;
      for (int x = it->r.left; x < it->r.right; x++)
        *dst++ = bswap_16 (*src++);
      spiWrite (mSpiHandle, ((char*)(dataHeaderBuf))+1, (it->r.getWidth() * 2) + 1);
      src += getWidth() - it->r.getWidth();
      }

    numPixels += it->r.getNumPixels();
    it = it->next;
    }

  return numPixels;
  }
//}}}
//}}}
