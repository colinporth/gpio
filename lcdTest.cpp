// lcdTest.cpp
//{{{  includes
#include "cLcd.h"

#include <cstdint>
#include <string>
#include <vector>

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
  vector <string> argStrings;
  for (int i = 1; i < numArgs; i++)
    argStrings.push_back (args[i]);

  for (size_t i = 0; i < argStrings.size(); i++)
    if (argStrings[i] == "90") rotate = cLcd::e90;
    else if (argStrings[i] == "180") rotate = cLcd::e180;
    else if (argStrings[i] == "270") rotate = cLcd::e270;
    else if (argStrings[i] == "o") info = cLcd::eOverlay;
    else if (argStrings[i] == "a") mode = cLcd::eAll;
    else if (argStrings[i] == "s") mode = cLcd::eSingle;
    else if (argStrings[i] == "c") mode = cLcd::eCoarse;
    else if (argStrings[i] == "e") mode = cLcd::eExact;
    else if (argStrings[i] == "1") logLevel = LOGINFO1;
    else if (argStrings[i] == "2") logLevel = LOGINFO2;
    else if (argStrings[i] == "d") draw = true;
    else
      cLog::log (LOGERROR, "unrecognised option " + argStrings[i]);

  cLog::init (logLevel, false, "", "gpio");

  //cLcd* lcd = new cLcdIli9320 (rotate, info, mode);
  cLcd* lcd = new cLcdTa7601 (rotate, info, mode);
  if (!lcd->initialise())
    return 0;

  if (draw) {
    //{{{  draw test
    while (true) {
      int height = 8;
      while (height < 120) {
        lcd->grad (kBlack, kRed, kYellow, kWhite, lcd->getRect());

        lcd->ellipse (lcd->getRect().getCentre(), cPointF(height, height), 16);
        lcd->renderAA (kYellow, true);

        lcd->ellipseOutline (lcd->getRect().getCentre(), cPointF(height, height), 6, 16);
        lcd->renderAA (kBlue, true);

        cPoint point;
        for (char ch = 'A'; ch < 0x7f; ch++) {
          point.x = lcd->text (kWhite, point, height, string(1,ch));
          if (point.x > lcd->getWidth()) {
            point.x = 0;
            point.y += height;
            if (point.y > lcd->getHeight())
              break;
            }
          }

        lcd->present();

        lcd->setBacklightOn();
        lcd->delayUs (10000);
        height += 4;
        }
      }
    }
    //}}}
  else {
    // snapshot
    while (true) {
      lcd->snapshot();
      lcd->present();
      lcd->setBacklightOn();
      lcd->delayUs (5000);
      }
    }

  return 0;
  }
