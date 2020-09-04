// cLcd.cpp
//{{{  includes
#include "cLcd.h"

#include <cstdint>
#include <string>
#include <thread>
#include <byteswap.h>

#include "pigpio/pigpio.h"

#include "../shared/utils/utils.h"
#include "../shared/utils/cLog.h"
#include "fonts/FreeSansBold.h"

using namespace std;
//}}}
//{{{  include freetype static library
#include <ft2build.h>
#include FT_FREETYPE_H

static FT_Library mLibrary;
static FT_Face mFace;
//}}}

constexpr int kSpanMergeThreshold8 = 8;
constexpr int kSpanMergeThreshold16 = 16;
//{{{
struct sSpan {
  cRect r;

  uint16_t lastScanRight; // scanline bottom-1 can be partial, ends in lastScanRight.
  uint32_t size;

  sSpan* next;   // linked skip list in array for fast pruning
  };
//}}}

// public
constexpr uint8_t kResetGpio = 25;
//{{{
cLcd::~cLcd() {

  gpioTerminate();

  vc_dispmanx_resource_delete (mSnapshot);
  vc_dispmanx_display_close (mDisplay);

  free (mFrameBuf);
  free (mPrevFrameBuf);
  }
//}}}

//{{{
const string cLcd::getInfoString() {
// return info string for log display

  return dec(getUpdatePixels()) + "pix took:" +
         dec(getUpdateUs()) + "uS " +
         dec(getNumSpans()) + "spans took:" +
         dec(getDiffUs()) + "uS";
  }
//}}}
//{{{
const string cLcd::getPaddedInfoString() {
// return info string with padded format for on screen display

  return dec(getUpdatePixels(),5,'0') + " " +
         dec(getUpdateUs(),5,'0') + "uS " +
         dec(getNumSpans(),4,'0') + " " +
         dec(getDiffUs(),3,'0') + "uS";
  }
//}}}
//{{{
void cLcd::setFont (const uint8_t* font, const int fontSize)  {

  FT_Init_FreeType (&mLibrary);
  FT_New_Memory_Face (mLibrary, (FT_Byte*)font, fontSize, 0, &mFace);
  }
//}}}

//{{{
void cLcd::clear (const uint16_t colour) {
// start update

  uint64_t colour64 = colour;
  colour64 |= (colour64 << 48) | (colour64 << 32) | (colour64 << 16);

  uint64_t* ptr = (uint64_t*)mFrameBuf;
  for (int i = 0; i < getNumPixels()/4; i++)
    *ptr++ = colour64;
  }
//}}}
//{{{
void cLcd::clearSnapshot() {
// start update, snapshot main display to frameBuffer

  vc_dispmanx_snapshot (mDisplay, mSnapshot, DISPMANX_TRANSFORM_T(0));
  vc_dispmanx_resource_read_data (mSnapshot, &mVcRect, mFrameBuf, getWidth() * 2);
  }
//}}}
//{{{
bool cLcd::present() {
// present update

  if (mInfo)
    text (kWhite, cPoint(0,0), 20, getPaddedInfoString());

  // make diffSpans list
  double diffStartTime = time();
  mNumDiffSpans = diffCoarse (mDiffSpans);
  if (!mNumDiffSpans) // nothing changed
    return false;

  // merge diffSpans
  merge (mDiffSpans, kSpanMergeThreshold16);
  mDiffUs = int((time() - diffStartTime) * 1000000.);

  // update lcd with diffSpans
  double updateStartTime = time();
  mUpdatePixels = updateLcd (mDiffSpans);
  mUpdateUs = int((time() - updateStartTime) * 1000000.);

  auto temp = mPrevFrameBuf;
  mPrevFrameBuf = mFrameBuf;
  mFrameBuf = temp;

  return true;
  }
//}}}

//{{{
void cLcd::rect (const uint16_t colour, const cRect& r) {
// rect with right,bottom clip

  int16_t xmax = min (r.right, getWidth());
  int16_t ymax = min (r.bottom, getHeight());

  for (int y = r.top; y < ymax; y++) {
    uint16_t* ptr = mFrameBuf + y*getWidth() + r.left;
    for (int x = r.left; x < xmax; x++)
      *ptr++ = colour;
    }
  }
