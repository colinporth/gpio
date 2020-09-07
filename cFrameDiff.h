// cFrameDiff.h
#pragma once
#include "cPointRect.h"

//{{{  sSpan - diff span list element
constexpr int kMaxSpans = 10000;
constexpr bool kCoarseDiff = true;
constexpr int kSpanExactThreshold = 8;
constexpr int kSpanMergeThreshold = 16;

struct sSpan {
  cRect r;

  uint16_t lastScanRight; // scanline bottom-1 can be partial, ends in lastScanRight.
  uint32_t size;

  sSpan* next;   // linked skip list in array for fast pruning
  };
//}}}

class cFrameDiff {
public:
  //{{{
  cFrameDiff (const int width, const int height) : mWidth(width), mHeight(height) {

    mPrevFrameBuf = (uint16_t*)aligned_alloc (128, width * height * 2);
    mSpans = (sSpan*)malloc (kMaxSpans * sizeof(sSpan));
    }
  //}}}
  //{{{
  ~cFrameDiff() {

    free (mPrevFrameBuf);
    free (mSpans);
    }
  //}}}

  int getNumSpans() { return mNumSpans; }

  uint16_t* swap (uint16_t* frameBuf);
  void copy (uint16_t* frameBuf);

  // diff
  sSpan* diffSingle (uint16_t* frameBuf);
  sSpan* diffExact (uint16_t* frameBuf);
  sSpan* diffCoarse (uint16_t* frameBuf);
  static sSpan* merge (sSpan* spans, int pixelThreshold);

private:
  static int coarseLinearDiff (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd);
  static int coarseLinearDiffBack (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd);

  const uint16_t mWidth;
  const uint16_t mHeight;
  //const eMode mMode = eSingle;

  uint16_t* mPrevFrameBuf = nullptr;
  sSpan* mSpans = nullptr;
  int mNumSpans = 0;
  };
