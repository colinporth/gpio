// test.cpp
//{{{  includes
#include "lcd/cLcd.h"
#include "cTouchscreen.h"

#include "../shared/utils/utils.h"
#include "../shared/utils/cLog.h"

using namespace std;
//}}}

static uint8_t surface[320*320];
//{{{
void gradSimpleSymmetry (const uint8_t& c1, const uint8_t& c2, int dim) {
// faster version using symmetry and direct writing to memory
// (assumes width and height are even!)

  int width = dim;
  int height = dim;

  // the center
  double cx = (double)width/2.0;
  double cy = (double)height/2.0;

  // compute max distance M from center
  double M = sqrt(cx*cx+cy*cy);

  // the color delta
  double dc = c2 - c1;

  // and constant used in the code
  double K = dc / M;
  for (int j = 0; j < height/2; j++) {
    uint8_t* p1 = surface + (j * width);
    uint8_t* p2 = p1 + width - 1;
    uint8_t* p3 = surface + (height - 1 - j) * width;
    uint8_t* p4 = p3 + width - 1;
    for (int i = 0; i < width/2; i++) {
      // coodinates relative to center, shifted to pixel centers
      double x = i - cx + 0.5;
      double y = j - cy + 0.5;
      double r = sqrt ((x * x) + (y * y));
      int8_t ce = (uint8_t)((r * K) + c1);
      *p1++ = ce;
      *p2-- = ce;
      *p3++ = ce;
      *p4-- = ce;
      }
    }
  }
//}}}
//{{{
void gradChebyshev (const uint8_t& c1, const uint8_t& c2, int dim) {
// version using Chebyshev polynomial and constants precomputed

  int width = dim;
  int height = dim;

  // the color delta
  double dc = c2-c1;
  double maxDimension = max(height,width);

  // and constant used in the code....
  double t1 = width/maxDimension;
  double t2 = height/maxDimension;
  double K = dc / (sqrt(t1*t1+t2*t2));
  for (int j = 0; j < height/2; j++) {
    double beta = ((double)(height/2-1-j)+0.5)/(maxDimension/2.0);
    uint8_t* p1 = surface + j*width;
    uint8_t* p2 = p1 + width - 1;
    uint8_t* p3 = surface + (height - 1 - j)*width;
    uint8_t* p4 = p3 + width - 1;
    double j2 = beta*beta;
    double r1 = sqrt(0.0014485813926750633 + j2);
    double r2 = sqrt(0.09526993616913669 + j2);
    double r3 = sqrt(0.4779533685342265 + j2);
    double r4 = sqrt(0.9253281139039617 + j2);
    double a0 = 1.2568348730314625*r1 - 0.3741514406663722*r2 +
                0.16704465947982383*r3 - 0.04972809184491411*r4;
    double a1 = -7.196457548543286*r1 + 10.760659484982682*r2 -
                5.10380523549030050*r3 + 1.53960329905090450*r4;
    double a2 = 12.012829501508346*r1 - 25.001535905017075*r2 +
                19.3446816555246950*r3 - 6.35597525201596500*r4;
    double a3 = -6.122934917841437*r1 + 14.782072520180590*r2 -
               14.7820725201805900*r3 + 6.12293491784143700*r4;
    for (int i = 0; i < width/2; i++) {
      // get color
      double alpha = ((double)(width/2-1-i)+0.5)/(maxDimension/2.0);

      // evaluate approximating polynomial
      double d = ((a3*alpha+a2)*alpha+a1)*alpha+a0;
      uint8_t ce = (uint8_t)(d*K + c1);

      // now draw exact colors - 4 pixels
      *p1++ = ce;
      *p2-- = ce;
      *p3++ = ce;
      *p4-- = ce;
      }
    }
  }
