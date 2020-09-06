// lcdTest.cpp
//{{{  includes
#include "cLcd.h"

#include <vector>

#include "../shared/utils/utils.h"
#include "../shared/utils/cLog.h"

using namespace std;
//}}}

int main (int numArgs, char* args[]) {

  vector <string> argStrings;
  for (int i = 1; i < numArgs; i++)
    argStrings.push_back (args[i]);

  bool fontTest = false;
  cLcd::eRotate rotate = cLcd::e0;
  cLcd::eInfo info = cLcd::eNone;
  cLcd::eMode mode = cLcd::eAll;
  eLogLevel logLevel = logLevel;

  for (size_t i = 0; i < argStrings.size(); i++)
    if (argStrings[i] == "90") rotate = cLcd::e90;
    else if (argStrings[i] == "180") rotate = cLcd::e180;
    else if (argStrings[i] == "270") rotate = cLcd::e270;
    else if (argStrings[i] == "o") info = cLcd::eOverlay;
    else if (argStrings[i] == "a") mode = cLcd::eAll;
    else if (argStrings[i] == "c") mode = cLcd::eCoarse;
    else if (argStrings[i] == "e") mode = cLcd::eExact;
    else if (argStrings[i] == "s") mode = cLcd::eSingle;
    else if (argStrings[i] == "1") logLevel = LOGINFO1;
    else if (argStrings[i] == "2") logLevel = LOGINFO2;
    else if (argStrings[i] == "f") fontTest = true;

  cLog::init (logLevel, false, "", "gpio");


  //cLcd* lcd = new cLcdIli9320 (rotate, info, mode);
  cLcd* lcd = new cLcdTa7601 (rotate, info, mode);
  if (!lcd->initialise())
    return 0;

  if (fontTest) {
    //{{{  font test
    while (true) {
      int height = 8;
      while (height < 100) {
        cPoint point;
        lcd->clear (kMagenta);
        for (char ch = 'A'; ch < 0x7f; ch++) {
          point.x = lcd->text (kWhite, point, height, string(1,ch));
          if (point.x > lcd->getWidth()) {
            point.x = 0;
            point.y += height;
            if (point.y > lcd->getHeight())
              break;
            }
          }

        //lcd->text (kYellow, cPoint(), 20, "Hello Colin");

        lcd->present();

        lcd->setBacklightOn();
        lcd->delayUs (16000);
        height += 4;
        }
      }
    }
    //}}}

  // snapshot test
  while (true) {
    lcd->clearSnapshot();
    lcd->present();
    lcd->delayUs (5000);
    }

  return 0;
  }
