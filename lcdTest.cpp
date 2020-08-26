// lcdTest.cpp
#include <cstdint>
#include <string>

#include "cLcd.h"

#include "../shared/utils/utils.h"
#include "../shared/utils/cLog.h"

using namespace std;

int main() {

  cLog::init (LOGINFO, false, "", "gpio");

  int rotate = 0;
  while (true) {
    cLcd* lcd = new cLcd1289();
    lcd->initialise (rotate);

    for (int i = 0; i < 100; i++) {
      lcd->clear (kOrange);
      lcd->text (kWhite, 0,0, 100, dec(i,3));
      lcd->update();
      lcd->delayUs (40000);
      }

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
      lcd->delayUs (40000);
      }

    rotate = (rotate + 90) % 360;
    delete (lcd);
    }

  return 0;
  }