//}}}
//{{{
void gradChebyshevForward (const uint8_t& c1, const uint8_t& c2, int dim) {
// version using Chebyshev polynomial approximation, and forward differencing

  int width = dim;
  int height = dim;

  // the color delta
  double dc = c2-c1;
  double maxDimension = max(height,width);

  // and constant used in the code....
  double t1 = width/maxDimension;
  double t2 = height/maxDimension;
  double K = dc/(sqrt(t1*t1+t2*t2));
  double delta = 1.0/(maxDimension/2.0); // stepsize

  // initial pixel relative x coord
  double alpha = (1.0) / maxDimension;
  for (int j = 0; j < height/2; j++) {
    double beta = ((double)(height/2 - 1 - j) + 0.5) / (maxDimension /2.0);

    uint8_t*p1 = surface + j*width+width/2;
    uint8_t*p2 = p1 - 1;
    uint8_t*p3 = surface + (height - 1 - j) * width + width/2;
    uint8_t*p4 = p3 - 1;

    double j2 = beta * beta;

    double r1 = sqrt (0.0014485813926750633 + j2);
    double r2 = sqrt (0.0952699361691366900 + j2);
    double r3 = sqrt (0.4779533685342265000 + j2);
    double r4 = sqrt (0.9253281139039617000 + j2);

    double a0 = 1.2568348730314625*r1 - 0.3741514406663722*r2 +
                0.16704465947982383*r3 - 0.04972809184491411*r4;
    double a1 = -7.196457548543286*r1 + 10.760659484982682*r2 -
                 5.10380523549030050*r3 + 1.53960329905090450*r4;
    double a2 = 12.012829501508346*r1 - 25.001535905017075*r2 +
                19.3446816555246950*r3 - 6.35597525201596500*r4;
    double a3 = -6.122934917841437*r1 + 14.782072520180590*r2 -
                14.7820725201805900*r3 + 6.12293491784143700*r4;

    // forward differencing stuff, initial color value and differences
    double d = ((a3 * alpha + a2) * alpha + a1) * alpha + a0 + c1/K;
    double d1 = 3*a3*alpha*alpha*delta + alpha*delta*(2*a2+3*a3*delta) + delta*(a1+a2*delta+a3*delta*delta);
    double d2 = 6*a3*alpha*delta*delta + 2*delta*delta * (a2 + 3*a3*delta);
    double d3 = 6*a3*delta*delta*delta;

    d *= K; // we can prescale these here
    d1 *= K;
    d2 *= K;
    d3 *= K;
    for (int i = 0; i < width/2; i++) {
      // get color and update forward differencing stuff
      uint8_t ce = (uint8_t)d;
      d += d1;
      d1 += d2;
      d2 += d3;

      // now draw 4 pixels
      *p1++ = ce;
      *p2-- = ce;
      *p3++ = ce;
      *p4-- = ce;
      }
    }
  }
