#include "cSharpLcd.h"

#include <unistd.h>
#include <cmath>
#include <iostream>

using namespace std;

cSharpLcd sharpLcd;

int lcdWidth;
int lcdHeight;
char numRepetitions = 4;
bool toggle = true;

int main() {

  sharpLcd.clearDisplay();
  lcdWidth = sharpLcd.getDisplayWidth();
  lcdHeight = sharpLcd.getDisplayHeight();
  sleep(2);

  while(1) {
    printf ("sinewave\n");
    //{{{  print sinewave
    //float increment = 360.00/lcdWidth;  // one number MUST have a decimal point here!
    float increment = 6.2831/lcdWidth;
    int sinValue = 0;
    for(float theta=0; theta<50.26; theta += 0.1256) {
      for(int y=1; y<=lcdHeight; y++) {
        for(int x=1; x<=lcdWidth; x++) {
          sinValue = ( sin( theta+(x*increment) ) * lcdHeight/2 ) + lcdHeight/2;
          if(sinValue >= y && y > lcdHeight/2) {
            sharpLcd.writePixelToLineBuffer(x, 0);
            }
          if(sinValue <= y && y <= lcdHeight/2) {
            sharpLcd.writePixelToLineBuffer(x, 0);
            }
          }

        sharpLcd.writeLineBufferToDisplay(y);
        sharpLcd.clearLineBuffer();
        }

      usleep(20000);
      sharpLcd.clearDisplay();
      }
    //}}}
    printf ("circles\n");
    //{{{  print expanding and contracting circles
    unsigned int originX = lcdWidth/2;
    unsigned int originY = lcdHeight/2;
    unsigned int expandingCircleRadius = (lcdHeight/2)*0.9;
    for(unsigned int repeat = 0; repeat < 2; repeat++) {
      for(unsigned int radius = 5; radius < expandingCircleRadius; radius++) {
        for(unsigned int y = originY - radius; y <= originY; y++) {
          // need to calculate left and right limits of the circle
          float theta = acos(float(abs(originY-(float)y))/float(radius));
          theta -= 1.5708;
          unsigned int xLength = cos(theta)*float(radius);
          for(unsigned int x = originX - xLength; x <= originX; x++) {
            sharpLcd.writePixelToLineBuffer(x, 0);
            sharpLcd.writePixelToLineBuffer(originX + (originX - x), 0);
           }
          sharpLcd.writeLineBufferToDisplay(y);
          sharpLcd.writeLineBufferToDisplay(originY + (originY - y));
          sharpLcd.clearLineBuffer();
          }
        usleep(20000);
        }
      for(unsigned int radius = expandingCircleRadius; radius > 2; radius--) {
        for(unsigned int y = originY - radius; y <= originY; y++) {
          // need to calculate left and right limits of the circle
          float theta = acos(float(abs(originY-(float)y))/float(radius));
          theta -= 1.5708;
          unsigned int xLength = cos(theta)*float(radius);
          for(unsigned int x = originX - xLength; x <= originX ; x++) {
            sharpLcd.writePixelToLineBuffer(x, 0);
            sharpLcd.writePixelToLineBuffer(originX + (originX - x), 0);
            }
          sharpLcd.writeLineBufferToDisplay(y);
          sharpLcd.writeLineBufferToDisplay(originY + (originY - y));
          sharpLcd.clearLineBuffer();
          }

        sharpLcd.writeLineBufferToDisplay(originY+radius);
        sharpLcd.writeLineBufferToDisplay(originY-radius);
        usleep(20000);
        }
      sharpLcd.clearDisplay();
      sharpLcd.clearLineBuffer();
      }
    //}}}
    printf ("circling circles\n");
    //{{{  print circling circle
    numRepetitions = 4;
    unsigned int sweepRadius = (lcdHeight/2)*0.8;
    unsigned int sweepOriginX = lcdWidth/2;
    unsigned int sweepOriginY = lcdHeight/2;
    unsigned int circleRadius = 0.7*((lcdHeight/2)-sweepRadius);
    for(float rads=0; rads<6.2824*numRepetitions; rads += 0.04) {
      // calculate circle centre
      unsigned int circleOriginX = sweepOriginX + cos(rads)*sweepRadius;
      unsigned int circleOriginY = sweepOriginY + sin(rads)*sweepRadius;
      // draw circle about the centre
      for(unsigned int y = circleOriginY - circleRadius; y <= circleOriginY; y++) {
        // need to calculate left and right limits of the circle
        float theta = acos(float(std::abs(circleOriginY-(float)y))/float(circleRadius));
        theta -= 1.5708;
        unsigned int xLength = cos(theta)*float(circleRadius);
        for(unsigned int x = circleOriginX - xLength; x <= circleOriginX; x++) {
          sharpLcd.writePixelToLineBuffer(x, 0);
          sharpLcd.writePixelToLineBuffer(circleOriginX + (circleOriginX - x), 0);
          }
        sharpLcd.writeLineBufferToDisplay(y);
        sharpLcd.writeLineBufferToDisplay(circleOriginY + (circleOriginY - y));
        sharpLcd.clearLineBuffer();
        }
      usleep(15000);
      sharpLcd.clearDisplay();
      }

    sharpLcd.clearDisplay();
    sharpLcd.clearLineBuffer();
    //}}}
    printf ("triangles\n");
    //{{{  print triangles
    numRepetitions = 4;
    toggle = false;
    for(char i=0; i< numRepetitions; i++) {
      for(int y=1; y<=lcdHeight; y++) {
        for(int x=1; x<y+((lcdWidth-lcdHeight)/2); x++) {
          sharpLcd.writePixelToLineBuffer(x, toggle);
          }
        sharpLcd.writeLineBufferToDisplay(y);
        usleep(5000);
        }
      for(int y=lcdHeight; y>0; y--) {
        for(int x=lcdWidth; x>y+((lcdWidth-lcdHeight)/2); x--) {
          sharpLcd.writePixelToLineBuffer(x, toggle);
          }
        sharpLcd.writeLineBufferToDisplay(y);
        usleep(5000);
        }
      if (toggle)
        toggle = false;
      else
        toggle =  true;
      }

    sharpLcd.clearDisplay();
    sharpLcd.clearLineBuffer();
    //}}}
    printf ("chequerboard\n");
    //{{{  print chequerboard patterns
    numRepetitions = 8;
    for(char i=0; i<numRepetitions; i++) {
      for(int y=1; y<=lcdHeight; y++) {
        for(int x=1; x <=lcdWidth/8; x++) {
          if(toggle) {
            sharpLcd.writeByteToLineBuffer(x, 0xFF);
            toggle = false;
            }
          else {
            sharpLcd.writeByteToLineBuffer(x, 0x00);
            toggle = true;
            }
          }

        sharpLcd.writeLineBufferToDisplay(y);
        sharpLcd.clearLineBuffer();

        if ((y % 8) == 0) {
          if (toggle)
            toggle = false;
          else
            toggle = true;
          }
        }
      usleep(500000);
      if(toggle)
        toggle = false;
      else
       toggle = true;
      }

    sharpLcd.clearDisplay();
    sharpLcd.clearLineBuffer();

    numRepetitions = 4;
    for(char i=0; i<numRepetitions; i++) {
      if (i%2 == 0)
        sharpLcd.setLineBufferBlack();
      else
        sharpLcd.setLineBufferWhite();
      for (int y = 1; y < lcdHeight+1; y++) {
        sharpLcd.writeLineBufferToDisplay (y);
        usleep(5000);
        }
      }

    sharpLcd.clearLineBuffer();
    sharpLcd.clearDisplay();
    //}}}
    printf ("horizontal line\n");
    //{{{  print horizontal line descending down the screen
    char lineThickness = 10;
    for (int i=0; i<numRepetitions; i++) {
      for (char y=1; y<lcdHeight+lineThickness+1; y++) {  // lcdHeight+10 to give the line some thickness
        int blackLine = y;
        int whiteLine = y - lineThickness;
        if(whiteLine > 0 && whiteLine < lcdHeight+1) {
          sharpLcd.setLineBufferWhite();
          sharpLcd.writeLineBufferToDisplay(whiteLine);
          }
        if(blackLine > 0 && blackLine < lcdHeight+1) {
          sharpLcd.setLineBufferBlack();
          sharpLcd.writeLineBufferToDisplay(blackLine);
          }
        usleep(5000);
        }
      }

    // make sure you clear both the buffer and display before moving on to a new sequence
    sharpLcd.clearLineBuffer();
    sharpLcd.clearDisplay();
    //}}}
    }

  sleep(1);
  return 0;
  }
