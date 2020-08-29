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

#define SWAPU32(x, y) { uint32_t tmp = x; x = y; y = tmp; }
//{{{
struct sSpan {
  uint16_t x;
  uint16_t endX;
  uint16_t y;
  uint16_t endY;
  uint16_t lastScanEndX;

  uint32_t size; // Specifies a box of width [x, endX[ * [y, endY[, where scanline endY-1 can be partial, and ends in lastScanEndX.

  sSpan* next; // Maintain a linked skip list inside the array for fast seek to next active element when pruning
  };
//}}}
sSpan* spans = (sSpan*)malloc ((480 * 320 / 2) * sizeof(sSpan));

//{{{
// Coarse diffing of two framebuffers with tight stride, 16 pixels at a time
// Finds the first changed pixel, coarse result aligned down to 8 pixels boundary
int coarseLinearDiff (uint16_t* framebuffer, uint16_t* prevFramebuffer, uint16_t* framebufferEnd) {

  uint16_t* endPtr;

  asm volatile(
    "mov r0, %[framebufferEnd]\n" // r0 <- pointer to end of current framebuffer
    "mov r1, %[framebuffer]\n"   // r1 <- current framebuffer
    "mov r2, %[prevFramebuffer]\n" // r2 <- framebuffer of previous frame

    "start_%=:\n"
      "pld [r1, #128]\n" // preload data caches for both current and previous framebuffers 128 bytes ahead of time
      "pld [r2, #128]\n"

      "ldmia r1!, {r3,r4,r5,r6}\n" // load 4x32-bit elements (8 pixels) of current framebuffer
      "ldmia r2!, {r7,r8,r9,r10}\n" // load corresponding 4x32-bit elements (8 pixels) of previous framebuffer
      "cmp r3, r7\n" // compare all 8 pixels if they are different
      "cmpeq r4, r8\n"
      "cmpeq r5, r9\n"
      "cmpeq r6, r10\n"
      "bne end_%=\n" // if we found a difference, we are done

      // Unroll once for another set of 4x32-bit elements. On Raspberry Pi Zero, data cache line is 32 bytes in size, so one iteration
      // of the loop computes a single data cache line, with preloads in place at the top.
      "ldmia r1!, {r3,r4,r5,r6}\n"
      "ldmia r2!, {r7,r8,r9,r10}\n"
      "cmp r3, r7\n"
      "cmpeq r4, r8\n"
      "cmpeq r5, r9\n"
      "cmpeq r6, r10\n"
      "bne end_%=\n" // if we found a difference, we are done

      "cmp r0, r1\n" // framebuffer == framebufferEnd? did we finish through the array?
      "bne start_%=\n"
      "b done_%=\n"

    "end_%=:\n"
      "sub r1, r1, #16\n" // ldmia r1! increments r1 after load, so subtract back the last increment in order to not shoot past the first changed pixels

    "done_%=:\n"
      "mov %[endPtr], r1\n" // output endPtr back to C code
      : [endPtr]"=r"(endPtr)
      : [framebuffer]"r"(framebuffer), [prevFramebuffer]"r"(prevFramebuffer), [framebufferEnd]"r"(framebufferEnd)
      : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "cc"
    );

  return endPtr - framebuffer;
  }
//}}}
//{{{
// Same as coarse_linear_diff, but finds the last changed pixel in linear order instead of first, i.e.
// Finds the last changed pixel, coarse result aligned up to 8 pixels boundary
int coarseBackwardsLinearDiff (uint16_t* framebuffer, uint16_t* prevFramebuffer, uint16_t* framebufferEnd) {

  uint16_t *endPtr;
  asm volatile(
    "mov r0, %[framebufferBegin]\n" // r0 <- pointer to beginning of current framebuffer
    "mov r1, %[framebuffer]\n"   // r1 <- current framebuffer (starting from end of framebuffer)
    "mov r2, %[prevFramebuffer]\n" // r2 <- framebuffer of previous frame (starting from end of framebuffer)

    "start_%=:\n"
      "pld [r1, #-128]\n" // preload data caches for both current and previous framebuffers 128 bytes ahead of time
      "pld [r2, #-128]\n"

      "ldmdb r1!, {r3,r4,r5,r6}\n" // load 4x32-bit elements (8 pixels) of current framebuffer
      "ldmdb r2!, {r7,r8,r9,r10}\n" // load corresponding 4x32-bit elements (8 pixels) of previous framebuffer
      "cmp r3, r7\n" // compare all 8 pixels if they are different
      "cmpeq r4, r8\n"
      "cmpeq r5, r9\n"
      "cmpeq r6, r10\n"
      "bne end_%=\n" // if we found a difference, we are done

      // Unroll once for another set of 4x32-bit elements. On Raspberry Pi Zero, data cache line is 32 bytes in size, so one iteration
      // of the loop computes a single data cache line, with preloads in place at the top.
      "ldmdb r1!, {r3,r4,r5,r6}\n"
      "ldmdb r2!, {r7,r8,r9,r10}\n"
      "cmp r3, r7\n"
      "cmpeq r4, r8\n"
      "cmpeq r5, r9\n"
      "cmpeq r6, r10\n"
      "bne end_%=\n" // if we found a difference, we are done

      "cmp r0, r1\n" // framebuffer == framebufferEnd? did we finish through the array?
      "bne start_%=\n"
      "b done_%=\n"

    "end_%=:\n"
      "add r1, r1, #16\n" // ldmdb r1! decrements r1 before load, so add back the last decrement in order to not shoot past the first changed pixels

    "done_%=:\n"
      "mov %[endPtr], r1\n" // output endPtr back to C code
      : [endPtr]"=r"(endPtr)
      : [framebuffer]"r"(framebufferEnd), [prevFramebuffer]"r"(prevFramebuffer+(framebufferEnd-framebuffer)), [framebufferBegin]"r"(framebuffer)
      : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "cc"
    );

  return endPtr - framebuffer;
  }
