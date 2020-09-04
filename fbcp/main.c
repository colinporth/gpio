//{{{  includers
#include <stdio.h>
#include <syslog.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <bcm_host.h>
//}}}

int process() {
  bcm_host_init();
  DISPMANX_DISPLAY_HANDLE_T display = vc_dispmanx_display_open(0);
  if (!display) {
    //{{{  error, log, return
    syslog(LOG_ERR, "Unable to open primary display");
    return -1;
    }
    //}}}

  DISPMANX_MODEINFO_T screen_info;
  int ret = vc_dispmanx_display_get_info (display, &display_info);
  if (ret) {
    //{{{  error, log, return
    syslog (LOG_ERR, "Unable to get primary display information");
    return -1;
    }
    //}}}
  syslog(LOG_INFO, "Primary display is %d x %d", display_info.width, display_info.height);


  int fbfd = open("/dev/fb1", O_RDWR);
  if (fbfd == -1) {
    //{{{  error, log, return
    syslog (LOG_ERR, "Unable to open secondary display");
    return -1;
    }
    //}}}

  struct fb_fix_screeninfo finfo;
  if (ioctl (fbfd, FBIOGET_FSCREENINFO, &finfo)) {
    //{{{  error, log, return
    syslog (LOG_ERR, "Unable to get secondary display information");
    return -1;
    }
    //}}}

  struct fb_var_screeninfo vinfo;
  if (ioctl (fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
    //{{{  error, log, return
    syslog (LOG_ERR, "Unable to get secondary display information");
    return -1;
    }
    //}}}

  syslog (LOG_INFO, "Second display is %d x %d %dbps\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

  uint32_t imageHandle;
  DISPMANX_RESOURCE_HANDLE_T screen_resource = vc_dispmanx_resource_create (VC_IMAGE_RGB565, vinfo.xres, vinfo.yres, &imageHandle);
  if (!screen_resource) {
    //{{{  error, log, return
    syslog (LOG_ERR, "Unable to create screen buffer");
    close (fbfd);

    vc_dispmanx_display_close (display);
    return -1;
    }
    //}}}
  char* fbp = (char*)mmap (0, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
  if (fbp <= 0) {
    //{{{  error, log, return
    syslog (LOG_ERR, "Unable to create memory mapping");
    close (fbfd);

    ret = vc_dispmanx_resource_delete (screen_resource);
    vc_dispmanx_display_close (display);

    return -1;
    }
    //}}}

  VC_RECT_T rect1;
  vc_dispmanx_rect_set (&rect1, 0, 0, vinfo.xres, vinfo.yres);

  while (1) {
    ret = vc_dispmanx_snapshot (display, screen_resource, 0);
    vc_dispmanx_resource_read_data (screen_resource, &rect1, fbp, vinfo.xres * vinfo.bits_per_pixel / 8);
    usleep (25 * 1000);
    }

  munmap (fbp, finfo.smem_len);
  close (fbfd);
  ret = vc_dispmanx_resource_delete (screen_resource);
  vc_dispmanx_display_close (display);
  }

int main (int argc, char **argv) {

  setlogmask(LOG_UPTO(LOG_DEBUG));
  openlog("fbcp", LOG_NDELAY | LOG_PID, LOG_USER);

  return process();
  }
