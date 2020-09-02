// lcdTest.cpp
//{{{  includes
#include <cstdint>
#include <string>
#include <vector>

#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>

#include "cLcd.h"
#include "cScreen.h"

#include "../shared/utils/utils.h"
#include "../shared/utils/cLog.h"

using namespace std;
//}}}

int main (int numArgs, char* args[]) {

  vector <string> argStrings;
  for (int i = 1; i < numArgs; i++)
    argStrings.push_back (args[i]);

  cLog::init (LOGINFO, false, "", "gpio");

  int rotate = argStrings.empty() ? 270 : stoi (argStrings[0]);
  cLcd* lcd = new cLcdIli9320 (rotate);
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
  //{{{  display font
  int height = 8;
  while (height++ < 100) {
    int x = 0;
    int y = 0;
    lcd->clear (kMagenta);
    for (char ch = 'A'; ch < 0x7f; ch++) {
      x = lcd->text (kWhite, cPoint(x, y), height, string(1,ch));
      if (x > lcd->getWidth()) {
        x = 0;
        y += height;
        if (y > lcd->getHeight())
          break;
        }
      }
    lcd->text (kYellow, cPoint(0,0), 20, "Hello Colin");
    lcd->update();
    lcd->delayUs (16000);
    }
  //}}}

  cScreen screen (lcd->getWidth(), lcd->getHeight());
  lcd->clear (kOrange);

  int i = 0;
  while (true) {
    screen.snap();

    double startTime = lcd->time();
    cScreen::sDiffSpan* spans = screen.diffCoarse();
    if (spans) {
      int diffTook = int((lcd->time() - startTime) * 10000000);

      lcd->copy (screen.getBuf(), screen.getSize());
      //{{{  show pre merge in red
      //while (spans) {
        //int x = screen.getHeight() - spans->endY;
        //int y = spans->x;
        //int xlen = spans->endY - spans->y + 1;
        //int ylen = spans->endX - spans->x + 1;
        //lcd->rectOutline (kRed, x,y,xlen,ylen);
        //spans = spans
        //}
      //}}}
      spans = screen.merge (16);
      //{{{  show post merge in green
      while (spans) {
        lcd->rect (kYellow, 0x60, cRect(spans->x,spans->y, spans->endX,spans->endY));
        spans = spans->next;
        }
      //}}}
      lcd->text (kWhite, cPoint(0,0), 20, dec(i++) +
                                  " " + dec(lcd->getUpdateUs(),5) +
                                  " " + dec(screen.getNumDiffSpans(),5) +
                                  " " + dec((screen.getNumDiffPixels() * 100) / screen.getNumPixels(),3) +
                                  " " + dec(diffTook,5));
      lcd->update();
      }

    lcd->delayUs (10000);
    }

  return 0;
  }
