// cFrameDiff.h - classes
#pragma once
#include "cPointRect.h"

// cFrameDiff - base class
class cFrameDiff {
public:
  cFrameDiff (const uint16_t width, const uint16_t height) : mWidth(width), mHeight(height){}
  virtual ~cFrameDiff();

  int getNumSpans() { return mNumSpans; }

  virtual uint16_t* swap (uint16_t* frameBuf);
  virtual void copy (uint16_t* frameBuf);

  // diff
  virtual sSpan* diff (uint16_t* frameBuf) = 0;

protected:
  void allocateResources();

  void merge (int pixelThreshold);

  const uint16_t mWidth;
  const uint16_t mHeight;

  uint16_t* mPrevFrameBuf = nullptr;
  sSpan* mSpans = nullptr;
  int mNumSpans = 0;
  };

//{{{
class cAllFrameDiff : public cFrameDiff {
// slightly fake version to return whole screen, allocate single wholeScreen sSpan
public:
  //{{{
  cAllFrameDiff (const uint16_t width, const uint16_t height) : cFrameDiff (width, height) {

    mSpans = (sSpan*)malloc (sizeof(sSpan));
    *mSpans = { { 0,0, (int16_t)width, (int16_t)height }, width, uint32_t(width * height), nullptr };
    }
  //}}}
  virtual ~cAllFrameDiff() {}

  virtual uint16_t* swap (uint16_t* frameBuf) { return frameBuf; }
  virtual void copy (uint16_t* frameBuf) {}
  virtual sSpan* diff (uint16_t* frameBuf) { return mSpans; };
  };
//}}}
//{{{
class cSingleFrameDiff : public cFrameDiff {
public:
  //{{{
  cSingleFrameDiff (const int width, const int height) : cFrameDiff (width, height) {
    allocateResources();
    }
  //}}}
  virtual ~cSingleFrameDiff() {}

  virtual sSpan* diff (uint16_t* frameBuf);

private:
  static int coarseLinearDiff (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd);
  static int coarseLinearDiffBack (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd);
  };
//}}}
//{{{
class cCoarseFrameDiff : public cFrameDiff {
public:
  //{{{
  cCoarseFrameDiff (const int width, const int height) : cFrameDiff (width, height) {
    allocateResources();
    }
  //}}}
  virtual ~cCoarseFrameDiff() {}

  virtual sSpan* diff (uint16_t* frameBuf);
  };
//}}}
//{{{
class cExactFrameDiff : public cFrameDiff {
public:
  //{{{
  cExactFrameDiff (const int width, const int height) : cFrameDiff (width, height) {
    allocateResources();
    }
  //}}}
  virtual ~cExactFrameDiff() {}

  virtual sSpan* diff (uint16_t* frameBuf);
  };
//}}}