//}}}
//{{{
void cLcd::rect (const uint16_t colour, const uint8_t alpha, const cRect& r) {

  uint16_t xmax = min (r.right, getWidth());
  uint16_t ymax = min (r.bottom, getHeight());

  for (int y = r.top; y < ymax; y++)
    for (int x = r.left; x < xmax; x++)
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
void cLcd::pix (const uint16_t colour, const uint8_t alpha, const cPoint& p) {
// blend with clip
// magical rgb565 alpha composite
// - linear interp background * (1.0 - alpha) + foreground * alpha
//   - factorized into: result = background + (foreground - background) * alpha
//   - alpha is in Q1.5 format, so 0.0 is represented by 0, and 1.0 is represented by 32
// - Converts  0000000000000000rrrrrggggggbbbbb
// -     into  00000gggggg00000rrrrr000000bbbbb

  if ((alpha >= 0) && (p.x >= 0) && (p.y > 0) && (p.x < getWidth()) && (p.y < getHeight())) {
    // clip opaque and offscreen
    if (alpha == 0xFF)
      // simple case - set bigEndianColour frameBuf pixel to littleEndian colour
      mFrameBuf[(p.y*getWidth()) + p.x] = colour;
    else {
      // get bigEndianColour frame buffer into littleEndian background
      uint32_t background = mFrameBuf[(p.y*getWidth()) + p.x];

      // composite littleEndian colour
      uint32_t foreground = colour;
      foreground = (foreground | (foreground << 16)) & 0x07e0f81f;
      background = (background | (background << 16)) & 0x07e0f81f;
      background += (((foreground - background) * ((alpha + 4) >> 3)) >> 5) & 0x07e0f81f;

      // set bigEndianColour frameBuf pixel to littleEndian background result
      mFrameBuf[(p.y*getWidth()) + p.x] = background | (background >> 16);
      }
    }
  }
//}}}
//{{{
void cLcd::copy (const uint16_t* src) {
// copy all of src of same width,height

  memcpy (mFrameBuf, src, getNumPixels() * 2);
  }
//}}}
//{{{
void cLcd::copy (const uint16_t* src, const cRect& srcRect, const cPoint& dstPoint) {
// copy rect from src of same width,height

  for (int y = 0; y < srcRect.getHeight(); y++)
    memcpy (mFrameBuf + ((dstPoint.y + y) * getWidth()) + dstPoint.x,
                  src + ((srcRect.top + y) * getWidth()) + srcRect.left,
            srcRect.getWidth() * 2);
  }
//}}}
//{{{
int cLcd::text (const uint16_t colour, const cPoint& p, const int height, const string& str) {

  FT_Set_Pixel_Sizes (mFace, 0, height);

  int curX = p.x;
  for (unsigned i = 0; (i < str.size()) && (curX < getWidth()); i++) {
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
//}}}

//{{{
void cLcd::delayUs (const int us) {
// delay in microSeconds

  gpioDelay (us);
  }
//}}}
//{{{
double cLcd::time() {
// return time in double microSeconds

  return time_time();
  }
//}}}

// protected
//{{{
bool cLcd::initialise() {

  unsigned version = gpioVersion();
  unsigned hardwareRevision = gpioHardwareRevision();
  cLog::log (LOGINFO, "pigpio hwRev:%x version:%d", hardwareRevision, version);

  if (gpioInitialise() <= 0)
    return false;

  setFont (getFreeSansBold(), getFreeSansBoldSize());

  // allocate and clear both frameBuffers to black
  mFrameBuf = (uint16_t*)aligned_alloc (getNumPixels() * 2, 32);
  if (!mFrameBuf) {
    //{{{  error return
    cLog::log (LOGERROR, "frameBuf allocate");
    return false;
    }
    //}}}
  mPrevFrameBuf = (uint16_t*)aligned_alloc (getNumPixels() * 2, 32);
  if (!mPrevFrameBuf) {
    //{{{  error return
    cLog::log (LOGERROR, "prevFrameBuf allocate");
    return false;
    }
    //}}}
  clear();
  clear();

  // allocate large possible size
  // !!! do something better to bomb out > couple of thousand !!!
  mDiffSpans = (sSpan*)malloc ((getNumPixels() / 2) * sizeof(sSpan));
  if (!mDiffSpans) {
    //{{{  error return
    cLog::log (LOGERROR, "diffSpans allocate");
    return false;
    }
    //}}}

  // dispmanx init
  bcm_host_init();
  mDisplay = vc_dispmanx_display_open (0);
  if (!mDisplay) {
    //{{{  error return
    cLog::log (LOGERROR, "vc_dispmanx_display_open failed");
    return false;
    }
    //}}}
  if (vc_dispmanx_display_get_info (mDisplay, &mModeInfo)) {
    //{{{  error return
    cLog::log (LOGERROR, "vc_dispmanx_display_get_info failed");
    return false;
    }
    //}}}
  mSnapshot = vc_dispmanx_resource_create (VC_IMAGE_RGB565, getWidth(), getHeight(), &mImagePrt);
  if (!mSnapshot) {
    //{{{  error return
    cLog::log (LOGERROR, "vc_dispmanx_resource_create failed");
    return false;
    }
    //}}}
  vc_dispmanx_rect_set (&mVcRect, 0, 0, getWidth(), getHeight());

  cLog::log (LOGINFO, "Dispmanx %dx%d", mModeInfo.width, mModeInfo.height);

  // reset lcd
  gpioSetMode (kResetGpio, PI_OUTPUT);
  gpioWrite (kResetGpio, 0);
  gpioDelay (10000);
  gpioWrite (kResetGpio, 1);
  gpioDelay (120000);

  return true;
  }
//}}}

//{{{  private
// faster asm coarse diff test
constexpr bool kCoarseDiff = true;

//{{{
bool cLcd::diffSingle (sSpan* spans) {
// return true with single bopunding span if differnet

  int minY = 0;
  int minX = -1;

  // stride as uint16 elements.
  const int stride = getWidth();
  const int widthAligned4 = (uint32_t)getWidth() & ~3u;

  uint16_t* scanline = mFrameBuf;
  uint16_t* prevFrameScanline = mPrevFrameBuf;

  if (kCoarseDiff) {
    //{{{  coarse diff
    // Coarse diff computes a diff at 8 adjacent pixels at a time
    // returns the point to the 8-pixel aligned coordinate where the pixels began to differ.

    int numPixels = getWidth() * getHeight();
    int firstDiff = coarseLinearDiff (mFrameBuf, mPrevFrameBuf, mFrameBuf + numPixels);
    if (firstDiff == numPixels)
      return false;

    // Compute the precise diff position here.
    while (mFrameBuf[firstDiff] == mPrevFrameBuf[firstDiff])
      ++firstDiff;

    minX = firstDiff % getWidth();
    minY = firstDiff / getWidth();
    }
    //}}}
  else {
    //{{{  fine diff
    while (minY < getHeight()) {
      int x = 0;
      // diff 4 pixels at a time
      for (; x < widthAligned4; x += 4) {
        uint64_t diff = *(uint64_t*)(scanline+x) ^ *(uint64_t*)(prevFrameScanline+x);
        if (diff) {
          minX = x + (__builtin_ctzll (diff) >> 4);
          goto foundTop;
          }
        }

      // tail unaligned 0-3 pixels one by one
      for (; x < getWidth(); ++x) {
        uint16_t diff = *(scanline+x) ^ *(prevFrameScanline+x);
        if (diff) {
          minX = x;
          goto foundTop;
          }
        }

      scanline += stride;
      prevFrameScanline += stride;
      ++minY;
      }

    // No pixels changed, nothing to do.
    return false;
    }
    //}}}

  foundTop:
  //{{{  found top
  int maxX = -1;
  int maxY = getHeight() - 1;

  if (kCoarseDiff) {
    int numPixels = getWidth() * getHeight();

    // coarse diff computes a diff at 8 adjacent pixels at a time
    // returns the point to the 8-pixel aligned coordinate where the pixels began to differ.
    int firstDiff = coarseLinearDiffBack (mFrameBuf, mPrevFrameBuf, mFrameBuf + numPixels);

    // compute the precise diff position here.
    while (firstDiff > 0 && mFrameBuf[firstDiff] == mPrevFrameBuf[firstDiff])
      --firstDiff;
    maxX = firstDiff % getWidth();
    maxY = firstDiff / getWidth();
    }

  else {
    scanline = mFrameBuf + (getHeight() - 1)*stride;
    // same scanline from previous frame, not preceding scanline
    prevFrameScanline = mPrevFrameBuf + (getHeight() - 1)*stride;

    while (maxY >= minY) {
      int x = getWidth()-1;
      // tail unaligned 0-3 pixels one by one
      for (; x >= widthAligned4; --x) {
        if (scanline[x] != prevFrameScanline[x]) {
          maxX = x;
          goto foundBottom;
          }
        }

      // diff 4 pixels at a time
      x = x & ~3u;
      for (; x >= 0; x -= 4) {
        uint64_t diff = *(uint64_t*)(scanline + x) ^ *(uint64_t*)(prevFrameScanline + x);
        if (diff) {
          maxX = x + 3 - (__builtin_clzll(diff) >> 4);
          goto foundBottom;
          }
        }

      scanline -= stride;
      prevFrameScanline -= stride;
      --maxY;
      }
    }
  //}}}

  foundBottom:
  //{{{  found bottom
  scanline = mFrameBuf + minY*stride;
  prevFrameScanline = mPrevFrameBuf + minY*stride;

  int lastScanRight = maxX;
  if (minX > maxX) {
    //{{{  swap minX, maxX
    uint32_t temp = minX;
    minX = maxX;
    maxX = temp;
    }
    //}}}

  int leftX = 0;
  while (leftX < minX) {
    uint16_t* s = scanline + leftX;
    uint16_t* prevS = prevFrameScanline + leftX;
    for (int y = minY; y <= maxY; ++y) {
      if (*s != *prevS)
        goto foundLeft;

      s += stride;
      prevS += stride;
      }

    ++leftX;
    }
  //}}}

  foundLeft:
  //{{{  found left
  int rightX = getWidth()-1;
  while (rightX > maxX) {
    uint16_t* s = scanline + rightX;
    uint16_t* prevS = prevFrameScanline + rightX;
    for (int y = minY; y <= maxY; ++y) {
      if (*s != *prevS)
        goto foundRight;

      s += stride;
      prevS += stride;
      }

    --rightX;
    }
  //}}}

  foundRight:
  sSpan* head = spans;

  head->r.left = leftX;
  head->r.right = rightX+1;
  head->r.top = minY;
  head->r.bottom = maxY+1;

  head->lastScanRight = lastScanRight+1;
  head->size = (head->r.right - head->r.left) * (head->r.bottom - head->r.top - 1) + (head->lastScanRight - head->r.left);
  head->next = nullptr;

  return true;
  }
//}}}
//{{{
int cLcd::diffCoarse (sSpan* spans) {
// return numDiffSpans

  int numDiffSpans = 0;

  int y =  0;
  int yInc = 1;

  uint64_t* scanline = (uint64_t*)(mFrameBuf + (y * getWidth()));
  uint64_t* prevFrameScanline = (uint64_t*)(mPrevFrameBuf + (y * getWidth()));

  const int width64 = getWidth() >> 2;
  int scanlineInc = getWidth() >> 2;

  sSpan* span = spans;
  while (y < getHeight()) {
    uint16_t* scanlineStart = (uint16_t*)scanline;

    for (int x = 0; x < width64;) {
      if (scanline[x] != prevFrameScanline[x]) {
        uint16_t* spanStart = (uint16_t*)(scanline + x) + (__builtin_ctzll (scanline[x] ^ prevFrameScanline[x]) >> 4);
        ++x;

        // found start of a span of different pixels on this scanline, now find where this span ends
        uint16_t* spanEnd;
        for (;;) {
          if (x < width64) {
            if (scanline[x] != prevFrameScanline[x]) {
              ++x;
              continue;
              }
            else {
              spanEnd = (uint16_t*)(scanline + x) + 1 - (__builtin_clzll(scanline[x-1] ^ prevFrameScanline[x-1]) >> 4);
              ++x;
              break;
              }
            }
          else {
            spanEnd = scanlineStart + getWidth();
            break;
            }
          }

        span->r.left = spanStart - scanlineStart;
        span->r.right = spanEnd - scanlineStart;
        span->r.top = y;
        span->r.bottom = y+1;

        span->lastScanRight = span->r.right;
        span->size = spanEnd - spanStart;
        span->next = span+1;

        ++span;
        ++numDiffSpans;
        }
      else
        ++x;
      }

    y += yInc;
    scanline += scanlineInc;
    prevFrameScanline += scanlineInc;
    }

  if (numDiffSpans > 0)
    spans[numDiffSpans-1].next = nullptr;

  return numDiffSpans;
  }
//}}}
//{{{
int cLcd::diffExact (sSpan* spans) {
// return numDiffSpans

  int numDiffSpans = 0;

  int y =  0;
  int yInc = 1;

  uint16_t* scanline = mFrameBuf + y * getWidth();
  uint16_t* prevFrameScanline = mPrevFrameBuf + y * getWidth();

  int scanlineInc = getWidth();
  int scanlineEndInc = scanlineInc - getWidth();

  while (y < getHeight()) {
    uint16_t* scanlineStart = scanline;
    uint16_t* scanlineEnd = scanline + getWidth();
    while (scanline < scanlineEnd) {
      uint16_t* spanStart;
      uint16_t* spanEnd;
      int numConsecutiveUnchangedPixels = 0;

      if (scanline + 1 < scanlineEnd) {
        uint32_t diff = (*(uint32_t*)scanline) ^ (*(uint32_t*)prevFrameScanline);
        scanline += 2;
        prevFrameScanline += 2;

        if (diff == 0) // Both 1st and 2nd pixels are the same
          continue;

        if ((diff & 0xFFFF) == 0) {
          //{{{  1st pixels are the same, 2nd pixels are not
          spanStart = scanline - 1;
          spanEnd = scanline;
          }
          //}}}
        else {
          //{{{  1st pixels are different
          spanStart = scanline - 2;
          if ((diff & 0xFFFF0000u) != 0) // 2nd pixels are different?
            spanEnd = scanline;
          else {
            spanEnd = scanline - 1;
            numConsecutiveUnchangedPixels = 1;
            }
          }
          //}}}
        //{{{  We've found a start of a span of different pixels on this scanline, now find where this span ends
        while (scanline < scanlineEnd) {
          if (*scanline++ != *prevFrameScanline++) {
            spanEnd = scanline;
            numConsecutiveUnchangedPixels = 0;
            }
          else {
            if (++numConsecutiveUnchangedPixels > kSpanMergeThreshold8)
              break;
            }
          }
        //}}}
        }
      else {
       //{{{  handle the single last pixel on the row
       if (*scanline++ == *prevFrameScanline++)
         break;

       spanStart = scanline - 1;
       spanEnd = scanline;
       }
       //}}}

      sSpan* span = spans + numDiffSpans;

      span->r.left = spanStart - scanlineStart;
      span->r.right = span->lastScanRight = spanEnd - scanlineStart;
      span->r.top = y;
      span->r.bottom = y+1;

      span->lastScanRight = span->r.right;
      span->size = spanEnd - spanStart;

      if (numDiffSpans > 0)
        span[-1].next = span;
      span->next = nullptr;

      ++numDiffSpans;
      }

    y += yInc;
    scanline += scanlineEndInc;
    prevFrameScanline += scanlineEndInc;
    }

  return numDiffSpans;
  }
//}}}

//{{{
sSpan* cLcd::merge (sSpan* spans, int pixelThreshold) {

  for (sSpan* i = spans; i; i = i->next) {
    sSpan* prev = i;
    for (sSpan* j = i->next; j; j = j->next) {
      // If the spans i and j are vertically apart, don't attempt to merge span i any further
      // since all spans >= j will also be farther vertically apart.
      // (the list is nondecreasing with respect to Span::y)
      if (j->r.top > i->r.bottom)
        break;

      // Merge the spans i and j, and figure out the wastage of doing so
      int left = min (i->r.left, j->r.left);
      int top = min (i->r.top, j->r.top);
      int right = max (i->r.right, j->r.right);
      int bottom = max (i->r.bottom, j->r.bottom);

      int lastScanRight = bottom > i->r.bottom ?
                            j->lastScanRight : (bottom > j->r.bottom ?
                              i->lastScanRight : max (i->lastScanRight, j->lastScanRight));

      int newSize = (right - left) * (bottom - top - 1) + (lastScanRight - left);

      int wastedPixels = newSize - i->size - j->size;
      if (wastedPixels <= pixelThreshold) {
        i->r.left = left;
        i->r.top = top;
        i->r.right = right;
        i->r.bottom = bottom;

        i->lastScanRight = lastScanRight;
        i->size = newSize;

        prev->next = j->next;
        j = prev;
        }
      else // Not merging - travel to next node remembering where we came from
        prev = j;
      }
    }

  return spans;
  }
//}}}

//{{{
int cLcd::coarseLinearDiff (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd) {
// Coarse diffing of two frameBufs with tight stride, 16 pixels at a time
// Finds the first changed pixel, coarse result aligned down to 8 pixels boundary

  uint16_t* endPtr;

  asm volatile(
    "mov r0, %[frameBufEnd]\n"   // r0 <- pointer to end of current frameBuf
    "mov r1, %[frameBuf]\n"      // r1 <- current frameBuf
    "mov r2, %[prevFrameBuf]\n"  // r2 <- frameBuf of previous frame

    "start_%=:\n"
      "pld [r1, #128]\n" // preload data caches for both current and previous frameBufs 128 bytes ahead of time
      "pld [r2, #128]\n"

      "ldmia r1!, {r3,r4,r5,r6}\n"  // load 4x32-bit elements (8 pixels) of current frameBuf
      "ldmia r2!, {r7,r8,r9,r10}\n" // load corresponding 4x32-bit elements (8 pixels) of previous frameBuf
      "cmp r3, r7\n"                // compare all 8 pixels if they are different
      "cmpeq r4, r8\n"
      "cmpeq r5, r9\n"
      "cmpeq r6, r10\n"
      "bne end_%=\n"                // if we found a difference, we are done

      // Unroll once for another set of 4x32-bit elements.
      // On Raspberry Pi Zero, data cache line is 32 bytes in size
      // one iteration of the loop computes a single data cache line, with preloads in place at the top.
      "ldmia r1!, {r3,r4,r5,r6}\n"
      "ldmia r2!, {r7,r8,r9,r10}\n"
      "cmp r3, r7\n"
      "cmpeq r4, r8\n"
      "cmpeq r5, r9\n"
      "cmpeq r6, r10\n"
      "bne end_%=\n"                // if we found a difference, we are done

      "cmp r0, r1\n"                // frameBuf == frameBufEnd? did we finish through the array?
      "bne start_%=\n"
      "b done_%=\n"

    "end_%=:\n"
      "sub r1, r1, #16\n"           // ldmia r1! increments r1 after load
                                    //subtract back last increment in order to not shoot past the first changed pixels
    "done_%=:\n"
      "mov %[endPtr], r1\n"         // output endPtr back to C code
      : [endPtr]"=r"(endPtr)
      : [frameBuf]"r"(frameBuf), [prevFrameBuf]"r"(prevFrameBuf), [frameBufEnd]"r"(frameBufEnd)
      : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "cc"
    );

  return endPtr - frameBuf;
  }
//}}}
//{{{
int cLcd::coarseLinearDiffBack (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd) {
// Same as coarse_linear_diff, but finds the last changed pixel in linear order instead of first, i.e.
// Finds the last changed pixel, coarse result aligned up to 8 pixels boundary

  uint16_t* endPtr;
  asm volatile(
    "mov r0, %[frameBufBegin]\n" // r0 <- pointer to beginning of current frameBuf
    "mov r1, %[frameBuf]\n"      // r1 <- current frameBuf (starting from end of frameBuf)
    "mov r2, %[prevFrameBuf]\n"  // r2 <- frameBuf of previous frame (starting from end of frameBuf)

    "start_%=:\n"
      "pld [r1, #-128]\n"           // preload data caches for both current and previous frameBufs 128 bytes ahead of time
      "pld [r2, #-128]\n"

      "ldmdb r1!, {r3,r4,r5,r6}\n"  // load 4x32-bit elements (8 pixels) of current frameBuf
      "ldmdb r2!, {r7,r8,r9,r10}\n" // load corresponding 4x32-bit elements (8 pixels) of previous frameBuf
      "cmp r3, r7\n"                // compare all 8 pixels if they are different
      "cmpeq r4, r8\n"
      "cmpeq r5, r9\n"
      "cmpeq r6, r10\n"
      "bne end_%=\n"                // if we found a difference, we are done

      // Unroll once for another set of 4x32-bit elements. On Raspberry Pi Zero, data cache line is 32 bytes in size, so one iteration
      // of the loop computes a single data cache line, with preloads in place at the top.
      "ldmdb r1!, {r3,r4,r5,r6}\n"
      "ldmdb r2!, {r7,r8,r9,r10}\n"
      "cmp r3, r7\n"
      "cmpeq r4, r8\n"
      "cmpeq r5, r9\n"
      "cmpeq r6, r10\n"
      "bne end_%=\n"                // if we found a difference, we are done

      "cmp r0, r1\n"                // frameBuf == frameBufEnd? did we finish through the array?
      "bne start_%=\n"
      "b done_%=\n"

    "end_%=:\n"
      "add r1, r1, #16\n"           // ldmdb r1! decrements r1 before load,
                                    // so add back the last decrement in order to not shoot past the first changed pixels
    "done_%=:\n"
      "mov %[endPtr], r1\n"         // output endPtr back to C code
      : [endPtr]"=r"(endPtr)
      : [frameBuf]"r"(frameBufEnd), [prevFrameBuf]"r"(prevFrameBuf+(frameBufEnd-frameBuf)), [frameBufBegin]"r"(frameBuf)
      : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "cc"
    );

  return endPtr - frameBuf;
  }
//}}}
//}}}

// 16bit parallel
//{{{  cLcd16 : public cLcd 
// 16bit J8 header pins, gpio, constexpr
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

constexpr uint8_t k16WriteGpio = 17;
constexpr uint8_t k16ReadGpio = 22;
constexpr uint8_t k16ChipSelectGpio = 23;
constexpr uint8_t k16RegisterSelectGpio = 24;
constexpr uint8_t k16BacklightGpio = 27;

constexpr uint32_t k16DataMask = 0xFFFF;
constexpr uint32_t k16WriteMask = 1 << k16WriteGpio;
constexpr uint32_t k16WriteClrMask = k16WriteMask | k16DataMask;

//{{{
void cLcd16::writeCommand (const uint8_t command) {

  gpioWrite (k16RegisterSelectGpio, 0);

  gpioWrite_Bits_0_31_Clear (~command & k16WriteClrMask); // clear lo data bits + k16WrGpio bit lo
  gpioWrite_Bits_0_31_Set (command);                      // set hi data bits
  gpioWrite_Bits_0_31_Set (k16WriteMask);                 // write on k16WrGpio rising edge

  gpioWrite (k16RegisterSelectGpio, 1);
  }
//}}}
//{{{
void cLcd16::writeDataWord (const uint16_t data) {

  fastGpioWrite_Bits_0_31_Clear (~data & k16WriteClrMask); // clear lo data bits + k16WrGpio bit lo
  fastGpioWrite_Bits_0_31_Set (data);                      // set hi data bits
  fastGpioWrite_Bits_0_31_Set (k16WriteMask);              // write on k16WrGpio rising edge
  }
//}}}
//{{{
void cLcd16::writeCommandData (const uint8_t command, const uint16_t data) {

  writeCommand (command);
  writeDataWord (data);
  }
//}}}
//{{{
void cLcd16::writeCommandMultiData (const uint8_t command, const uint8_t* dataPtr, const int len) {

  writeCommand (command);

  // send data
  uint16_t* ptr = (uint16_t*)dataPtr;
  uint16_t* ptrEnd = (uint16_t*)dataPtr + len/2;

  while (ptr < ptrEnd)
    writeDataWord (*ptr++);
  }
//}}}
//}}}
//{{{  cLcdTa7601 : public cLcd16
constexpr int16_t kWidthTa7601 = 320;
constexpr int16_t kHeightTa7601 = 480;

cLcdTa7601::cLcdTa7601 (const int rotate, const int info)
  : cLcd16(kWidthTa7601, kHeightTa7601, rotate, info) {}

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
  gpioSetMode (k16RegisterSelectGpio, PI_OUTPUT);
  gpioWrite (k16RegisterSelectGpio, 1);

  // chipSelect
  gpioSetMode (k16ChipSelectGpio, PI_OUTPUT);
  gpioWrite (k16ChipSelectGpio, 0);

  // backlight
  gpioSetMode (k16BacklightGpio, PI_OUTPUT);
  gpioWrite (k16BacklightGpio, 1);

  // 16 d0-d15
  for (int i = 0; i < 16; i++)
    gpioSetMode (i, PI_OUTPUT);
  gpioWrite_Bits_0_31_Clear (k16DataMask);

  // portrait mode with (0,0) being the top left. top is the side opposite the LCD connector.
  writeCommandData (0x01, 0x023C); // gate_scan & display boundary
  writeCommandData (0x02, 0x0100); // inversion
  writeCommandData (0x03, 0x1030); // GRAM access
  writeCommandData (0x08, 0x0808); // Porch period
  writeCommandData (0x0A, 0x0500); // osc control & clock number per 1H
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

  writeCommandData (0x45, 0x0000);          // h start ram address window even = 0
  writeCommandData (0x44, kWidthTa7601-1);  // h end ram address window even   = 320-1
  writeCommandData (0x47, 0x0000);          // v start ram address window      = 0
  writeCommandData (0x46, kHeightTa7601-1); // v end ram address window        = 480-1

  updateLcd();
  return true;
  }
//}}}

//{{{
void cLcdTa7601::writeCommand (const uint8_t command) {
// slow down a bit

  gpioWrite (k16RegisterSelectGpio, 0);

  gpioWrite_Bits_0_31_Clear (~command & k16WriteClrMask); // clear lo data bits + k16WrGpio bit lo
  gpioWrite_Bits_0_31_Set (command);                      // set hi data bits
  gpioWrite_Bits_0_31_Set (command);                      // set hi data bits
  gpioWrite_Bits_0_31_Set (k16WriteMask);                 // write on k16WrGpio rising edge

  gpioWrite (k16RegisterSelectGpio, 1);
  }
//}}}
//{{{
void cLcdTa7601::writeDataWord (const uint16_t data) {
// slow down data write

  fastGpioWrite_Bits_0_31_Clear (~data & k16WriteClrMask); // clear lo data bits + k16WrGpio bit lo
  fastGpioWrite_Bits_0_31_Set (data);                      // set hi data bits
  fastGpioWrite_Bits_0_31_Set (data);                      // set hi data bits
  fastGpioWrite_Bits_0_31_Set (data);                      // set hi data bits
  fastGpioWrite_Bits_0_31_Set (k16WriteMask);              // write on k16WrGpio rising edge
  }
//}}}

//{{{
int cLcdTa7601::updateLcd (sSpan* spans) {
// no AM style inc GDRAM V rather H for landscape, have to do it yourself

  if (mRotate != 0) // other rotate cases still to do
    return updateLcd();

  // rotate == 0 works
  int numPixels = 0;

  auto it = spans;
  while (it) {
    uint16_t xstart;
    uint16_t ystart;
    uint16_t yinc;

    switch (mRotate) {
      default:
      case 0:
        xstart = it->r.left;
        ystart = it->r.top;
        yinc = 1;
        break;

      case 90:
        xstart = getWidth() - it->r.left;
        ystart = it->r.top;
        yinc = 1;
        break;

      case 180:
        xstart = getWidth()-1 - it->r.left;
        ystart = getHeight()-1 - it->r.top;
        yinc = -1;
        break;

      case 270:
        xstart = it->r.left;
        ystart = getHeight()-1 - it->r.top;
        yinc = -1;
        break;
      }

    for (int y = it->r.top; y < it->r.bottom; y++) {
      writeCommandData (0x20, ystart);  // Y start address of GRAM
      writeCommandData (0x21, xstart);  // X start address of GRAM

      writeCommand (0x22);
      uint16_t* ptr = mFrameBuf + (ystart * getWidth()) + xstart;
      for (int i = 0; i < it->r.getWidth(); i++)
        writeDataWord (*ptr++);

      numPixels += it->r.getWidth();
      ystart += yinc;
      }

    it = it->next;
    }

  return numPixels;
  }
//}}}
//{{{
int cLcdTa7601::updateLcd() {
// no AM style inc GDRAM V rather H for landscape, have to do it yourself

  writeCommandData (0x20, 0x0000);  // Y start address of GRAM
  writeCommandData (0x21, 0x0000);  // X start address of GRAM

  writeCommand (0x22);

  // send data
  switch (mRotate) {
    case 0: {
      uint16_t* ptr = mFrameBuf;
      for (int i = 0; i < getNumPixels(); i++)
        writeDataWord (*ptr++);
      break;
      }

    case 90:
      // !!! simplify the back step at end of line !!!
      for (int x = 0; x < getWidth(); x++) {
        uint16_t* ptr = mFrameBuf + x;
        for (int y = 0; y < getHeight(); y++) {
          writeDataWord (*ptr);
          ptr += getWidth();
          }
        }
      break;

    case 180:
      // !!! simplify !!!
      for (int y = 0; y < getHeight(); y++) {
        uint16_t* ptr = mFrameBuf + ((getHeight()-1-y) * getWidth());
        for (int x = 0; x < getWidth(); x++) {
          writeDataWord (*ptr);
          ptr--;
          }
        }
      break;

    case 270:
      // !!! simplify the back step at aned of line !!!
      for (int x = 0; x < getWidth(); x++) {
        uint16_t* ptr = mFrameBuf + ((getHeight()-1) * getWidth()) + x;
        for (int y = 0; y < getHeight(); y++) {
          writeDataWord (*ptr);
          ptr -= getWidth();
          }
        }
      break;
    }

  return getNumPixels();
  }
//}}}
//}}}
//{{{  cLcdSsd1289 : public cLcd16
constexpr int16_t kWidth1289 = 240;
constexpr int16_t kHeight1289 = 320;

cLcdSsd1289::cLcdSsd1289 (const int rotate, const int info)
  : cLcd16(kWidth1289, kHeight1289, rotate, info) {}

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
  gpioSetMode (k16RegisterSelectGpio, PI_OUTPUT);
  gpioWrite (k16RegisterSelectGpio, 1);

  // 16 d0-d15
  for (int i = 0; i < 16; i++)
    gpioSetMode (i, PI_OUTPUT);
  gpioWrite_Bits_0_31_Clear (k16DataMask);

  // startup commands
  writeCommandData (0x00, 0x0001); // SSD1289_REG_OSCILLATION
  writeCommandData (0x03, 0xA8A4); // SSD1289_REG_POWER_CTRL_1
  writeCommandData (0x0c, 0x0000); // SSD1289_REG_POWER_CTRL_2
  writeCommandData (0x0d, 0x080C); // SSD1289_REG_POWER_CTRL_3
  writeCommandData (0x0e, 0x2B00); // SSD1289_REG_POWER_CTRL_4
  writeCommandData (0x1e, 0x00B7); // SSD1289_REG_POWER_CTRL_5

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

  int xstart = 0;
  int ystart = 0;
  int xres = kWidth1289-1;
  int yres = kHeight1289-1;
  switch (mRotate) {
    case 90:
      writeCommandData (0x11, 0x6040 | 0b011000); // 0x11 REG_ENTRY_MODE
      writeCommandData (0x4e, ystart);            // 0x4E GDDRAM X address counter
      writeCommandData (0x4f, xres - xstart);     // 0x4F GDDRAM Y address counter
      break;
    case 180:
      writeCommandData (0x11, 0x6040 | 0b000000); // 0x11 REG_ENTRY_MODE
      writeCommandData (0x4e, xres - xstart);     // 0x4E GDDRAM X address counter
      writeCommandData (0x4f, yres - ystart);     // 0x4F GDDRAM Y address counter
      break;
    case 270:
      writeCommandData (0x11, 0x6040 | 0b101000); // 0x11 REG_ENTRY_MODE
      writeCommandData (0x4e, yres - ystart);     // 0x4E GDDRAM X address counter
      writeCommandData (0x4f, xstart);            // 0x4F GDDRAM Y address counter
      break;
    default:
      writeCommandData (0x11, 0x6040 | 0b110000); // 0x11 REG_ENTRY_MODE
      writeCommandData (0x4e, xstart);            // 0x4E GDDRAM X address counter
      writeCommandData (0x4f, ystart);            // 0x4F GDDRAM Y address counter
      break;
    }

  updateLcd();
  return true;
  }
//}}}

//{{{
int cLcdSsd1289::updateLcd() {

  double startTime = time_time();
  writeCommandMultiData (0x22, (const uint8_t*)mFrameBuf, getNumPixels() * 2);
  mUpdateUs = (int)((time_time() - startTime) * 1000000.0);

  return getNumPixels();
  }
//}}}
//}}}

// spi
//{{{  cLcdSpi : public cLcd
// spi J8 header pins, gpio, constexpr
//      3.3v 17  18 gpio24   - registerSelect/backlight
// spi0 mosi 19  20 0v
// spi0 miso 20  22 gpio25   - reset
// spi0 sck -23  24 spi0 Ce0 - gpio8 - cs

constexpr uint8_t kSpiCe0Gpio = 8;
constexpr uint8_t kSpiRegisterSelectGpio  = 24;
constexpr uint8_t kBacklightGpio = 24;

//{{{
cLcdSpi::~cLcdSpi() {
  spiClose (mSpiHandle);
  }
//}}}
//}}}
//{{{  cLcdSpiHeaderSelect : public cLcdSpi
//{{{
void cLcdSpiHeaderSelect::writeCommand (const uint8_t command) {

  // send command
  const uint8_t kCommandHeader[3] = { 0x70, 0, command };
  spiWrite (mSpiHandle, (char*)kCommandHeader, 3);
  }
//}}}
//{{{
void cLcdSpiHeaderSelect::writeCommandData (const uint8_t command, const uint16_t data) {

  writeCommand (command);

  // send data
  const uint8_t kDataHeader[3] = { 0x72, uint8_t(data >> 8), uint8_t(data & 0xff) };
  spiWrite (mSpiHandle, (char*)kDataHeader, 3);
  }
//}}}
void cLcdSpiHeaderSelect::writeCommandMultiData (const uint8_t command, const uint8_t* dataPtr, const int len) {}
//}}}
//{{{  cLcdSpiRegisterSelect : public cLcdSpi
//{{{
void cLcdSpiRegisterSelect::writeCommand (const uint8_t command) {

  gpioWrite (kSpiRegisterSelectGpio, 0);
  spiWrite (mSpiHandle, (char*)(&command), 1);
  gpioWrite (kSpiRegisterSelectGpio, 1);
  }
//}}}
//{{{
void cLcdSpiRegisterSelect::writeCommandData (const uint8_t command, const uint16_t data) {

  writeCommand (command);

  uint8_t dataBytes[2] = { uint8_t(data >> 8), uint8_t(data & 0xff) };
  spiWrite (mSpiHandle, (char*)dataBytes, 2);
  }
//}}}
//{{{
void cLcdSpiRegisterSelect::writeCommandMultiData (const uint8_t command, const uint8_t* dataPtr, const int len) {

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
//{{{  cLcdIli9320 : public cLcdSpiHeaderSelect
constexpr int16_t kWidth9320 = 240;
constexpr int16_t kHeight9320 = 320;
constexpr int kSpiClock9320 = 24000000;

cLcdIli9320::cLcdIli9320 (const int rotate, const int info)
  : cLcdSpiHeaderSelect(kWidth9320, kHeight9320, rotate, info) {}

//{{{
void cLcdIli9320::setBacklight (bool on) {
  gpioWrite (kBacklightGpio, on ? 1 : 0);
  }
//}}}

//{{{
bool cLcdIli9320::initialise() {

  if (cLcd::initialise())
    return false;

  // backlight off - active hi
  gpioSetMode (kBacklightGpio, PI_OUTPUT);
  gpioWrite (kBacklightGpio, 0);

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

  writeCommandData (0x0050, 0x0000);        // H GRAM start address
  writeCommandData (0x0051, kWidth9320-1);  // H GRAM end address
  writeCommandData (0x0052, 0x0000);        // V GRAM start address
  writeCommandData (0x0053, kHeight9320-1); // V GRAM end address

  switch (mRotate) {
    case 0:
      writeCommandData (0x03, 0x1030); // Entry Mode - BGR, HV inc, vert write,
      break;
    case 90:
      writeCommandData (0x03, 0x1018); // Entry Mode - BGR, HV inc, vert write,
      break;
    case 180:
      writeCommandData (0x03, 0x1000); // Entry Mode - BGR, HV inc, vert write,
      break;
    case 270:
      writeCommandData (0x03, 0x1028); // Entry Mode - BGR, HV inc, vert write,
      break;
    default:
      cLog::log (LOGERROR, "unkown rotate " + dec (mRotate));
      break;
    }

  updateLcd();
  return true;
  }
//}}}

//{{{
int cLcdIli9320::updateLcd (sSpan* spans) {
// command,data sequences expanded inline

  constexpr uint8_t kCommand20[3] = { 0x70, 0, 0x20 };  // GDRAM vert addr command
  constexpr uint8_t kCommand21[3] = { 0x70, 0, 0x21 };  // GDRAMWR horiz addr command
  constexpr uint8_t kCommand22[3] = { 0x70, 0, 0x22 };  // GDRAMWR command

  uint8_t dataHeader[3] = { 0x72, 0,0 };

  uint16_t dataHeaderBuf [320+1];
  dataHeaderBuf[0] = 0x7272;

  int numPixels = 0;

  auto it = spans;
  while (it) {
    cRect& r = it->r;
    //{{{  set xstart, ystart, yinc
    uint16_t xstart = r.left;
    uint16_t ystart = r.top;
    uint16_t yinc = 1;

    switch (mRotate) {
      case 90:
        xstart = kHeight9320-1 - r.left;
        break;

      case 180:
        xstart = kWidth9320-1 - r.left;
        ystart = kHeight9320-1 - r.top;
        yinc = -1;
        break;

      case 270:
        ystart = kWidth9320-1 - r.top;
        yinc = -1;
        break;
      }
    //}}}
    for (int y = r.top; y < r.bottom; y++) {
      // send GDRAM vert addr command
      spiWrite (mSpiHandle, (char*)kCommand20, 3);

      // - data
      dataHeader[1] = ystart >> 8;
      dataHeader[2] = ystart & 0xff;
      spiWrite (mSpiHandle, (char*)dataHeader, 3);

      // send GDRAMWR horiz addr command
      spiWrite (mSpiHandle, (char*)kCommand21, 3);

      // - data
      dataHeader[1] = xstart >> 8;
      dataHeader[2] = xstart & 0xff;
      spiWrite (mSpiHandle, (char*)dataHeader, 3);

      // send GDRAMWR
      spiWrite (mSpiHandle, (char*)kCommand22, 3);

      // - data, miss header bytes, alignment for bswap_16
      const uint16_t* src = mFrameBuf + (y * getWidth()) + r.left;
      uint16_t* dst = dataHeaderBuf + 1;
      for (int i = 0; i < r.getWidth(); i++)
        *dst++ = bswap_16 (*src++);

      // send from second byte of dataHeaderBuf
      spiWrite (mSpiHandle, ((char*)(dataHeaderBuf))+1, (r.getWidth() * 2) + 1);

      numPixels += r.getWidth();
      ystart += yinc;
      }

    it = it->next;
    }

  return numPixels;
  }
//}}}
//{{{
int cLcdIli9320::updateLcd() {
// command,data sequences expanded inline

  constexpr uint8_t kCommand20[3] = { 0x70, 0, 0x20 };  // GDRAM vert addr command
  constexpr uint8_t kCommand21[3] = { 0x70, 0, 0x21 };  // GDRAMWR horiz addr command
  constexpr uint8_t kCommand22[3] = { 0x70, 0, 0x22 };  // GDRAMWR command

  uint8_t dataHeader[3] = { 0x72, 0,0 };

  uint16_t dataHeaderBuf [320+1];
  dataHeaderBuf[0] = 0x7272;

  cRect r = getRect();

  int numPixels = 0;
  //{{{  set xstart, ystart, yinc
  uint16_t xstart = r.left;
  uint16_t ystart = r.top;
  uint16_t yinc = 1;

  switch (mRotate) {
    case 90:
      xstart = kHeight9320-1 - r.left;
      break;

    case 180:
      xstart = kWidth9320-1 - r.left;
      ystart = kHeight9320-1 - r.top;
      yinc = -1;
      break;

    case 270:
      ystart = kWidth9320-1 - r.top;
      yinc = -1;
      break;
    }
  //}}}
  for (int y = r.top; y < r.bottom; y++) {
    // send GDRAM vert addr command
    spiWrite (mSpiHandle, (char*)kCommand20, 3);

    // - data
    dataHeader[1] = ystart >> 8;
    dataHeader[2] = ystart & 0xff;
    spiWrite (mSpiHandle, (char*)dataHeader, 3);

    // send GDRAMWR horiz addr command
    spiWrite (mSpiHandle, (char*)kCommand21, 3);

    // - data
    dataHeader[1] = xstart >> 8;
    dataHeader[2] = xstart & 0xff;
    spiWrite (mSpiHandle, (char*)dataHeader, 3);

    // send GDRAMWR
    spiWrite (mSpiHandle, (char*)kCommand22, 3);

    // - data, miss header bytes, alignment for bswap_16
    const uint16_t* src = mFrameBuf + (y * getWidth()) + r.left;
    uint16_t* dst = dataHeaderBuf + 1;
    for (int i = 0; i < r.getWidth(); i++)
      *dst++ = bswap_16 (*src++);

    // send from second byte of dataHeaderBuf
    spiWrite (mSpiHandle, ((char*)(dataHeaderBuf))+1, (r.getWidth() * 2) + 1);

    numPixels += r.getWidth();
    ystart += yinc;
    }

  return numPixels;
  }
//}}}
//}}}
//{{{  cLcdSt7735r : public cLcdSpiRegisterSelect
constexpr int16_t kWidth7735 = 128;
constexpr int16_t kHeight7735 = 160;
constexpr int kSpiClock7735 = 24000000;

cLcdSt7735r::cLcdSt7735r (const int rotate, const int info)
  : cLcdSpiRegisterSelect (kWidth7735, kHeight7735, rotate, info) {}

//{{{
bool cLcdSt7735r::initialise() {

  if (!cLcd::initialise())
    return false;

  // rs
  gpioSetMode (kSpiRegisterSelectGpio, PI_OUTPUT);
  gpioWrite (kSpiRegisterSelectGpio, 1);

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

  updateLcd();
  return true;
  }
//}}}

//{{{
int cLcdSt7735r::updateLcd() {

  double startTime = time_time();
  writeCommandMultiData (0x2C, (const uint8_t*)mFrameBuf, getNumPixels() * 2); // RAMRW command
  mUpdateUs = (int)((time_time() - startTime) * 1000000.0);

  return getNumPixels();
  }
//}}}
//}}}
//{{{  cLcdIli9225b : public cLcdSpiRegisterSelect
constexpr int16_t kWidth9225b = 176;
constexpr int16_t kHeight9225b = 220;
constexpr int kSpiClock9225b = 16000000;

cLcdIli9225b::cLcdIli9225b (const int rotate, const int info)
  : cLcdSpiRegisterSelect(kWidth9225b, kHeight9225b, rotate, info) {}

//{{{
bool cLcdIli9225b::initialise() {

  if (cLcd::initialise())
    return false;

  // rs
  gpioSetMode (kSpiRegisterSelectGpio, PI_OUTPUT);
  gpioWrite (kSpiRegisterSelectGpio, 1);

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
  writeCommandData (0x36, getWidth()-1);
  writeCommandData (0x37, 0);
  writeCommandData (0x38, getHeight()-1);
  writeCommandData (0x39, 0);
  writeCommandData (0x20, 0);
  writeCommandData (0x21, 0);
  //}}}

  updateLcd();
  return true;
  }
//}}}

//{{{
int cLcdIli9225b::updateLcd() {

  double startTime = time_time();
  writeCommandMultiData (0x22, (const uint8_t*)mFrameBuf, getNumPixels() * 2);
  mUpdateUs = (int)((time_time() - startTime) * 1000000.0);

  return getNumPixels();
  }
//}}}
//}}}
