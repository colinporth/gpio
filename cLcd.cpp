// cLcd.cpp - rgb565 lcd
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
//{{{  include static freetype - assumes singleton cLcd
#include <ft2build.h>
#include FT_FREETYPE_H

static FT_Library mLibrary;
static FT_Face mFace;
//}}}
//{{{  include static dispmanx - assumes singleton cLcd
#include <bcm_host.h>

static DISPMANX_DISPLAY_HANDLE_T mDisplay;
static DISPMANX_MODEINFO_T mModeInfo;
static DISPMANX_RESOURCE_HANDLE_T mSnapshot;
static VC_RECT_T mVcRect;
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

//{{{
string cRect::getString() {
  return "l:" + dec(left) + " r:" + dec(right) + " t:" + dec(top) + " b:" + dec(bottom);
  }
//}}}
//{{{
string cRect::getYfirstString() {
  return "t:" + dec(top) + " b:" + dec(bottom) + " l:" + dec(left) + " r:" + dec(right);
  }
//}}}

//{{{  span constexpr
constexpr int kMaxSpans = 10000;
constexpr bool kCoarseDiff = true;
constexpr int kSpanExactThreshold = 8;
constexpr int kSpanMergeThreshold = 16;
//}}}
//{{{
struct sSpan {
  cRect r;

  uint16_t lastScanRight; // scanline bottom-1 can be partial, ends in lastScanRight.
  uint32_t size;

  sSpan* next;   // linked skip list in array for fast pruning
  };
//}}}

//{{{
struct sCell {
public:
  //{{{
  void set (int16_t x, int16_t y, int c, int a) {

    mPackedCoord = (y << 16) + x;
    mCoverage = c;
    mArea = a;
    }
  //}}}
  //{{{
  void setCoverage (int32_t c, int32_t a) {

    mCoverage = c;
    mArea = a;
    }
  //}}}
  //{{{
  void addCoverage (int32_t c, int32_t a) {

    mCoverage += c;
    mArea += a;
    }
  //}}}

  int32_t mPackedCoord;
  int32_t mCoverage;
  int32_t mArea;
  };
//}}}
//{{{
class cOutline {
public:
  //{{{
  cOutline() {
    mNumCellsInBlock = 2048;
    reset();
    }
  //}}}
  //{{{
  ~cOutline() {

    free (mSortedCells);

    if (mNumBlockOfCells) {
      sCell** ptr = mBlockOfCells + mNumBlockOfCells - 1;
      while (mNumBlockOfCells--) {
        // free a block of cells
        free (*ptr);
        ptr--;
        }

      // free pointers to blockOfCells
      free (mBlockOfCells);
      }
    }
  //}}}

  int32_t getMinx() const { return mMinx; }
  int32_t getMiny() const { return mMiny; }
  int32_t getMaxx() const { return mMaxx; }
  int32_t getMaxy() const { return mMaxy; }

  uint16_t getNumCells() const { return mNumCells; }
  //{{{
  const sCell* const* getSortedCells() {

    if (!mClosed) {
      lineTo (mClosex, mClosey);
      mClosed = true;
      }

    // Perform sort only the first time.
    if (mSortRequired) {
      addCurCell();
      if (mNumCells == 0)
        return 0;

      sortCells();
      mSortRequired = false;
      }

    return mSortedCells;
    }
  //}}}

  //{{{
  void reset() {

    mNumCells = 0;
    mCurCell.set (0x7FFF, 0x7FFF, 0, 0);
    mSortRequired = true;
    mClosed = true;

    mMinx =  0x7FFFFFFF;
    mMiny =  0x7FFFFFFF;
    mMaxx = -0x7FFFFFFF;
    mMaxy = -0x7FFFFFFF;
    }
  //}}}
  //{{{
  void moveTo (int32_t x, int32_t y) {

    if (!mSortRequired)
      reset();

    if (!mClosed)
      lineTo (mClosex, mClosey);

    setCurCell (x >> 8, y >> 8);

    mCurx = x;
    mClosex = x;
    mCury = y;
    mClosey = y;
    }
  //}}}
  //{{{
  void lineTo (int32_t x, int32_t y) {

    if (mSortRequired && ((mCurx ^ x) | (mCury ^ y))) {
      int c = mCurx >> 8;
      if (c < mMinx)
        mMinx = c;
      ++c;
      if (c > mMaxx)
        mMaxx = c;

      c = x >> 8;
      if (c < mMinx)
        mMinx = c;
      ++c;
      if (c > mMaxx)
        mMaxx = c;

      renderLine (mCurx, mCury, x, y);
      mCurx = x;
      mCury = y;
      mClosed = false;
      }
    }
  //}}}

private:
  //{{{
  void addCurCell() {

    if (mCurCell.mArea | mCurCell.mCoverage) {
      if ((mNumCells % mNumCellsInBlock) == 0) {
        // use next block of sCells
        uint32_t block = mNumCells / mNumCellsInBlock;
        if (block >= mNumBlockOfCells) {
          // allocate new block of sCells
          mBlockOfCells = (sCell**)realloc (mBlockOfCells, (mNumBlockOfCells + 1) * sizeof(sCell*));
          mBlockOfCells[mNumBlockOfCells] = (sCell*)malloc (mNumCellsInBlock * sizeof(sCell));
          mNumBlockOfCells++;
          cLog::log (LOGINFO, "allocated new blockOfCells %d of %d", block, mNumBlockOfCells);
          }
        mCurCellPtr = mBlockOfCells[block];
        }

      *mCurCellPtr++ = mCurCell;
      mNumCells++;
      }
    }
  //}}}
  //{{{
  void setCurCell (int16_t x, int16_t y) {

    if (mCurCell.mPackedCoord != (y << 16) + x) {
      addCurCell();
      mCurCell.set (x, y, 0, 0);
      }
   }
  //}}}
  //{{{
  void swapCells (sCell** a, sCell** b) {
    sCell* temp = *a;
    *a = *b;
    *b = temp;
    }
  //}}}
  //{{{
  void sortCells() {

    if (mNumCells == 0)
      return;

    // allocate mSortedCells, a contiguous vector of sCell pointers
    if (mNumCells > mNumSortedCells) {
      mSortedCells = (sCell**)realloc (mSortedCells, (mNumCells + 1) * 4);
      mNumSortedCells = mNumCells;
      }

    // point mSortedCells at sCells
    sCell** blockPtr = mBlockOfCells;
    sCell** sortedPtr = mSortedCells;
    uint16_t numBlocks = mNumCells / mNumCellsInBlock;
    while (numBlocks--) {
      sCell* cellPtr = *blockPtr++;
      unsigned cellInBlock = mNumCellsInBlock;
      while (cellInBlock--)
        *sortedPtr++ = cellPtr++;
      }

    sCell* cellPtr = *blockPtr++;
    unsigned cellInBlock = mNumCells % mNumCellsInBlock;
    while (cellInBlock--)
      *sortedPtr++ = cellPtr++;

    // terminate mSortedCells with nullptr
    mSortedCells[mNumCells] = nullptr;

    // sort it
    qsortCells (mSortedCells, mNumCells);
    }
  //}}}