//}}}
//{{{
void gradChebyshevForwardFixed (const uint8_t& c1, const uint8_t& c2, int dim) {
// stuff above, with fixed point math

  int width = dim;
  int height = dim;

  #define _BITS 23 // bits of fractional point stuff
  #define _SCALE (1 << _BITS) // size to scale it

  // color delta
  double dc = c2 - c1;

  double maxDimension = max (height, width);

  // and constants used in the code
  double t1 = width / maxDimension;
  double t2 = height / maxDimension;

  double K = dc / sqrt(t1 * t1 + t2 * t2) * _SCALE;

  double delta = 2.0 / maxDimension; // stepsize
  double delta2 = delta * delta;
  double delta3 = delta2 * delta;

  // initial color value and differences
  double alpha = 1.0/maxDimension;
  for (int j = 0; j < height/2; j++) {
    // pixel coords in rectangle [-1,1]x[-1,1]
    double beta = ((double)(height-1-(j<<1))) / maxDimension;
    uint8_t* p1 = surface + j * width + width/2;
    uint8_t* p2 = p1 - 1;
    uint8_t* p3 = surface + (height - 1 - j) * width + width/2;
    uint8_t* p4 = p3 - 1;

    double j2 = beta * beta;

    // numbers from the analysis to create the polynomial
    double r1 = sqrt (0.0014485813926750633 + j2);
    double r2 = sqrt (0.0952699361691366900 + j2);
    double r3 = sqrt (0.4779533685342265000 + j2);
    double r4 = sqrt (0.9253281139039617000 + j2);
    double a0 =  1.2568348730314625 * r1 -  0.3741514406663722 * r2 +
                0.16704465947982383 * r3 - 0.04972809184491411 * r4;
    double a1 =  -7.196457548543286 * r1 +  10.760659484982682 * r2 -
                5.10380523549030050 * r3 + 1.53960329905090450 * r4;
    double a2 =  12.012829501508346 * r1 -  25.001535905017075 * r2 +
                19.3446816555246950 * r3 - 6.35597525201596500 * r4;
    double a3 =  -6.122934917841437 * r1 +  14.782072520180590 * r2 -
                14.7820725201805900 * r3 + 6.12293491784143700 * r4;

    // forward differencing variables, initial color value and differences
    double d = ((a3 * alpha + a2) * alpha + a1) * alpha + a0 + c1/K * _SCALE;
    double d1 = delta * (3 * a3 * alpha * alpha + alpha * (2 * a2 + 3 * a3 * delta) + a2 * delta + a3 * delta2 + a1);
    double d2 = 2 * delta2 * (3 * a3 * (alpha + delta) + a2);
    double d3 = 6 * a3 * delta3;

    // fixed point stuff
    int color = (int)(d * K + 0.5); // round to nearest value
    int dc1 = (int)(d1 * K + 0.5);
    int dc2 = (int)(d2 * K + 0.5);
    int dc3 = (int)(d3 * K + 0.5);
    for (int i = 0; i < width/2; i++) {
      // get color and update forward differencing stuff
      uint8_t ce = (color >> _BITS);
      color += dc1;
      dc1 += dc2;
      dc2 += dc3;

      // now draw 4 pixels
      *p1++ = ce;
      *p2-- = ce;
      *p3++ = ce;
      *p4-- = ce;
      }
    }

  }
//}}}
//{{{
void radial (cLcd* lcd, int dim) {

  int width = dim;
  int height = dim;

  lcd->clear();
  memset (surface, 0, width*height);

  double time = lcd->timeUs();
  gradChebyshev (255, 0, dim);
  double time1 = lcd->timeUs() - time;

  for (int y = 0; y < height; y++)
    for (int x = 0; x < width; x++)
      lcd->pix (kWhite, surface[(y*width) + x], cPoint(x,y));
  lcd->present();

  lcd->clear();
  memset (surface, 0, width*height);

  time = lcd->timeUs();
  gradChebyshevForward (255, 0, dim);
  double time2 = lcd->timeUs() - time;

  for (int y = 0; y < height; y++)
    for (int x = 0; x < width; x++)
      lcd->pix (kWhite, surface[(y*width) + x], cPoint(x,y));
  lcd->present();

  lcd->clear();
  memset (surface, 0, width*height);

  time = lcd->timeUs();
  gradChebyshevForwardFixed (255, 0, dim);
  double time3 = lcd->timeUs() - time;

  for (int y = 0; y < height; y++)
    for (int x = 0; x < width; x++)
      lcd->pix (kWhite, surface[(y*width) + x], cPoint(x,y));

  lcd->present();

  cLog::log (LOGINFO1, dec(int(time1*1000000.)) + " " +
                       dec(int(time2*1000000.)) + " " +
                       dec(int(time3*1000000.)));
  }
//}}}

