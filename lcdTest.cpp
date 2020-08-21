// lcdTest.cpp
#include <cstdint>
#include "lcd.h"

#include "../shared/utils/utils.h"

using namespace std;

int main() {

  cLog::init (LOGINFO, false, "", "gpio");

  cLcd* lcd = new cLcd9225b();
  lcd->initialise();

  while (true) {
    for (int i = 0; i < 1000; i += 3) {
      lcd->clear (Black);
      lcd->text (White, 0,0, 100, dec(i,3));
      lcd->update();
      gpioDelay (40000);
      }

    int height = 8;
    while (height++ < lcd->getHeight()) {
      int x = 0;
      int y = 0;
      lcd->clear (Magenta);
      for (char ch = 'A'; ch < 0x7f; ch++) {
        x = lcd->text (White, x, y, height, string(1,ch));
        if (x > lcd->getWidth()) {
          x = 0;
          y += height;
          if (y > lcd->getHeight())
            break;
          }
        }
      lcd->text (Yellow, 0,0, 20, "Hello Colin");
      lcd->update();
      gpioDelay (40000);
      }
    }

  delete lcd;
  return 0;
  }