//}}}
//{{{
bool diffFramebuffersToSingleChangedRectangle (uint16_t* framebuffer, uint16_t* prevFramebuffer, sSpan*& head) {

  int minY = 0;
  int minX = -1;

  const int stride = 480; // Stride as uint16 elements.
  const int WidthAligned4 = (uint32_t)480 & ~3u;

  uint16_t* scanline = framebuffer;
  uint16_t* prevScanline = prevFramebuffer;

  bool framebufferSizeCompatibleWithCoarseDiff = true;
  if (framebufferSizeCompatibleWithCoarseDiff) {
    //{{{  coarse diff
    // Coarse diff computes a diff at 8 adjacent pixels at a time
    // returns the point to the 8-pixel aligned coordinate where the pixels began to differ.

    int numPixels = 480 * 320;
    int firstDiff = coarseLinearDiff (framebuffer, prevFramebuffer, framebuffer + numPixels);
    if (firstDiff == numPixels)
      return false; // No pixels changed, nothing to do.

    // Compute the precise diff position here.
    while (framebuffer[firstDiff] == prevFramebuffer[firstDiff])
      ++firstDiff;

    minX = firstDiff % 480;
    minY = firstDiff / 480;
    }
    //}}}
  else {
    //{{{  fine diff
    while (minY < 320) {
      int x = 0;
      // diff 4 pixels at a time
      for (; x < WidthAligned4; x += 4) {
        uint64_t diff = *(uint64_t*)(scanline+x) ^ *(uint64_t*)(prevScanline+x);
        if (diff) {
          minX = x + (__builtin_ctzll(diff) >> 4);
          goto found_top;
          }
        }

      // tail unaligned 0-3 pixels one by one
      for (; x < 480; ++x) {
        uint16_t diff = *(scanline+x) ^ *(prevScanline+x);
        if (diff) {
          minX = x;
          goto found_top;
          }
        }

      scanline += stride;
      prevScanline += stride;
      ++minY;
      }

    // No pixels changed, nothing to do.
    return false;
    }
    //}}}

found_top:
  //{{{  found top
  int maxX = -1;
  int maxY = 320-1;

  if (framebufferSizeCompatibleWithCoarseDiff) {
    int numPixels = 480 * 320;
    int firstDiff = coarseBackwardsLinearDiff (framebuffer, prevFramebuffer, framebuffer + numPixels);
    // Coarse diff computes a diff at 8 adjacent pixels at a time
    // returns the point to the 8-pixel aligned coordinate where the pixels began to differ.

    // Compute the precise diff position here.
    while (firstDiff > 0 && framebuffer[firstDiff] == prevFramebuffer[firstDiff])
      --firstDiff;
    maxX = firstDiff % 480;
    maxY = firstDiff / 480;
    }

  else {
    scanline = framebuffer + (320 - 1)*stride;
    // same scanline from previous frame, not preceding scanline
    prevScanline = prevFramebuffer + (320 - 1)*stride;

    while (maxY >= minY) {
      int x = 480-1;
      // tail unaligned 0-3 pixels one by one
      for (; x >= WidthAligned4; --x) {
        if (scanline[x] != prevScanline[x]) {
          maxX = x;
          goto found_bottom;
          }
        }

      // diff 4 pixels at a time
      x = x & ~3u;
      for(; x >= 0; x -= 4) {
        uint64_t diff = *(uint64_t*)(scanline+x) ^ *(uint64_t*)(prevScanline+x);
        if (diff) {
          maxX = x + 3 - (__builtin_clzll(diff) >> 4);
          goto found_bottom;
          }
        }

      scanline -= stride;
      prevScanline -= stride;
      --maxY;
      }
    }
  //}}}

found_bottom:
  //{{{  found bottom
  scanline = framebuffer + minY*stride;
  prevScanline = prevFramebuffer + minY*stride;

  int lastScanEndX = maxX;
  if (minX > maxX)
    SWAPU32 (minX, maxX);

  int leftX = 0;
  while (leftX < minX) {
    uint16_t* s = scanline + leftX;
    uint16_t* prevS = prevScanline + leftX;
    for (int y = minY; y <= maxY; ++y) {
      if (*s != *prevS)
        goto found_left;
      s += stride;
      prevS += stride;
      }

    ++leftX;
    }
  //}}}

found_left:
  //{{{  found left
  int rightX = 480-1;
  while (rightX > maxX) {
    uint16_t* s = scanline + rightX;
    uint16_t* prevS = prevScanline + rightX;
    for(int y = minY; y <= maxY; ++y) {
      if (*s != *prevS)
        goto found_right;
      s += stride;
      prevS += stride;
      }

    --rightX;
    }
  //}}}

found_right:
  head = spans;

  head->x = leftX;
  head->endX = rightX+1;
  head->lastScanEndX = lastScanEndX+1;
  head->y = minY;
  head->endY = maxY+1;

  head->size = (head->endX-head->x) * (head->endY-head->y-1) + (head->lastScanEndX - head->x);
  head->next = 0;

  return true;
  }