  //{{{
  void renderScanLine (int32_t ey, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {

    int ex1 = x1 >> 8;
    int ex2 = x2 >> 8;
    int fx1 = x1 & 0xFF;
    int fx2 = x2 & 0xFF;

    // trivial case. Happens often
    if (y1 == y2) {
      setCurCell (ex2, ey);
      return;
      }

    // single cell
    if (ex1 == ex2) {
      int delta = y2 - y1;
      mCurCell.addCoverage (delta, (fx1 + fx2) * delta);
      return;
      }

    // render a run of adjacent cells on the same scanLine
    int p = (0x100 - fx1) * (y2 - y1);
    int first = 0x100;
    int incr = 1;
    int dx = x2 - x1;
    if (dx < 0) {
      p = fx1 * (y2 - y1);
      first = 0;
      incr = -1;
      dx = -dx;
      }

    int delta = p / dx;
    int mod = p % dx;
    if (mod < 0) {
      delta--;
      mod += dx;
      }

    mCurCell.addCoverage (delta, (fx1 + first) * delta);

    ex1 += incr;
    setCurCell (ex1, ey);
    y1  += delta;
    if (ex1 != ex2) {
      p = 0x100 * (y2 - y1 + delta);
      int lift = p / dx;
      int rem = p % dx;
      if (rem < 0) {
        lift--;
        rem += dx;
        }

      mod -= dx;
      while (ex1 != ex2) {
        delta = lift;
        mod  += rem;
        if (mod >= 0) {
          mod -= dx;
          delta++;
          }

        mCurCell.addCoverage (delta, (0x100) * delta);
        y1 += delta;
        ex1 += incr;
        setCurCell (ex1, ey);
        }
      }
    delta = y2 - y1;
    mCurCell.addCoverage (delta, (fx2 + 0x100 - first) * delta);
    }
  //}}}
  //{{{
  void renderLine (int32_t x1, int32_t y1, int32_t x2, int32_t y2) {

    int ey1 = y1 >> 8;
    int ey2 = y2 >> 8;
    int fy1 = y1 & 0xFF;
    int fy2 = y2 & 0xFF;

    int x_from, x_to;
    int p, rem, mod, lift, delta, first;

    if (ey1   < mMiny)
      mMiny = ey1;
    if (ey1+1 > mMaxy)
      mMaxy = ey1+1;
    if (ey2   < mMiny)
      mMiny = ey2;
    if (ey2+1 > mMaxy)
      mMaxy = ey2+1;

    int dx = x2 - x1;
    int dy = y2 - y1;

    // everything is on a single cScanLine
    if (ey1 == ey2) {
      renderScanLine (ey1, x1, fy1, x2, fy2);
      return;
      }

    // Vertical line - we have to calculate start and end cell
    // the common values of the area and coverage for all cells of the line.
    // We know exactly there's only one cell, so, we don't have to call renderScanLine().
    int incr  = 1;
    if (dx == 0) {
      int ex = x1 >> 8;
      int two_fx = (x1 - (ex << 8)) << 1;
      first = 0x100;
      if (dy < 0) {
        first = 0;
        incr  = -1;
        }

      x_from = x1;
      delta = first - fy1;
      mCurCell.addCoverage (delta, two_fx * delta);

      ey1 += incr;
      setCurCell (ex, ey1);

      delta = first + first - 0x100;
      int area = two_fx * delta;
      while (ey1 != ey2) {
        mCurCell.setCoverage (delta, area);
        ey1 += incr;
        setCurCell (ex, ey1);
        }

      delta = fy2 - 0x100 + first;
      mCurCell.addCoverage (delta, two_fx * delta);
      return;
      }

    // render several scanLines
    p  = (0x100 - fy1) * dx;
    first = 0x100;
    if (dy < 0) {
      p     = fy1 * dx;
      first = 0;
      incr  = -1;
      dy    = -dy;
      }

    delta = p / dy;
    mod = p % dy;
    if (mod < 0) {
      delta--;
      mod += dy;
      }

    x_from = x1 + delta;
    renderScanLine (ey1, x1, fy1, x_from, first);

    ey1 += incr;
    setCurCell (x_from >> 8, ey1);

    if (ey1 != ey2) {
      p = 0x100 * dx;
      lift  = p / dy;
      rem   = p % dy;
      if (rem < 0) {
        lift--;
        rem += dy;
        }
      mod -= dy;
      while (ey1 != ey2) {
        delta = lift;
        mod  += rem;
        if (mod >= 0) {
          mod -= dy;
          delta++;
          }

        x_to = x_from + delta;
        renderScanLine (ey1, x_from, 0x100 - first, x_to, first);
        x_from = x_to;

        ey1 += incr;
        setCurCell (x_from >> 8, ey1);
        }
      }

    renderScanLine (ey1, x_from, 0x100 - first, x2, fy2);
    }
  //}}}

  //{{{
  void qsortCells (sCell** start, unsigned numCells) {

    sCell**  stack[80];
    sCell*** top;
    sCell**  limit;
    sCell**  base;

    limit = start + numCells;
    base = start;
    top = stack;

    while (true) {
      int len = int(limit - base);

      sCell** i;
      sCell** j;
      sCell** pivot;

      if (len > 9) { // qsort_threshold)
        // we use base + len/2 as the pivot
        pivot = base + len / 2;
        swapCells (base, pivot);

        i = base + 1;
        j = limit - 1;
        // now ensure that *i <= *base <= *j
        if ((*j)->mPackedCoord < (*i)->mPackedCoord)
          swapCells (i, j);
        if ((*base)->mPackedCoord < (*i)->mPackedCoord)
          swapCells (base, i);
        if ((*j)->mPackedCoord < (*base)->mPackedCoord)
          swapCells (base, j);

        while (true) {
          do {
            i++;
            } while ((*i)->mPackedCoord < (*base)->mPackedCoord);
          do {
            j--;
            } while ((*base)->mPackedCoord < (*j)->mPackedCoord);
          if ( i > j )
            break;
          swapCells (i, j);
          }
        swapCells (base, j);

        // now, push the largest sub-array
        if(j - base > limit - i) {
          top[0] = base;
          top[1] = j;
          base   = i;
          }
        else {
          top[0] = i;
          top[1] = limit;
          limit  = j;
          }
        top += 2;
        }
      else {
        // the sub-array is small, perform insertion sort
        j = base;
        i = j + 1;

        for (; i < limit; j = i, i++) {
          for (; (*(j+1))->mPackedCoord < (*j)->mPackedCoord; j--) {
            swapCells (j + 1, j);
            if (j == base)
              break;
            }
          }

        if (top > stack) {
          top  -= 2;
          base  = top[0];
          limit = top[1];
          }
        else
          break;
        }
      }
    }
  //}}}

  uint16_t mNumCellsInBlock = 0;
  uint16_t mNumBlockOfCells = 0;
  uint16_t mNumSortedCells = 0;
  sCell** mBlockOfCells = nullptr;
  sCell** mSortedCells = nullptr;

  uint16_t mNumCells;
  sCell mCurCell;
  sCell* mCurCellPtr = nullptr;

  int32_t mCurx = 0;
  int32_t mCury = 0;
  int32_t mClosex = 0;
  int32_t mClosey = 0;

  int32_t mMinx;
  int32_t mMiny;
  int32_t mMaxx;
  int32_t mMaxy;

  bool mClosed;
  bool mSortRequired;
  };
//}}}
//{{{
class cScanLine {
public:
  //{{{
  class iterator {
  public:
    iterator (const cScanLine& scanLine) :
      mCoverage(scanLine.mCoverage), mCurCount(scanLine.mCounts), mCurStartPtr(scanLine.mStartPtrs) {}

