#include "cSharpLcd.h"

#include <unistd.h>
#include <cstdio>
#include <cmath>

using namespace std;

int main() {

  bool toggle = true;
  int numRepetitions = 4;

  cSharpLcd sharpLcd;
  int lcdWidth = sharpLcd.getDisplayWidth();
  int lcdHeight = sharpLcd.getDisplayHeight();

  sharpLcd.clearDisplay();

  while(1) {
    printf ("sinewave\n");
    //{{{  print sinewave
    float increment = 6.2831f / lcdWidth;

    for (float theta = 0; theta < 10.f; theta += 0.1256f) {
      for (int y = 0; y < lcdHeight; y++) {
        sharpLcd.clearLine();
        for (int x = 1; x <= lcdWidth; x++) {
          int sinValue = (sin(theta + (x * increment)) * lcdHeight/2) + lcdHeight/2;
          if (sinValue >= y && y > lcdHeight/2)
            sharpLcd.writePixelToLine (x, 0);
          if (sinValue <= y && y <= lcdHeight/2)
            sharpLcd.writePixelToLine (x, 0);
          }
        sharpLcd.lineToFrame (y);
        }

      sharpLcd.displayFrame();
      usleep (10000);
      }
    //}}}

    printf ("circles\n");
    //{{{  print expanding and contracting circles
    unsigned int originX = lcdWidth/2;
    unsigned int originY = lcdHeight/2;
    unsigned int expandingCircleRadius = (lcdHeight/2) * 0.9;

    for (unsigned int repeat = 0; repeat < 2; repeat++) {
      for (unsigned int radius = 5; radius < expandingCircleRadius; radius++) {
        sharpLcd.clearDisplay();
        for (unsigned int y = originY - radius; y <= originY; y++) {
          sharpLcd.clearLine();
          // need to calculate left and right limits of the circle
          float theta = acos (float(abs (originY - (float)y))/float(radius));
          theta -= 1.5708;
          unsigned int xLength = cos (theta) * float(radius);
          for(unsigned int x = originX - xLength; x <= originX; x++) {
            sharpLcd.writePixelToLine (x, 0);
            sharpLcd.writePixelToLine (originX + (originX - x), 0);
            }
          sharpLcd.lineToFrame (y);
          sharpLcd.lineToFrame (originY + (originY - y));
          }

        sharpLcd.displayFrame();
        usleep (20000);
        }

      for (unsigned int radius = expandingCircleRadius; radius > 2; radius--) {
        sharpLcd.clearDisplay();
        for (unsigned int y = originY - radius; y <= originY; y++) {
          sharpLcd.clearLine();
          // need to calculate left and right limits of the circle
          float theta = acos (float(abs (originY-(float)y))/float(radius)) - 1.5708f;
          unsigned int xLength = cos (theta) * float(radius);
          for (unsigned int x = originX - xLength; x <= originX ; x++) {
            sharpLcd.writePixelToLine (x, 0);
            sharpLcd.writePixelToLine (originX + (originX - x), 0);
            }
          sharpLcd.lineToFrame (y);
          sharpLcd.lineToFrame (originY + (originY - y));
          }

        sharpLcd.clearLine();
        sharpLcd.lineToFrame (originY + radius);
        sharpLcd.lineToFrame (originY - radius);

        sharpLcd.displayFrame();
        usleep (20000);
        }
      }
    //}}}

    printf ("circling circles\n");
    //{{{  print circling circle
    numRepetitions = 4;

    unsigned int sweepRadius = (lcdHeight/2) * 0.8;
    unsigned int sweepOriginX = lcdWidth/2;
    unsigned int sweepOriginY = lcdHeight/2;
    unsigned int circleRadius = 0.7 * ((lcdHeight/2) - sweepRadius);

    for (float rads = 0; rads < 6.2824 * numRepetitions; rads += 0.04) {
      sharpLcd.clearDisplay();

      // calculate circle centre
      unsigned int circleOriginX = sweepOriginX + cos(rads)*sweepRadius;
      unsigned int circleOriginY = sweepOriginY + sin(rads)*sweepRadius;

      // draw circle about the centre
      for(unsigned int y = circleOriginY - circleRadius; y <= circleOriginY; y++) {
        sharpLcd.clearLine();

        // need to calculate left and right limits of the circle
        float theta = acos (float(std::abs (circleOriginY - (float)y)) / float(circleRadius));
        theta -= 1.5708;
        unsigned int xLength = cos (theta) * float(circleRadius);
        for(unsigned int x = circleOriginX - xLength; x <= circleOriginX; x++) {
          sharpLcd.writePixelToLine (x, 0);
          sharpLcd.writePixelToLine (circleOriginX + (circleOriginX - x), 0);
          }

        sharpLcd.lineToFrame (y);
        sharpLcd.lineToFrame (circleOriginY + circleOriginY - y);
        }

      sharpLcd.displayFrame();
      usleep (10000);
      }
    //}}}

    printf ("chequerboard\n");
    //{{{  print chequerboard patterns
    numRepetitions = 8;

    for (int i = 0; i < numRepetitions; i++) {
      sharpLcd.clearDisplay();

      for (int y = 0; y < lcdHeight; y++) {
        for (int x = 0; x < lcdWidth/8; x++) {
          sharpLcd.writeByteToLine (x, toggle ? 0xFF : 0x00);
          toggle = !toggle;
          }
        sharpLcd.lineToFrame (y);

        if ((y % 8) == 0)
          toggle = !toggle;
        }

      sharpLcd.displayFrame();
      usleep (250000);

      toggle = !toggle;
      }
    //}}}
    }

  sleep(1);
  return 0;
  }
