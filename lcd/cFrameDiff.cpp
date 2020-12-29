// cFrameDiff.cpp
#include "cFrameDiff.h"

#include "../../shared/utils/utils.h"
#include "../../shared/utils/cLog.h"

using namespace std;

//#define COARSE_DIFF_ALLOWED

constexpr int kMaxSpans = 10000;
constexpr int kSpanExactThreshold = 8;
constexpr int kSpanMergeThreshold = 16;

// cFrameDiff public
//{{{
cFrameDiff::~cFrameDiff() {

  free (mPrevFrameBuf);
  free (mSpans);
  }
//}}}

//{{{
uint16_t* cFrameDiff::swap (uint16_t* frameBuf) {
// just swap pointers with new frameBuf

  uint16_t* temp = mPrevFrameBuf;
  mPrevFrameBuf = frameBuf;
  return temp;
  }
//}}}
//{{{
void cFrameDiff::copy (uint16_t* frameBuf) {
// copy from frameBuf

  memcpy (mPrevFrameBuf, frameBuf, mWidth * mHeight * 2);
  }
//}}}

// cFrameDiff protected
//{{{
void cFrameDiff::allocateResources() {

  mPrevFrameBuf = (uint16_t*)aligned_alloc (128, mWidth * mHeight * 2);
  mSpans = (sSpan*)malloc (kMaxSpans * sizeof(sSpan));
  }