    int next() {
      ++mCurCount;
      ++mCurStartPtr;
      return int(*mCurStartPtr - mCoverage);
      }

    int getNumPix() const { return int(*mCurCount); }
    const uint8_t* getCoverage() const { return *mCurStartPtr; }

  private:
    const uint8_t* mCoverage;
    const uint16_t* mCurCount;
    const uint8_t* const* mCurStartPtr;
    };
  //}}}
  friend class iterator;

  cScanLine() {}
  //{{{
  ~cScanLine() {

    free (mCounts);
    free (mStartPtrs);
    free (mCoverage);
    }
  //}}}

  int16_t getY() const { return mLastY; }
  int16_t getBaseX() const { return mMinx;  }
  uint16_t getNumSpans() const { return mNumSpans; }
  int isReady (int16_t y) const { return mNumSpans && (y ^ mLastY); }

  //{{{
  void resetSpans() {

    mNumSpans = 0;

    mCurCount = mCounts;
    mCurStartPtr = mStartPtrs;

    mLastX = 0x7FFF;
    mLastY = 0x7FFF;
    }
  //}}}
  //{{{
  void reset (int16_t minx, int16_t maxx) {

    uint16_t maxLen = maxx - minx + 2;
    if (maxLen > mMaxlen) {
      // increase allocations
      mMaxlen = maxLen;

      free (mStartPtrs);
      free (mCounts);
      free (mCoverage);

      mCoverage = (uint8_t*)malloc (maxLen);
      mCounts = (uint16_t*)malloc (maxLen * 2);
      mStartPtrs = (uint8_t**)malloc (maxLen * 4);
      }

    mMinx = minx;
    resetSpans();
    }
  //}}}

