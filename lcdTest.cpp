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

int main (int numArgs, char* args[]) {

  vector <string> argStrings;
  for (int i = 1; i < numArgs; i++)
    argStrings.push_back (args[i]);

  cLog::init (LOGINFO, false, "", "gpio");

  int rotate = argStrings.empty() ? 0 : stoi (argStrings[0]);
  cLcd* lcd = new cLcdD51e5ta7601 (rotate);
  lcd->initialise();

  //{{{  fp0
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
  char* screenBuf = (char*)malloc (480 * 320 * 2);

  lcd->clear (kOrange);

  int i = 0;
  while (true) {
    vc_dispmanx_snapshot (display, screenGrab, DISPMANX_TRANSFORM_T(0));
    vc_dispmanx_resource_read_data (screenGrab, &vcRect, screenBuf, 480 * 2);
    lcd->copyRotate ((uint16_t*)screenBuf, 480, 320);
    lcd->text (kWhite, 0,0, 16, dec(i++,3));
    lcd->update();
    lcd->delayUs (16000);
    }

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
