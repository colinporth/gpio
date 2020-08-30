// lcdTest.cpp
//{{{  includes
#include <cstdint>
#include <string>
#include <vector>

#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <bcm_host.h>

#include "cLcd.h"

#include "../shared/utils/utils.h"
#include "../shared/utils/cLog.h"

using namespace std;
//}}}
constexpr bool kCoarseDiff = true;

//{{{
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

  //{{{
  cScreen (const int width, const int height) : mWidth(width), mHeight(height) {

    mBuf = (uint16_t*)aligned_alloc (width * height * 2, 32);
    mPrevBuf = (uint16_t*)aligned_alloc (width * height * 2, 32);

    // large possible max size
    mDiffSpans = (sDiffSpan*)malloc ((width * height / 2) * sizeof(sDiffSpan));

    bcm_host_init();

    mDisplay = vc_dispmanx_display_open (0);
    if (!mDisplay) {
      // error return
      cLog::log (LOGERROR, "vc_dispmanx_display_open failed");
      return;
      }

    if (vc_dispmanx_display_get_info (mDisplay, &mModeInfo)) {
      // error return
      cLog::log (LOGERROR, "vc_dispmanx_display_get_info failed");
      return;
      }
    cLog::log (LOGINFO, "Primary display is %d x %d", mModeInfo.width, mModeInfo.height);

    mScreenGrab = vc_dispmanx_resource_create (VC_IMAGE_RGB565, width, height, &mImagePrt);
    if (!mScreenGrab) {
      // error return
      cLog::log (LOGERROR, "vc_dispmanx_resource_create failed");
      return;
      }

    vc_dispmanx_rect_set (&mVcRect, 0, 0, width, height);
    }
  //}}}
  //{{{
  ~cScreen() {
    vc_dispmanx_resource_delete (mScreenGrab);
    vc_dispmanx_display_close (mDisplay);
    free (mBuf);
    free (mPrevBuf);
    }
  //}}}

  constexpr int getWidth() { return mWidth; }
  constexpr int getHeight() { return mHeight; }
  const uint16_t* getBuf() { return mBuf; }

  //{{{
  void snap() {

    auto temp = mPrevBuf;
    mPrevBuf = mBuf;
    mBuf = temp;

    vc_dispmanx_snapshot (mDisplay, mScreenGrab, DISPMANX_TRANSFORM_T(0));
    vc_dispmanx_resource_read_data (mScreenGrab, &mVcRect, mBuf, getWidth() * 2);
    }
  //}}}

  #define SPAN_MERGE_THRESHOLD 8
  #define SWAPU32(x, y) { uint32_t tmp = x; x = y; y = tmp; }
  //{{{
  sDiffSpan* diff (bool singleSpan, bool mergeSpans) {

    if (singleSpan) {
      //{{{  single span
      int minY = 0;
      int minX = -1;

      const int stride = getWidth(); // Stride as uint16 elements.
      const int widthAligned4 = (uint32_t)getWidth() & ~3u;

      uint16_t* scanline = mBuf;
      uint16_t* prevFrameScanline = mPrevBuf;

      if (kCoarseDiff) {
        //{{{  coarse diff
        // Coarse diff computes a diff at 8 adjacent pixels at a time
        // returns the point to the 8-pixel aligned coordinate where the pixels began to differ.

        int numPixels = getWidth() * getHeight();
        int firstDiff = coarseLinearDiff (mBuf, mPrevBuf, mBuf + numPixels);
        if (firstDiff == numPixels)
          return nullptr; // No pixels changed, nothing to do.

        // Compute the precise diff position here.
        while (mBuf[firstDiff] == mPrevBuf[firstDiff])
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
        return nullptr;
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
        int firstDiff = coarseBackLinearDiff (mBuf, mPrevBuf, mBuf + numPixels);

        // compute the precise diff position here.
        while (firstDiff > 0 && mBuf[firstDiff] == mPrevBuf[firstDiff])
          --firstDiff;
        maxX = firstDiff % getWidth();
        maxY = firstDiff / getWidth();
        }

      else {
        scanline = mBuf + (getHeight() - 1)*stride;
        // same scanline from previous frame, not preceding scanline
        prevFrameScanline = mPrevBuf + (getHeight() - 1)*stride;

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
      scanline = mBuf + minY*stride;
      prevFrameScanline = mPrevBuf + minY*stride;

      int lastScanEndX = maxX;
      if (minX > maxX)
        SWAPU32 (minX, maxX);

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
      sDiffSpan* head = mDiffSpans;

      head->x = leftX;
      head->endX = rightX+1;
      head->lastScanEndX = lastScanEndX+1;
      head->y = minY;
      head->endY = maxY+1;

      head->size = (head->endX-head->x) * (head->endY-head->y-1) + (head->lastScanEndX - head->x);
      head->next = 0;

      return mDiffSpans;
      }
      //}}}

    else {
      int y =  0;
      int yInc = 1;
      int numDiffSpans = 0;

      if (kCoarseDiff) {
        //{{{  coarse diff
        uint64_t* scanline = (uint64_t*)(mBuf + y * getWidth());
        uint64_t* prevFrameScanline = (uint64_t*)(mPrevBuf + y * getWidth());

        const int width64 = getWidth() >> 2;
        int scanlineInc = getWidth() >> 2;

        sDiffSpan* span = mDiffSpans;
        while (y < getHeight()) {
          uint16_t* scanlineStart = (uint16_t*)scanline;

          for (int x = 0; x < width64;) {
            if (scanline[x] != prevFrameScanline[x]) {
              uint16_t* spanStart = (uint16_t*)(scanline + x) + (__builtin_ctzll (scanline[x] ^ prevFrameScanline[x]) >> 4);
              ++x;

             //{{{  We've found a start of a span of different pixels on this scanline, now find where this span ends
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
             //}}}

              // Submit the span update task
              span->x = spanStart - scanlineStart;
              span->endX = span->lastScanEndX = spanEnd - scanlineStart;
              span->y = y;
              span->endY = y+1;
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
          mDiffSpans[numDiffSpans-1].next = 0;
        }
        //}}}
      else {
        //{{{  exact diff
        uint16_t* scanline = mBuf + y * getWidth();
        uint16_t* prevFrameScanline = mPrevBuf + y * getWidth();

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
                  if (++numConsecutiveUnchangedPixels > SPAN_MERGE_THRESHOLD)
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

            // Submit the span update task
            sDiffSpan* span = mDiffSpans + numDiffSpans;
            span->x = spanStart - scanlineStart;
            span->endX = span->lastScanEndX = spanEnd - scanlineStart;
            span->y = y;
            span->endY = y+1;
            span->size = spanEnd - spanStart;
            if (numDiffSpans > 0)
              span[-1].next = span;
            span->next = 0;
            ++numDiffSpans;
            }

          y += yInc;
          scanline += scanlineEndInc;
          prevFrameScanline += scanlineEndInc;
          }
        }
        //}}}

      if (numDiffSpans == 0)
        return nullptr;
      else if (mergeSpans) {
        //{{{  merge spans
        for (sDiffSpan* i = mDiffSpans; i; i = i->next) {
          sDiffSpan* prev = i;
          for (sDiffSpan* j = i->next; j; j = j->next) {
            // If the spans i and j are vertically apart, don't attempt to merge span i any further
            // since all spans >= j will also be farther vertically apart.
            // (the list is nondecreasing with respect to Span::y)
            if (j->y > i->endY)
              break;

            // Merge the spans i and j, and figure out the wastage of doing so
            int x = std::min (i->x, j->x);
            int y = std::min (i->y, j->y);
            int endX = std::max (i->endX, j->endX);
            int endY = std::max (i->endY, j->endY);
            int lastScanEndX = (endY > i->endY) ?
                                 j->lastScanEndX : ((endY > j->endY) ?
                                   i->lastScanEndX : std::max (i->lastScanEndX, j->lastScanEndX));
            int newSize = (endX - x) * (endY - y - 1) + (lastScanEndX - x);
            int wastedPixels = newSize - i->size - j->size;
            if (wastedPixels <= SPAN_MERGE_THRESHOLD) {
              i->x = x;
              i->y = y;
              i->endX = endX;
              i->endY = endY;
              i->lastScanEndX = lastScanEndX;
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

      return mDiffSpans;
      }
    }
  //}}}

private:
  //{{{
  static int coarseLinearDiff (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd) {
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
  static int coarseBackLinearDiff (uint16_t* frameBuf, uint16_t* prevFrameBuf, uint16_t* frameBufEnd) {
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
  };
//}}}

int main (int numArgs, char* args[]) {

  vector <string> argStrings;
  for (int i = 1; i < numArgs; i++)
    argStrings.push_back (args[i]);

  cLog::init (LOGINFO, false, "", "gpio");

  int rotate = argStrings.empty() ? 0 : stoi (argStrings[0]);
  cLcd* lcd = new cLcdTa7601 (rotate);
  lcd->initialise();

  //{{{  fb0
  int fbfd = open ("/dev/fb0", O_RDWR);
  if (fbfd == -1) {
    //{{{  error return
    cLog::log (LOGERROR, "fb0 open failed");
    return 0;
    }
    //}}}

  struct fb_fix_screeninfo finfo;
  if (ioctl (fbfd, FBIOGET_FSCREENINFO, &finfo)) {
    //{{{  error return
    cLog::log (LOGERROR, "fb0 get fscreeninfo failed");
    return 0;
    }
    //}}}

  struct fb_var_screeninfo vinfo;
  if (ioctl (fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
    //{{{  error return
    cLog::log (LOGERROR, "fb0 get vscreeninfo failed");
    return 0;
    }
    //}}}

  cLog::log (LOGINFO, "fb0 %dx%d %dbps", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

  //char* fbp = (char*)mmap (0, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
  //if (fbp <= 0) {
  //  return 0;
  //munmap (fbp, finfo.smem_len);
  //close (fbfd);
  //}}}

  cScreen screen (480,320);
  lcd->clear (kOrange);

  int i = 0;
  while (true) {
    screen.snap();

    double startTime = lcd->time();
    cScreen::sDiffSpan* diffSpans = screen.diff (false, true);
    if (diffSpans) {
      int diffTook = int((lcd->time() - startTime) * 10000000);

      lcd->copyRotate (screen.getBuf(), screen.getWidth(), screen.getHeight());
      lcd->text (kWhite, 0,0, 22, dec(i++) + " upd:" + dec(lcd->getUpdateUs(),5) + " diff:" + dec(diffTook,5));

      while (diffSpans) {
        int x = 320 - diffSpans->endY;
        int y = diffSpans->x;
        int xlen = diffSpans->endY - diffSpans->y + 1;
        int ylen = diffSpans->endX - diffSpans->x + 1;
        lcd->rectOutline (kGreen, x,y,xlen,ylen);
        diffSpans = diffSpans->next;
        }

      lcd->update();
      }

    lcd->delayUs (10000);
    }

  while (true) {
    //{{{  display font
    int height = 8;
    while (height++ < lcd->getHeight()) {
      int x = 0;
      int y = 0;
      lcd->clear (kMagenta);
      for (char ch = 'A'; ch < 0x7f; ch++) {
        x = lcd->text (kWhite, x, y, height, string(1,ch));
        if (x > lcd->getWidth()) {
          x = 0;
          y += height;
          if (y > lcd->getHeight())
            break;
          }
        }
      lcd->text (kYellow, 0,0, 20, "Hello Colin");
      lcd->update();
      lcd->delayUs (16000);
      }
    }
    //}}}

  return 0;
  }