//}}}
//{{{
void cFrameDiff::merge (int pixelThreshold) {
// !!!! need to recalc mNumSpans !!!!

  for (sSpan* i = mSpans; i; i = i->next) {
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
  }
//}}}

// cSingleFrameDiff
//{{{
sSpan* cSingleFrameDiff::diff (uint16_t* frameBuf) {
// return 1, if single bounding span is different, else 0

  sSpan* spans = mSpans;
  mNumSpans = 0;

  int minY = 0;
  int minX = -1;

  // stride as uint16 elements.
  const int stride = mWidth;
  const int widthAligned4 = (uint32_t)mWidth & ~3u;

  uint16_t* scanline = frameBuf;
  uint16_t* prevFrameScanline = mPrevFrameBuf;

  #ifdef COARSE_DIFF_ALLOWED
    //{{{  coarse diff
    {
    // Coarse diff computes a diff at 8 adjacent pixels at a time
    // returns the point to the 8-pixel aligned coordinate where the pixels began to differ.

    int numPixels = mWidth * mHeight;
    int firstDiff = coarseLinearDiff (frameBuf, mPrevFrameBuf, frameBuf + numPixels);
    if (firstDiff == numPixels)
      return nullptr;

    // Compute the precise diff position here.
    while (frameBuf[firstDiff] == mPrevFrameBuf[firstDiff])
      ++firstDiff;

    minX = firstDiff % mWidth;
    minY = firstDiff / mWidth;
    }
    //}}}
  #else
    //{{{  fine diff
    {
    while (minY < mHeight) {
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
      for (; x < mWidth; ++x) {
        uint16_t diff = *(scanline + x) ^ *(prevFrameScanline + x);
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
    return nullptr;
    }
    //}}}
  #endif

foundTop:
  int maxX = -1;
  int maxY = mHeight - 1;

  #ifdef COARSE_DIFF_ALLOWED
    //{{{  coarse diff
    {
    int numPixels = mWidth * mHeight;

    // coarse diff computes a diff at 8 adjacent pixels at a time
    // returns the point to the 8-pixel aligned coordinate where the pixels began to differ.
    int firstDiff = coarseLinearDiffBack (frameBuf, mPrevFrameBuf, frameBuf + numPixels);

    // compute the precise diff position here.
    while (firstDiff > 0 && frameBuf[firstDiff] == mPrevFrameBuf[firstDiff])
      --firstDiff;

    maxX = firstDiff % mWidth;
    maxY = firstDiff / mWidth;
    }
    //}}}
  #else
    //{{{  fine diff
    {
    scanline = frameBuf + (mHeight-1) * stride;
    prevFrameScanline = mPrevFrameBuf + (mHeight-1) * stride;

    while (maxY >= minY) {
      int x = mWidth-1;
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
  #endif

foundBottom:
  //{{{  found bottom
  scanline = frameBuf + minY*stride;
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
  int rightX = mWidth-1;
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

  mNumSpans = 1;
  return mSpans;
  }
//}}}
#ifdef COARSE_DIFF_ALLOWED
//{{{
int cSingleFrameDiff::coarseLinearDiff (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd) {
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
int cSingleFrameDiff::coarseLinearDiffBack (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd) {
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
#endif

// cCoarseFrameDiff
//{{{
sSpan* cCoarseFrameDiff::diff (uint16_t* frameBuf) {
// return numSpans, 4pix (64bit) alignment

  sSpan* spans = mSpans;
  int numSpans = 0;

  int y =  0;
  int yInc = 1;

  const int width64 = mWidth >> 2;
  const int scanlineInc = mWidth >> 2;

  sSpan* span = spans;
  uint64_t* scanline = (uint64_t*)(frameBuf + (y * mWidth));
  uint64_t* prevFrameScanline = (uint64_t*)(mPrevFrameBuf + (y * mWidth));
  while (y < mHeight) {
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
            spanEnd = scanlineStart + mWidth;
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
          return nullptr;
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

  mNumSpans = numSpans;
  merge (kSpanMergeThreshold);
  return numSpans > 0 ? mSpans : nullptr;
  }
//}}}

// cExactFrameDiff
//{{{
sSpan* cExactFrameDiff::diff (uint16_t* frameBuf) {
// return numSpans

  //constexpr uint32_t kDiffMask = 0xFFFFFFFF;  // all bits
  //constexpr uint32_t kDiffMask = 0xf79ef79e;  // top 4 bits
  constexpr uint32_t kDiffMask = 0xe71ce71c;  // top 3 bits
  //constexpr uint32_t kDiffMask = 0xc618c618;  // top 2 bits

  sSpan* spans = mSpans;
  int numSpans = 0;

  int y =  0;
  int yInc = 1;

  sSpan* span = spans;
  uint16_t* scanline = frameBuf + y * mWidth;
  uint16_t* prevFrameScanline = mPrevFrameBuf + y * mWidth;
  while (y < mHeight) {
    uint16_t* scanlineStart = scanline;
    uint16_t* scanlineEnd = scanline + mWidth;
    while (scanline < scanlineEnd) {
      uint16_t* spanStart;
      uint16_t* spanEnd;
      int numConsecutiveUnchangedPixels = 0;
      if (scanline + 1 < scanlineEnd) {
        uint32_t diff = ((*(uint32_t*)scanline) ^ (*(uint32_t*)prevFrameScanline)) & kDiffMask;
        scanline += 2;
        prevFrameScanline += 2;

        if (!diff) // both 1st,2nd pix same
          continue;
        if (diff & 0xFFFF) {
          //{{{  1st pix diff
          spanStart = scanline - 2;

          if (diff & 0xFFFF0000) // 2nd pix diff
            spanEnd = scanline;

          else {
            spanEnd = scanline - 1;
            numConsecutiveUnchangedPixels = 1;
            }
          }
          //}}}
        else {
          //{{{  only 2nd pix diff
          spanStart = scanline - 1;
          spanEnd = scanline;
          }
          //}}}

        //{{{  start of span diff pix, find end
        while (scanline < scanlineEnd) {
          uint32_t diff = ((*(uint32_t*)scanline++) ^ (*(uint32_t*)prevFrameScanline++)) & kDiffMask;
          if (diff) {
            spanEnd = scanline;
            numConsecutiveUnchangedPixels = 0;
            }
          else if (++numConsecutiveUnchangedPixels > kSpanExactThreshold)
            break;
          }
        //}}}
        }
      else {
       //{{{  handle single last pix on the row
       uint32_t diff = ((*(uint32_t*)scanline++) ^ (*(uint32_t*)prevFrameScanline++)) & kDiffMask;
       if (!diff)
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
        return nullptr;
        }
        //}}}
      }
    y += yInc;
    }

  mNumSpans = numSpans;
  merge (kSpanMergeThreshold);
  return numSpans > 0 ? mSpans : nullptr;
  }
//}}}
