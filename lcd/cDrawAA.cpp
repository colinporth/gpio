// cDrawAA.cpp
#include "cDrawAA.h"
#include <cstring>
#include <math.h>

//#include "../shared/utils/utils.h"
//#include "../shared/utils/cLog.h"
//using namespace std;

// cDrawAA public
//{{{
cDrawAA::cDrawAA() {

  mNumCellsInBlock = 2048;
  mScanLine = new cScanLine();

  for (unsigned i = 0; i < 256; i++)
    mGamma[i] = (uint8_t)(pow(double(i) / 255.0, 1.6) * 255.0);

  init();
  }
//}}}
//{{{
cDrawAA::~cDrawAA() {

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

  delete mScanLine;
  }
//}}}

//{{{
void cDrawAA::moveTo (int32_t x, int32_t y) {

  //if (!mSortRequired)
  //  init();

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
void cDrawAA::lineTo (int32_t x, int32_t y) {

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

    addLine (mCurx, mCury, x, y);
    mCurx = x;
    mCury = y;
    mClosed = false;
    }
  }
//}}}
//{{{
void cDrawAA::render (const uint16_t colour, bool fillNonZero, uint16_t* frameBuf, uint16_t width, uint16_t height) {

  const sCell* const* sortedCells = getSortedCells();
  uint32_t numCells = getNumCells();
  if (!numCells)
    return;

  mScanLine->init (getMinx(), getMaxx());

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
          renderScanLine (colour, frameBuf, width, height);
          mScanLine->initSpans();
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
           renderScanLine (colour, frameBuf, width, height);
           mScanLine->initSpans();
           }
         mScanLine->addSpan (x, y, int16_t(cell->mPackedCoord & 0xFFFF) - x, mGamma[alpha]);
         }
      }
    }

  if (mScanLine->getNumSpans())
    renderScanLine (colour, frameBuf, width, height);

  // clear down for next time
  init();
  }
//}}}

// cDrawAA private
//{{{
void cDrawAA::init() {

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
const cDrawAA::sCell* const* cDrawAA::getSortedCells() {

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
void cDrawAA::addCurCell() {

  if (mCurCell.mArea | mCurCell.mCoverage) {
    if ((mNumCells % mNumCellsInBlock) == 0) {
      // use next block of sCells
      uint32_t block = mNumCells / mNumCellsInBlock;
      if (block >= mNumBlockOfCells) {
        // allocate new block of sCells
        mBlockOfCells = (sCell**)realloc (mBlockOfCells, (mNumBlockOfCells + 1) * sizeof(sCell*));
        mBlockOfCells[mNumBlockOfCells] = (sCell*)malloc (mNumCellsInBlock * sizeof(sCell));
        mNumBlockOfCells++;
        //cLog::log (LOGINFO, "allocated new blockOfCells %d of %d", block, mNumBlockOfCells);
        }
      mCurCellPtr = mBlockOfCells[block];
      }

    *mCurCellPtr++ = mCurCell;
    mNumCells++;
    }
  }
//}}}
//{{{
void cDrawAA::setCurCell (int16_t x, int16_t y) {

  if (mCurCell.mPackedCoord != (y << 16) + x) {
    addCurCell();
    mCurCell.set (x, y, 0, 0);
    }
 }
//}}}
//{{{
void cDrawAA::swapCells (cDrawAA::sCell** a, cDrawAA::sCell** b) {
  sCell* temp = *a;
  *a = *b;
  *b = temp;
  }
//}}}
//{{{
void cDrawAA::sortCells() {

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
void cDrawAA::qsortCells (cDrawAA::sCell** start, unsigned numCells) {

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

//{{{
void cDrawAA::addScanLine (int32_t ey, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {

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
void cDrawAA::addLine (int32_t x1, int32_t y1, int32_t x2, int32_t y2) {

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
    addScanLine (ey1, x1, fy1, x2, fy2);
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
  addScanLine (ey1, x1, fy1, x_from, first);

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
      addScanLine (ey1, x_from, 0x100 - first, x_to, first);
      x_from = x_to;

      ey1 += incr;
      setCurCell (x_from >> 8, ey1);
      }
    }

  addScanLine (ey1, x_from, 0x100 - first, x2, fy2);
  }
//}}}

//{{{
void cDrawAA::renderScanLine (const uint16_t colour, uint16_t* frameBuf, uint16_t width, uint16_t height) {

  cScanLine* scanLine = mScanLine;

  // clip top
  auto y = scanLine->getY();
  if (y < 0)
    return;

  // clip bottom
  if (y >= height)
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
    if (p.x + numPix >= width) {
      numPix = width - p.x;
      if (numPix <= 0)
        continue;
      }

    uint16_t* ptr = frameBuf + (p.y * width) + p.x;
    for (uint16_t i = 0; i < numPix; i++) {
      // inline pix without clip and better ptr usage
      uint8_t alpha = *coverage++;
      if (alpha == 0xFF)
        // simple case - set frameBuf pixel to colour
        *ptr++ = colour;
      else {
        // get colour
        uint32_t fore = colour;
        fore = (fore | (fore << 16)) & 0x07e0f81f;

        // get frameBuf back, composite and write back
        uint32_t back = *ptr;
        back = (back | (back << 16)) & 0x07e0f81f;
        back += (((fore - back) * ((alpha + 4) >> 3)) >> 5) & 0x07e0f81f;
        back |= back >> 16;
        *ptr++ = back;
        }
      }

    } while (--numSpans);
  }
//}}}

// cDrawAA private static
//{{{
uint8_t cDrawAA::calcAlpha (int area, bool fillNonZero) {

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

// cDrawAA::cScanline
//{{{
cDrawAA::cScanLine::~cScanLine() {

  free (mCounts);
  free (mStartPtrs);
  free (mCoverage);
  }
//}}}
//{{{
void cDrawAA::cScanLine::initSpans() {

  mNumSpans = 0;

  mCurCount = mCounts;
  mCurStartPtr = mStartPtrs;

  mLastX = 0x7FFF;
  mLastY = 0x7FFF;
  }
//}}}
//{{{
void cDrawAA::cScanLine::init (int16_t minx, int16_t maxx) {

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
  initSpans();
  }
//}}}
//{{{
void cDrawAA::cScanLine::addSpan (int16_t x, int16_t y, uint16_t num, uint16_t coverage) {

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
