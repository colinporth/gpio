// lcdTest.cpp
//{{{  includes
#include "cLcd.h"

#include <vector>

#include "../shared/utils/utils.h"
#include "../shared/utils/cLog.h"

using namespace std;
//}}}

int main (int numArgs, char* args[]) {

  cLog::init (LOGINFO, false, "", "gpio");

  vector <string> argStrings;
  for (int i = 1; i < numArgs; i++)
    argStrings.push_back (args[i]);

  cLcd::eRotate rotate = argStrings.size() >= 1 ? cLcd::eRotate (stoi (argStrings[0])) : cLcd::e0;
  cLcd::eInfo info = argStrings.size() >= 2 ? cLcd::eInfo (stoi (argStrings[1])) : cLcd::eNone;
  cLcd::eMode mode = argStrings.size() >= 3 ? cLcd::eMode (stoi (argStrings[2])) : cLcd::eAll;

  //cLcd* lcd = new cLcdIli9320 (argStrings.empty() ? 270 : stoi (argStrings[0]));
  cLcd* lcd = new cLcdTa7601 (rotate, info, mode);
  if (!lcd->initialise())
    return 0;

  // font test
  //{{{  display font
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

    lcd->text (kYellow, cPoint(), 20, "Hello Colin");
    lcd->present();
    lcd->setBacklightOn();

    lcd->delayUs (16000);
    height += 4;
    }
  //}}}

  // snapshot test
  while (true) {
    lcd->clearSnapshot();
    lcd->present();
    lcd->delayUs (1000);
    // cLog::log (LOGINFO, lcd->getInfoString());
    }

  return 0;
  }