  //{{{
  void addSpan (int16_t x, int16_t y, uint16_t num, uint16_t coverage) {

    x -= mMinx;

    memset (mCoverage + x, coverage, num);
    if (x == mLastX+1)
      (*mCurCount) += (uint16_t)num;
    else {
      *++mCurCount = (uint16_t)num;
      *++mCurStartPtr = mCoverage + x;
      mNumSpans++;
      }

    mLastX = x + num - 1;
    mLastY = y;
    }
  //}}}

private:
  int16_t mMinx = 0;
  uint16_t mMaxlen = 0;
  int16_t mLastX = 0x7FFF;
  int16_t mLastY = 0x7FFF;

  uint8_t* mCoverage = nullptr;

  uint8_t** mStartPtrs = nullptr;
  uint8_t** mCurStartPtr = nullptr;

  uint16_t* mCounts = nullptr;
  uint16_t* mCurCount = nullptr;

  uint16_t mNumSpans = 0;
  };
//}}}

// cLcd public
//{{{
cLcd::~cLcd() {

  gpioTerminate();

  vc_dispmanx_resource_delete (mSnapshot);
  vc_dispmanx_display_close (mDisplay);

  free (mFrameBuf);
  free (mPrevFrameBuf);
  free (mSpans);

  delete mOutline;
  delete mScanLine;
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

  if (mMode == eAll)
    // allocate single span
    mSpans = (sSpan*)malloc (sizeof(sSpan));
  else {
    // allocate prevFrameBuf and diff span list
    mPrevFrameBuf = mFrameBuf;
    mFrameBuf = (uint16_t*)aligned_alloc (128, getNumPixels() * 2);

    // allocate lotsa spans
    mSpans = (sSpan*)malloc (kMaxSpans * sizeof(sSpan));
    }

  if (mSnapshotEnabled) {
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
    uint32_t imageHandle;
    mSnapshot = vc_dispmanx_resource_create (VC_IMAGE_RGB565, getWidth(), getHeight(), &imageHandle);
    if (!mSnapshot) {
      //{{{  error return
      cLog::log (LOGERROR, "vc_dispmanx_resource_create failed");
      return false;
      }
      //}}}
    vc_dispmanx_rect_set (&mVcRect, 0, 0, getWidth(), getHeight());

    cLog::log (LOGINFO, "display %dx%d", mModeInfo.width, mModeInfo.height);
    }

  if (mTypeEnabled)
    setFont (getFreeSansBold(), getFreeSansBoldSize());

  mOutline = new cOutline();
  mScanLine = new cScanLine();

  for (unsigned i = 0; i < 256; i++)
    mGamma[i] = (uint8_t)(pow(double(i) / 255.0, 1.6) * 255.0);

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

  if (mSnapshotEnabled) {
    vc_dispmanx_snapshot (mDisplay, mSnapshot, DISPMANX_TRANSFORM_T(0));
    vc_dispmanx_resource_read_data (mSnapshot, &mVcRect, mFrameBuf, getWidth() * 2);
    }
  else
    cLog::log (LOGERROR, "snapahot not enabled");
  }
//}}}
//{{{
bool cLcd::present() {
// present update

  // make diff spans list
  double diffStartTime = timeUs();
  switch (mMode) {
    //{{{
    case eAll :
      mSpans->r = getRect();
      mSpans->lastScanRight = getWidth();
      mSpans->size = getNumPixels();
      mSpans->next = nullptr;
      mNumSpans = 1;
      break;
    //}}}
    //{{{
    case eSingle :
      mNumSpans = diffSingle (mSpans);
      break;
    //}}}
    //{{{
    case eCoarse :
      mNumSpans = diffCoarse (mSpans);
      merge (mSpans, kSpanMergeThreshold);
      break;
    //}}}
    //{{{
    case eExact :
      mNumSpans = diffExact (mSpans);
      merge (mSpans, kSpanMergeThreshold);
      break;
    //}}}
    }
  mDiffUs = int((timeUs() - diffStartTime) * 1000000.0);

  if (!mNumSpans) {// nothing changed
    mUpdateUs = 0;
    return false;
    }

  if ((mInfo == eOverlay) && (mMode != eAll)) // copy frameBuf to prevFrameBuf without overlays
    memcpy (mPrevFrameBuf, mFrameBuf, getNumPixels() * 2);

  // updateLcd with diff spans list
  double updateStartTime = timeUs();
  mUpdatePixels = updateLcd (mSpans);
  mUpdateUs = int((timeUs() - updateStartTime) * 1000000.0);

  if (mInfo == eOverlay) {
    // draw span and info overlays
    sSpan* it = mSpans;
    while (it) {
      rect (kGreen, 100, it->r);
      it = it->next;
      }
    text (kWhite, cPoint(0,0), 20, getPaddedInfoString());

    // update whole screen with overlays, its saved without them in prevFrameBuffer
    updateLcdAll();
    }

  cLog::log (LOGINFO1, getInfoString());

  if ((mMode != eAll) && (mInfo != eOverlay)) {
    // swap buffers
    auto temp = mPrevFrameBuf;
    mPrevFrameBuf = mFrameBuf;
    mFrameBuf = temp;
    }

  return true;
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

  if ((alpha > 0) && (p.x >= 0) && (p.y >= 0) && (p.x < getWidth()) && (p.y < getHeight())) {
    // clip opaque and offscreen
    if (alpha == 0xFF)
      // simple case - set frameBuf pixel to colour
      mFrameBuf[(p.y*getWidth()) + p.x] = colour;
    else {
      // get frameBuf background
      uint32_t background = mFrameBuf[(p.y*getWidth()) + p.x];

      // composite colour
      uint32_t foreground = colour;
      foreground = (foreground | (foreground << 16)) & 0x07e0f81f;
      background = (background | (background << 16)) & 0x07e0f81f;
      background += (((foreground - background) * ((alpha + 4) >> 3)) >> 5) & 0x07e0f81f;

      // set frameBuf pixel to background result
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
void cLcd::copy (const uint16_t* src, cRect& srcRect, const cPoint& dstPoint) {
// copy rect from src of same width,height

  for (int y = 0; y < srcRect.getHeight(); y++)
    memcpy (mFrameBuf + ((dstPoint.y + y) * getWidth()) + dstPoint.x,
                  src + ((srcRect.top + y) * getWidth()) + srcRect.left,
            srcRect.getWidth() * 2);
  }
//}}}
//{{{
//void cLcd::grad (const uint16_t colTL, const uint16_t colTR,
                 //const uint16_t  colBL, const uint16_t const uint16_t, const cRect& r) {

  //int32_t rTL = colTL.getR() << 16;
  //int32_t rTR = colTR.getR() << 16;
  //int32_t rBL = colBL.getR() << 16;
  //int32_t rBR = colBR.getR() << 16;

  //int32_t gTL = colTL.getG() << 16;
  //int32_t gTR = colTR.getG() << 16;
  //int32_t gBL = colBL.getG() << 16;
  //int32_t gBR = colBR.getG() << 16;

  //int32_t bTL = colTL.getB() << 16;
  //int32_t bTR = colTR.getB() << 16;
  //int32_t bBL = colBL.getB() << 16;
  //int32_t bBR = colBR.getB() << 16;

  //int32_t rl16 = rTL;
  //int32_t gl16 = gTL;
  //int32_t bl16 = bTL;
  //int32_t rGradl16 = (rBL - rTL) / r.getHeight();
  //int32_t gGradl16 = (gBL - gTL) / r.getHeight();
  //int32_t bGradl16 = (bBL - bTL) / r.getHeight();

  //int32_t rr16 = rTR;
  //int32_t gr16 = gTR;
  //int32_t br16 = bTR;
  //int32_t rGradr16 = (rBR - rTR) / r.getHeight();
  //int32_t gGradr16 = (gBR - gTR) / r.getHeight();
  //int32_t bGradr16 = (bBR - bTR) / r.getHeight();

  //auto dst = (uint16_t*)mBuffer + r.top * getWidth() + r.left;
  //for (uint16_t y = r.top; y < r.bottom; y++) {
    //int32_t rGradx16 = (rr16 - rl16) / r.getWidth();
    //int32_t gGradx16 = (gr16 - gl16) / r.getWidth();
    //int32_t bGradx16 = (br16 - bl16) / r.getWidth();

    //int32_t r16 = rl16;
    //int32_t g16 = gl16;
    //int32_t b16 = bl16;
    //for (uint16_t x = r.left; x < r.right; x++) {
      //*dst++ = (b16 >> 16) | ((g16 >> 11) & 0x07E0) | ((r16 >> 5) & 0xF800);
      //r16 += rGradx16;
      //g16 += gGradx16;
      //b16 += bGradx16;
      //}
    //dst += getWidth() - r.getWidth();

    //rl16 += rGradl16;
    //gl16 += gGradl16;
    //bl16 += bGradl16;

    //rr16 += rGradr16;
    //gr16 += gGradr16;
    //br16 += bGradr16;
    //}
  //}
//}}}
//{{{
int cLcd::text (const uint16_t colour, const cPoint& p, const int height, const string& str) {

  if (mTypeEnabled) {
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

  cLog::log (LOGERROR, "type not enabled");

  return 0;
  }
//}}}

//{{{  simple draw
//{{{
void cLcd::rect (const uint16_t colour, const cRect& r) {
// rect with right,bottom clip

  int16_t xmax = min (r.right, (int16_t)getWidth());
  int16_t ymax = min (r.bottom, (int16_t)getHeight());

  for (int16_t y = r.top; y < ymax; y++) {
    uint16_t* ptr = mFrameBuf + y*getWidth() + r.left;
    for (int16_t x = r.left; x < xmax; x++)
      *ptr++ = colour;
    }
  }
//}}}
//{{{
void cLcd::rect (const uint16_t colour, const uint8_t alpha, const cRect& r) {

  uint16_t xmax = min (r.right, (int16_t)getWidth());
  uint16_t ymax = min (r.bottom, (int16_t)getHeight());

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
//{{{  anti aliased draw
//{{{
void cLcd::aMoveTo (const cPointF& p) {
  mOutline->moveTo (int(p.x * 256.f), int(p.y * 256.f));
  }
//}}}
//{{{
void cLcd::aLineTo (const cPointF& p) {
  mOutline->lineTo (int(p.x * 256.f), int(p.y * 256.f));
  }
//}}}
//{{{
void cLcd::aWideLine (const cPointF& p1, const cPointF& p2, float width) {

  cPointF perp = (p2 - p1).perp() * width;
  aMoveTo (p1 + perp);
  aLineTo (p2 + perp);
  aLineTo (p2 - perp);
  aLineTo (p1 - perp);
  }
//}}}
//{{{
void cLcd::aPointedLine (const cPointF& p1, const cPointF& p2, float width) {

  cPointF perp = (p2 - p1).perp() * width;
  aMoveTo (p1 + perp);
  aLineTo (p2);
  aLineTo (p1 - perp);
  }
//}}}

//{{{
void cLcd::aEllipse (const cPointF& centre, const cPointF& radius, int steps) {

  // anticlockwise ellipse
  float angle = 0.f;
  float fstep = 360.f / steps;
  aMoveTo (centre + cPointF(radius.x, 0.f));

  angle += fstep;
  while (angle < 360.f) {
    auto radians = angle * 3.1415926f / 180.0f;
    aLineTo (centre + cPointF (cos(radians) * radius.x, sin(radians) * radius.y));
    angle += fstep;
    }
  }
//}}}
//{{{
void cLcd::aEllipseOutline (const cPointF& centre, const cPointF& radius, float width, int steps) {

  float angle = 0.f;
  float fstep = 360.f / steps;
  aMoveTo (centre + cPointF(radius.x, 0.f));

  // clockwise ellipse
  angle += fstep;
  while (angle < 360.f) {
    auto radians = angle * 3.1415926f / 180.0f;
    aLineTo (centre + cPointF (cos(radians) * radius.x, sin(radians) * radius.y));
    angle += fstep;
    }

  // anti clockwise ellipse
  aMoveTo (centre + cPointF(radius.x - width, 0.f));

  angle -= fstep;
  while (angle > 0.f) {
    auto radians = angle * 3.1415926f / 180.0f;
    aLineTo (centre + cPointF (cos(radians) * (radius.x - width), sin(radians) * (radius.y - width)));
    angle -= fstep;
    }
  }
//}}}

//{{{
void cLcd::aRender (const uint16_t colour, bool fillNonZero) {

  const sCell* const* sortedCells = mOutline->getSortedCells();
  uint32_t numCells = mOutline->getNumCells();
  if (!numCells)
    return;

  mScanLine->reset (mOutline->getMinx(), mOutline->getMaxx());

  int coverage = 0;
  const sCell* cell = *sortedCells++;
  while (true) {
    int x = cell->mPackedCoord & 0xFFFF;
    int y = cell->mPackedCoord >> 16;
    int packedCoord = cell->mPackedCoord;
    int area = cell->mArea;
    coverage += cell->mCoverage;

    // accumulate all start cells
    while ((cell = *sortedCells++) != 0) {
      if (cell->mPackedCoord != packedCoord)
        break;
      area += cell->mArea;
      coverage += cell->mCoverage;
      }

    if (area) {
      uint8_t alpha = calcAlpha ((coverage << 9) - area, fillNonZero);
      if (alpha) {
        if (mScanLine->isReady (y)) {
          renderScanLine (colour, mScanLine);
          mScanLine->resetSpans();
          }
        mScanLine->addSpan (x, y, 1, mGamma[alpha]);
        }
      x++;
      }

    if (!cell)
      break;

    if (int16_t(cell->mPackedCoord & 0xFFFF) > x) {
      uint8_t alpha = calcAlpha (coverage << 9, fillNonZero);
      if (alpha) {
        if (mScanLine->isReady (y)) {
           renderScanLine (colour, mScanLine);
           mScanLine->resetSpans();
           }
         mScanLine->addSpan (x, y, int16_t(cell->mPackedCoord & 0xFFFF) - x, mGamma[alpha]);
         }
      }
    }

  if (mScanLine->getNumSpans())
    renderScanLine (colour, mScanLine);
  }
//}}}
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

// cLcd protected
//{{{
uint32_t cLcd::updateLcdAll() {
// update all of lcd with single span

  sSpan span = { getRect(), getWidth(), getNumPixels(), nullptr };
  return updateLcd (&span);
  }
//}}}

// cLcd private
//{{{
string cLcd::getInfoString() {
// return info string for log display

  return dec(getUpdatePixels()) + "px took:" +
         dec(getUpdateUs()) + "uS " +
         dec(getNumSpans()) + "spn took:" +
         dec(getDiffUs()) + "uS";
  }
//}}}
//{{{
string cLcd::getPaddedInfoString() {
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
int cLcd::diffSingle (sSpan* spans) {
// return 1, if single bounding span is different, else 0

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
      return 0;

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
    return 0;
    }
    //}}}

foundTop:
  int maxX = -1;
  int maxY = getHeight() - 1;

  if (kCoarseDiff) {
    //{{{  coarse diff
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
    //}}}
  else {
    //{{{  fine diff
    scanline = mFrameBuf + (getHeight() - 1)*stride;
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
          maxX = x + 3 - (__builtin_clzll (diff) >> 4);
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
  spans->r.left = leftX;
  spans->r.right = rightX+1;
  spans->r.top = minY;
  spans->r.bottom = maxY+1;
  spans->lastScanRight = lastScanRight+1;
  spans->size = (spans->r.right - spans->r.left) * (spans->r.bottom - spans->r.top - 1) +
                (spans->lastScanRight - spans->r.left);
  spans->next = nullptr;

  return 1;
  }
//}}}
//{{{
int cLcd::diffCoarse (sSpan* spans) {
// return numSpans, 4pix (64bit) alignment

  int numSpans = 0;

  int y =  0;
  int yInc = 1;

  const int width64 = getWidth() >> 2;
  const int scanlineInc = getWidth() >> 2;

  sSpan* span = spans;
  uint64_t* scanline = (uint64_t*)(mFrameBuf + (y * getWidth()));
  uint64_t* prevFrameScanline = (uint64_t*)(mPrevFrameBuf + (y * getWidth()));
  while (y < getHeight()) {
    uint16_t* scanlineStart = (uint16_t*)scanline;

    for (int x = 0; x < width64;) {
      if (scanline[x] != prevFrameScanline[x]) {
        uint16_t* spanStart = (uint16_t*)(scanline + x) +
                              (__builtin_ctzll (scanline[x] ^ prevFrameScanline[x]) >> 4);
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
              spanEnd = (uint16_t*)(scanline + x) + 1 -
                                   (__builtin_clzll (scanline[x-1] ^ prevFrameScanline[x-1]) >> 4);
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
        if (numSpans > 0)
          span[-1].next = span;
        span->next = nullptr;

        span++;
        if (numSpans++ >= kMaxSpans) {
          //{{{  error return, could fake up whole screen
          cLog::log (LOGERROR, "too many spans");
          return numSpans;
          }
          //}}}
        }
      else
        ++x;
      }

    y += yInc;
    scanline += scanlineInc;
    prevFrameScanline += scanlineInc;
    }

  if (numSpans > 0)
    spans[numSpans-1].next = nullptr;

  return numSpans;
  }
//}}}
//{{{
int cLcd::diffExact (sSpan* spans) {
// return numSpans

  int numSpans = 0;

  int y =  0;
  int yInc = 1;

  sSpan* span = spans;
  uint16_t* scanline = mFrameBuf + y * getWidth();
  uint16_t* prevFrameScanline = mPrevFrameBuf + y * getWidth();
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
        //{{{  found start of span of different pixels on this scanline, find where it ends
        while (scanline < scanlineEnd) {
          if (*scanline++ != *prevFrameScanline++) {
            spanEnd = scanline;
            numConsecutiveUnchangedPixels = 0;
            }
          else {
            if (++numConsecutiveUnchangedPixels > kSpanExactThreshold)
              break;
            }
          }
        //}}}
        }
      else {
       //{{{  handle single last pixel on the row
       if (*scanline++ == *prevFrameScanline++)
         break;

       spanStart = scanline - 1;
       spanEnd = scanline;
       }
       //}}}

      span->r.left = spanStart - scanlineStart;
      span->r.right = spanEnd - scanlineStart;
      span->r.top = y;
      span->r.bottom = y+1;
      span->lastScanRight = span->r.right;
      span->size = spanEnd - spanStart;
      if (numSpans > 0)
        span[-1].next = span;
      span->next = nullptr;

      span++;
      if (numSpans++ >= kMaxSpans) {
        //{{{  error return, could fake up whole screen
        cLog::log (LOGERROR, "too many spans");
        return numSpans;
        }
        //}}}
      }

    y += yInc;
    }

  return numSpans;
  }
//}}}
//{{{
void cLcd::renderScanLine (const uint16_t colour, cScanLine* scanLine) {

  // clip top
  auto y = scanLine->getY();
  if (y < 0)
    return;

  // clip bottom
  if (y >= getHeight())
    return;

  int baseX = scanLine->getBaseX();
  uint16_t numSpans = scanLine->getNumSpans();

  cScanLine::iterator span (*scanLine);
  do {
    cPoint p (baseX + span.next(), y);
    auto coverage = (uint8_t*)span.getCoverage();

    // clip left
    int16_t numPix = span.getNumPix();
    if (p.x < 0) {
      numPix += p.x;
      if (numPix <= 0)
        continue;
      coverage -= p.x;
      p.x = 0;
      }

    // clip right
    if (p.x + numPix >= getWidth()) {
      numPix = getWidth() - p.x;
      if (numPix <= 0)
        continue;
      }

    uint16_t* ptr = mFrameBuf + (p.y * getWidth()) + p.x;
    for (uint16_t i = 0; i < numPix; i++) {
      // inline pix without clip and better ptr usage
      uint8_t alpha = *coverage++;
      if (alpha == 0xFF)
        // simple case - set frameBuf pixel to colour
        *ptr++ = colour;
      else {
        // get colour
        uint32_t foreground = colour;
        foreground = (foreground | (foreground << 16)) & 0x07e0f81f;

        // get frameBuf background, composite and write back
        uint32_t background = *ptr;
        background = (background | (background << 16)) & 0x07e0f81f;
        background += (((foreground - background) * ((alpha + 4) >> 3)) >> 5) & 0x07e0f81f;
        *ptr++ = background | (background >> 16);
        }
      }

    } while (--numSpans);
  }
//}}}

// cLcd private static
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
//{{{
uint8_t cLcd::calcAlpha (int area, bool fillNonZero) {

  int coverage = area >> 9;
  if (coverage < 0)
    coverage = -coverage;

  if (!fillNonZero) {
    coverage &= 0x1FF;
    if (coverage > 0x100)
      coverage = 0x200 - coverage;
    }

  if (coverage > 0xFF)
    coverage = 0xFF;

  return coverage;
  }
//}}}

// parallel 16bit classes
//{{{  cLcdTa7601 : public cLcd
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

  updateLcdAll();

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
    case e0: { // send spans
      for (sSpan* it = spans; it; it = it->next) {
        cRect& r = it->r;
        writeCommandData (0x45, r.left);     // GRAM V start address window
        writeCommandData (0x44, r.right-1);  // GRAM V   end address window
        writeCommandData (0x47, r.top);      // GRAM H start address window - even
        writeCommandData (0x46, r.bottom-1); // GRAM H   end address window - even

        writeCommandData (0x20, r.top);      // GRAM V start address
        writeCommandData (0x21, r.left);     // GRAM H start address
        writeCommand (0x22);                 // GRAM write

        uint16_t* ptr = mFrameBuf + (r.top * getWidth()) + r.left;
        for (int16_t y = r.top; y < r.bottom; y++) {
          for (int16_t x = r.left; x < r.right; x++)
            writeDataWord (*ptr++);
          ptr += getWidth() - r.getWidth();
          }

        numPixels += r.getNumPixels();
        }
      break;
      }

    //{{{
    case  e90: // send whole frame
      writeCommandData (0x45, 0x0000);          // GRAM H start address window = 0
      writeCommandData (0x44, kWidthTa7601-1);  // GRAM H   end address window = 320-1
      writeCommandData (0x47, 0x0000);          // GRAM V start address window = 0
      writeCommandData (0x46, kHeightTa7601-1); // GRAM V   end address window = 480-1
      writeCommandData (0x20, 0x0000);          // GRAM V start address
      writeCommandData (0x21, 0x0000);          // GRAM H start address
      writeCommand (0x22);

      // !!! simplify the back step at end of line !!!
      for (int x = 0; x < getWidth(); x++) {
        uint16_t* ptr = mFrameBuf + x;
        for (int y = 0; y < getHeight(); y++) {
          writeDataWord (*ptr);
          ptr += getWidth();
          }
        }

      numPixels = getNumPixels();
      break;
    //}}}
    //{{{
    case e180: // send whole frame - should use direction bits
      writeCommandData (0x45, 0x0000);          // GRAM H start address window = 0
      writeCommandData (0x44, kWidthTa7601-1);  // GRAM H   end address window = 320-1
      writeCommandData (0x47, 0x0000);          // GRAM V start address window = 0
      writeCommandData (0x46, kHeightTa7601-1); // GRAM V   end address window = 480-1
      writeCommandData (0x20, 0x0000);          // GRAM V start address
      writeCommandData (0x21, 0x0000);          // GRAM H start address
      writeCommand (0x22);

      // !!! simplify, can i just reverse scan !!!
      for (int y = 0; y < getHeight(); y++) {
        uint16_t* ptr = mFrameBuf + ((getHeight()-1-y) * getWidth());
        for (int x = 0; x < getWidth(); x++) {
          writeDataWord (*ptr);
          ptr--;
          }
        }

      numPixels = getNumPixels();
      break;
    //}}}
    //{{{
    case e270: // send whole frame
      writeCommandData (0x45, 0x0000);          // GRAM H start address window = 0
      writeCommandData (0x44, kWidthTa7601-1);  // GRAM H   end address window = 320-1
      writeCommandData (0x47, 0x0000);          // GRAM V start address window = 0
      writeCommandData (0x46, kHeightTa7601-1); // GRAM V   end address window = 480-1
      writeCommandData (0x20, 0x0000);          // GRAM V start address
      writeCommandData (0x21, 0x0000);          // GRAM H start address
      writeCommand (0x22);

      // !!! simplify the back step at aned of line !!!
      for (int x = 0; x < getWidth(); x++) {
        uint16_t* ptr = mFrameBuf + ((getHeight()-1) * getWidth()) + x;
        for (int y = 0; y < getHeight(); y++) {
          writeDataWord (*ptr);
          ptr -= getWidth();
          }
        }

      numPixels = getNumPixels();
      break;
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

  updateLcdAll();

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

// spi - data/command pin classes
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

  updateLcdAll();

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
  writeCommandData (0x36, getWidth()-1);
  writeCommandData (0x37, 0);
  writeCommandData (0x38, getHeight()-1);
  writeCommandData (0x39, 0);
  writeCommandData (0x20, 0);
  writeCommandData (0x21, 0);
  //}}}

  updateLcdAll();

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

// spi - no data/command pin classes
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
      writeCommandData (0x03, 0x1030); // Entry Mode - BGR, HV inc, vert write,
      break;
    case e90:
      writeCommandData (0x03, 0x1018); // Entry Mode - BGR, HV inc, vert write,
      break;
    case e180:
      writeCommandData (0x03, 0x1000); // Entry Mode - BGR, HV inc, vert write,
      break;
    case e270:
      writeCommandData (0x03, 0x1028); // Entry Mode - BGR, HV inc, vert write,
      break;
    }

  updateLcdAll();

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

  writeCommandData (0x0050, 0x0000);        // H GRAM start address
  writeCommandData (0x0051, kWidth9320-1);  // H GRAM end address
  writeCommandData (0x0052, 0x0000);        // V GRAM start address
  writeCommandData (0x0053, kHeight9320-1); // V GRAM end address

  int numPixels = 0;

  sSpan* it = spans;
  while (it) {
    uint16_t xstart;
    uint16_t ystart;
    uint16_t yinc;
    switch (mRotate) {
      default:
      //{{{
      case e0:
        xstart = it->r.left;
        ystart = it->r.top;
        yinc = 1;
        break;
      //}}}
      //{{{
      case e90:
        xstart = kHeight9320 - 1 - it->r.left;
        ystart = it->r.top;
        yinc = 1;
        break;
      //}}}
      //{{{
      case e180:
        xstart = kWidth9320 - 1 - it->r.left;
        ystart = kHeight9320 - 1 - it->r.top;
        yinc = -1;
        break;
      //}}}
      //{{{
      case e270:
        xstart = it->r.left;
        ystart = kWidth9320 - 1 - it->r.top;
        yinc = -1;
        break;
      //}}}
      }

    for (int y = it->r.top; y < it->r.bottom; y++) {
      if ((mRotate == e0) || (mRotate == e180)) {
        writeCommandData (0x20, xstart); // GRAM V start
        writeCommandData (0x21, ystart); // GRAM H start
        }
      else {
        writeCommandData (0x20, ystart); // GRAM V start
        writeCommandData (0x21, xstart); // GRAM H start
        }

      writeCommand (0x22); // send GRAM write
      uint16_t* src = mFrameBuf + (y * getWidth()) + it->r.left;
      // 2 header bytes, alignment for data bswap_16, send spi data from second header byte
      uint16_t* dst = dataHeaderBuf + 1;
      for (int i = 0; i < it->r.getWidth(); i++)
        *dst++ = bswap_16 (*src++);
      spiWrite (mSpiHandle, ((char*)(dataHeaderBuf))+1, (it->r.getWidth() * 2) + 1);

      ystart += yinc;
      numPixels += it->r.getWidth();
      }

    it = it->next;
    }

  return numPixels;
  }
//}}}
//}}}
