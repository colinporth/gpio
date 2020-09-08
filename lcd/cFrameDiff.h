// cFrameDiff.h - classes
#pragma once
#include "cPointRect.h"

// cFrameDiff - base class
class cFrameDiff {
public:
  cFrameDiff (const int width, const int height);
  virtual ~cFrameDiff();

  int getNumSpans() { return mNumSpans; }

  uint16_t* swap (uint16_t* frameBuf);
  void copy (uint16_t* frameBuf);

  // diff
  virtual sSpan* diff (uint16_t* frameBuf) = 0;

protected:
  void merge (int pixelThreshold);

  const uint16_t mWidth;
  const uint16_t mHeight;

  uint16_t* mPrevFrameBuf = nullptr;
  sSpan* mSpans = nullptr;
  int mNumSpans = 0;
  };

// cSingleFrameDiff
class cSingleFrameDiff : public cFrameDiff {
public:
  cSingleFrameDiff (const int width, const int height) : cFrameDiff (width, height) {}
  virtual ~cSingleFrameDiff() {}

  sSpan* diff (uint16_t* frameBuf);

private:
  static int coarseLinearDiff (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd);
  static int coarseLinearDiffBack (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd);
  };

// cCoarseFrameDiff
class cCoarseFrameDiff : public cFrameDiff {
public:
  cCoarseFrameDiff (const int width, const int height) : cFrameDiff (width, height) {}
  virtual ~cCoarseFrameDiff();
  sSpan* diff (uint16_t* frameBuf);
  };

// cExactFrameDiff
class cExactFrameDiff : public cFrameDiff {
public:
  cExactFrameDiff (const int width, const int height) : cFrameDiff (width, height) {}
  virtual ~cExactFrameDiff() {}

  sSpan* diff (uint16_t* frameBuf);
  };