//}}}

// DiffFramebuffersToSingleChangedRectangle(framebuffer[0], framebuffer[1], head);

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
  //{{{  dispmanx
  bcm_host_init();
  DISPMANX_DISPLAY_HANDLE_T display = vc_dispmanx_display_open (0);
  if (!display) {
    //{{{  error return
    cLog::log (LOGERROR, "vc_dispmanx_display_open failed");
    return 0;
    }
    //}}}

  DISPMANX_MODEINFO_T modeInfo;
  if (vc_dispmanx_display_get_info (display, &modeInfo)) {
    //{{{  error return
    cLog::log (LOGERROR, "vc_dispmanx_display_get_info failed");
    return 0;
    }
    //}}}
  cLog::log (LOGINFO, "Primary display is %d x %d", modeInfo.width, modeInfo.height);

  uint32_t image_prt;
  DISPMANX_RESOURCE_HANDLE_T screenGrab = vc_dispmanx_resource_create (VC_IMAGE_RGB565, 480, 320, &image_prt);
  if (!screenGrab) {
    //{{{  error return
    cLog::log (LOGERROR, "vc_dispmanx_resource_create failed");
    return 0;
    }
    //}}}

  VC_RECT_T vcRect;
  vc_dispmanx_rect_set (&vcRect, 0, 0, 480, 320);
  //}}}

  char* screenBuf = (char*)malloc (480 * 320 * 2);
  char* lastScreenBuf = (char*)malloc (480 * 320 * 2);

  lcd->clear (kOrange);

  int i = 0;
  while (true) {
    vc_dispmanx_snapshot (display, screenGrab, DISPMANX_TRANSFORM_T(0));
    vc_dispmanx_resource_read_data (screenGrab, &vcRect, screenBuf, 480 * 2);

    double startTime = lcd->time();
    if (diffFramebuffersToSingleChangedRectangle ((uint16_t*)screenBuf, (uint16_t*)lastScreenBuf, spans)) {
      int took = int((lcd->time()-startTime) * 10000000);
      i++;
      lcd->copyRotate ((uint16_t*)screenBuf, 480, 320);
      lcd->text (kWhite, 0,0, 22, dec(i) + " " + dec (lcd->getUpdateUs(),5) + " " + dec(took,5));

      lcd->rectOutline (kRed, 320-spans->endY, spans->x,  spans->endY - spans->y + 1, spans->endX - spans->x + 1);
      lcd->update();

      auto temp = lastScreenBuf;
      lastScreenBuf = screenBuf;
      screenBuf = temp;
      }

    lcd->delayUs (10000);
    }

  free (screenBuf);
  free (lastScreenBuf);
  vc_dispmanx_resource_delete (screenGrab);
  vc_dispmanx_display_close (display);

  while (true) {
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

  return 0;
  }
