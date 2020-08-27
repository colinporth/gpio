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

  DISPMANX_MODEINFO_T display_info;
  int ret = vc_dispmanx_display_get_info (display, &display_info);
  if (ret) {
    //{{{  error return
    cLog::log (LOGERROR, "vc_dispmanx_display_get_info failed");
    return 0;
    }
    //}}}
  cLog::log (LOGINFO, "Primary display is %d x %d", display_info.width, display_info.height);

  uint32_t image_prt;
  DISPMANX_RESOURCE_HANDLE_T screen = vc_dispmanx_resource_create (VC_IMAGE_RGB565, 320, 480, &image_prt);
  if (!screen) {
    //{{{  error return
    cLog::log (LOGERROR, "vc_dispmanx_resource_create failed");
    return 0;
    }
    //}}}

  lcd->clear (kOrange);

  VC_RECT_T rect1;
  vc_dispmanx_rect_set (&rect1, 0, 0, 320, 480);
  char* screenBuf = (char*)malloc (320 * 480 * 2);

  while (true) {
    vc_dispmanx_snapshot (display, screen, DISPMANX_TRANSFORM_T(0));
    vc_dispmanx_resource_read_data (screen, &rect1, screenBuf, 320 * 2);
    lcd->copy ((uint16_t*)screenBuf, 320, 480);
    lcd->update();
    lcd->delayUs (40000);
    }

  ret = vc_dispmanx_resource_delete (screen);
  vc_dispmanx_display_close (display);

  return 0;
  }
