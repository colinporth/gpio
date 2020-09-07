// lcdTest.cpp
//{{{  includes
#include "cLcd.h"

#include <cstdint>
#include <string>

#include "../shared/utils/utils.h"
#include "../shared/utils/cLog.h"

using namespace std;
//}}}

int main (int numArgs, char* args[]) {

  bool draw = false;
  cLcd::eRotate rotate = cLcd::e0;
  cLcd::eInfo info = cLcd::eNone;
  cLcd::eMode mode = cLcd::eCoarse;
  eLogLevel logLevel = LOGINFO;

  // dumb command line option parser
  for (int argIndex = 1; argIndex < numArgs; argIndex++) {
    string str (args[argIndex]);
    if (str == "0") rotate = cLcd::e0;
    else if (str == "90") rotate = cLcd::e180;
    else if (str == "180") rotate = cLcd::e180;
    else if (str == "270") rotate = cLcd::e270;
    else if (str == "o") info = cLcd::eOverlay;
    else if (str == "a") mode = cLcd::eAll;
    else if (str == "s") mode = cLcd::eSingle;
    else if (str == "c") mode = cLcd::eCoarse;
    else if (str == "e") mode = cLcd::eExact;
    else if (str == "1") logLevel = LOGINFO1;
    else if (str == "2") logLevel = LOGINFO2;
    else if (str == "d") draw = true;
    else
      cLog::log (LOGERROR, "unrecognised option " + str);
    }

  cLog::init (logLevel, false, "", "gpio");

  //cLcd* lcd = new cLcdIli9320 (rotate, info, mode);
  cLcd* lcd = new cLcdTa7601 (rotate, info, mode);
  if (!lcd->initialise())
    return 0;

  while (true) {
    if (draw) {
      //{{{  draw test
      float height = 6.f;
      float maxHeight = 60.f;

      while (height < maxHeight) {
        lcd->snapshot();
        lcd->grad (kBlack, kRed, kYellow, kWhite, cRect (0,0, lcd->getWidth(), int(height)));

        lcd->ellipseAA (cPointF(height/2.f, height/2.f), cPointF(height/2.f, height/2.f), 16);
        lcd->renderAA (kYellow, true);

        lcd->ellipseOutlineAA (cPointF(height/2.f, height/2.f), cPointF(height/2.f, height/2.f), height / 4.f, 16);
        lcd->renderAA (kBlue, true);

        cPoint point (int(maxHeight), 0);
        for (char ch = 'A'; ch < 0x7f; ch++) {
          point.x = lcd->text (kWhite, point, height, string(1,ch));
          if (point.x > lcd->getWidth())
            break;
          }
        height += 1;

        lcd->present();
        lcd->setBacklightOn();
        lcd->delayUs (5000);
        }
      }
      //}}}
    else {
      // snapshot
      lcd->snapshot();
      lcd->present();
      lcd->setBacklightOn();
      lcd->delayUs (5000);
      }
    }

  return 0;
  }
