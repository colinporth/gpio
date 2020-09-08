// cFrameDiff.h
#pragma once
#include "cPointRect.h"

struct sSpan {
  cRect r;
  uint16_t lastScanRight; // scanline bottom-1 can be partial, ends in lastScanRight.
  uint32_t size;
  sSpan* next;   // linked skip list in array for fast pruning
  };

class cFrameDiff {
public:
  cFrameDiff (const int width, const int height);
  ~cFrameDiff();

  int getNumSpans() { return mNumSpans; }

  uint16_t* swap (uint16_t* frameBuf);
  void copy (uint16_t* frameBuf);

  // diff
  sSpan* single (uint16_t* frameBuf);
  sSpan* exact (uint16_t* frameBuf);
  sSpan* coarse (uint16_t* frameBuf);

private:
  void merge (int pixelThreshold);

  static int coarseLinearDiff (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd);
  static int coarseLinearDiffBack (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd);

  // vars
  const uint16_t mWidth;
  const uint16_t mHeight;

  uint16_t* mPrevFrameBuf = nullptr;
  sSpan* mSpans = nullptr;
  int mNumSpans = 0;
  };