int main (int numArgs, char* args[]) {

  bool draw = false;
  bool drawRadial = false;
  cLcd::eRotate rotate = cLcd::e0;
  cLcd::eInfo info = cLcd::eNone;
  cLcd::eMode mode = cLcd::eCoarse;
  eLogLevel logLevel = LOGINFO;
  int lcdType = 93418;
  int spiSpeed = 16000000;

  //{{{  dumb command line option parser
  for (int argIndex = 1; argIndex < numArgs; argIndex++) {
    string str (args[argIndex]);

    if (str == "0") rotate = cLcd::e0;
    else if (str == "90") rotate = cLcd::e90;
    else if (str == "180") rotate = cLcd::e180;
    else if (str == "270") rotate = cLcd::e270;

    else if (str == "o") info = cLcd::eOverlay;
    else if (str == "a") mode = cLcd::eAll;
    else if (str == "s") mode = cLcd::eSingle;
    else if (str == "c") mode = cLcd::eCoarse;
    else if (str == "e") mode = cLcd::eExact;

    else if (str == "1") logLevel = LOGINFO1;
    else if (str == "2") logLevel = LOGINFO2;

    else if (str == "r") drawRadial = true;
    else if (str == "d") draw = true;

    else if (str == "1289") lcdType = 1289;
    else if (str == "7601") lcdType = 7601;
    else if (str == "7735") lcdType = 7735;
    else if (str == "9225") lcdType = 9225;
    else if (str == "9320") lcdType = 9320;
    else if (str == "9341p8") lcdType = 93418;
    else if (str == "9341p16") lcdType = 934116;
    else if (str == "100k") spiSpeed = 100000;
    else if (str == "400k") spiSpeed = 400000;

    else if (str == "1m")   spiSpeed = 1000000;
    else if (str == "2m")   spiSpeed = 2000000;
    else if (str == "4m")   spiSpeed = 4000000;
    else if (str == "8m")   spiSpeed = 8000000;
    else if (str == "16m")  spiSpeed = 16000000;
    else if (str == "24m")  spiSpeed = 24000000;
    else if (str == "30m")  spiSpeed = 30000000;
    else if (str == "32m")  spiSpeed = 32000000;
    else if (str == "36m")  spiSpeed = 36000000;
    else if (str == "40m")  spiSpeed = 40000000;
    else if (str == "44m")  spiSpeed = 44000000;
    else if (str == "48m")  spiSpeed = 48000000;
    else
      cLog::log (LOGERROR, "unrecognised option " + str);
    }
  //}}}

  cLog::init (logLevel, false, "", "gpio");

  cLcd* lcd;
  switch (lcdType) {
    case 1289: lcd = new cLcd1289 (rotate, info, mode); break;
    case 7601: lcd = new cLcd7601 (rotate, info, mode); break;
    case 7735: lcd = new cLcd7735 (rotate, info, mode, spiSpeed); break; // 16000000
    case 9225: lcd = new cLcd9225 (rotate, info, mode, spiSpeed); break; // 24000000
    case 9320: lcd = new cLcd9320 (rotate, info, mode); break;
    case 9341: lcd = new cLcd9341 (rotate, info, mode, spiSpeed); break; // 30000000
    case 93418: lcd = new cLcd9341p8 (rotate, info, mode); break;
    case 934116: lcd = new cLcd9341p16 (rotate, info, mode); break;
    default: exit(1);
    }

  if (!lcd->initialise())
    return 0;

  cTouchscreen* ts = nullptr;
  //cTouchscreen* ts = new cTouchscreen();
  if (ts)
    ts->init();

  lcd->setBacklightOn();
  if (drawRadial) {
    //{{{  draw radial
    for (int i = 2; i < 320; i += 2)
      radial (lcd, i);
     }
    //}}}

  while (true) {
    if (draw) {
      //{{{  draw test
      float height = 30.f;
      float maxHeight = 60.f;

      lcd->snapshot();
      lcd->grad (kBlack, kRed, kYellow, kWhite, cRect (0,0, lcd->getWidth(), int(height)));

      cPoint point (int(maxHeight), 0);
      for (char ch = 'A'; ch < 0x7f; ch++) {
        point.x = lcd->text (kWhite, point, height, string(1,ch));
        if (point.x > lcd->getWidth())
          break;
        }

      if (ts && ts->getTouchDown()) {
        int16_t x;
        int16_t y;
        int16_t z;
        if (ts->getTouchPos (&x,&y,&z, lcd->getWidth(), lcd->getHeight())) {
          cPointF p (x-2,y-2);
          lcd->ellipseAA (p, cPointF(height/2.f, height/2.f), 16);
          lcd->renderAA (kYellow, true);
          lcd->ellipseOutlineAA (p, cPointF(height/2.f, height/2.f), height / 4.f, 16);
          lcd->renderAA (kBlue, true);
          }
        else
          cLog::log (LOGERROR, "lifted");
        }

      lcd->present();
      lcd->setBacklightOn();
      lcd->delayUs (5000);
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
