// cScreen.h
#pragma once
#include <cstdint>
#include <bcm_host.h>

class cScreen {
public:
  //{{{
  struct sDiffSpan {
    sDiffSpan* next;   // linked skip list in array for fast pruning

    uint16_t x;
    uint16_t endX;
    uint16_t y;
    uint16_t endY;

    // box of width [x, endX[ * [y, endY[, scanline endY-1 can be partial, ends in lastScanEndX.
    uint32_t size;
    uint16_t lastScanEndX;
    };
  //}}}

  cScreen (const int width, const int height);
  ~cScreen();

  constexpr int getWidth() { return mWidth; }
  constexpr int getHeight() { return mHeight; }
  constexpr int getNumPixels() { return mWidth * mHeight; }

  const uint16_t* getBuf() { return mBuf; }

  const int getNumDiffSpans() { return mNumDiffSpans; }
  const int getNumDiffPixels();

  void snap();

  sDiffSpan* diffSingle();
  sDiffSpan* diffCoarse();
  sDiffSpan* diffExact();
  sDiffSpan* merge (int pixelThreshold);

private:
  static int coarseLinearDiff (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd);
  static int coarseBackLinearDiff (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd);

  const int mWidth;
  const int mHeight;

  // screen buffers
  uint16_t* mBuf;
  uint16_t* mPrevBuf;

  // dispmanx
  DISPMANX_DISPLAY_HANDLE_T mDisplay;
  DISPMANX_MODEINFO_T mModeInfo;
  DISPMANX_RESOURCE_HANDLE_T mScreenGrab;
  uint32_t mImagePrt;
  VC_RECT_T mVcRect;

  // diff spans
  sDiffSpan* mDiffSpans;
  int mNumDiffSpans;
  };
