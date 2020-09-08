// code to derive and demonstrate a fast gradient fill algorithm - Chris Lomont VC++ 7.0 Nov 2002
#include <windows.h> // used for timing and graphics
#include <cmath>
#include <iostream>
using namespace std;

typedef unsigned char color_t;

// constants defining the surface to fill - change to check different sizes
static const int width = 200;
static const int height = 200;

// a global surface to fill, and a backup for comparisons
static color_t surface[width*height],backupSurface[width*height];

// gradient function pointer
typedef void (*GradientFillPtr)(const color_t & c1, const color_t & c2);

// function pointer to polynomial to test for drawing
typedef double (*PolyPtr)(double j, double x);

PolyPtr interpolatingPoly;
//{{{
void SetPixel (int x, int y, const color_t & color) {
// draw a pixel in the global surface

  surface[x+y*width] = color;
  } // SetPixel
//}}}

//{{{
void ComputeDifferences (bool relative, int mult, double * totalError = NULL, double * maxError = NULL) {
// show difference between surface and backup
// overwrites global surface with values showing differences
// if relative false, set surface[pos] to 255 wherever
// there is a difference, else 0
// if relative true, set value to abs of difference times mult
// also, return error - total and max values, if non NULL entries
// reset these

  if (NULL != totalError)
    *totalError = 0;

  if (NULL != maxError)
    *maxError = 0;

  for (int pos = 0; pos < sizeof(surface); pos++) {
    int err;
    err = abs(surface[pos] - backupSurface[pos]);
    if (0 != err) {
      if (false == relative)
        surface[pos] = 255;
      else
        surface[pos] = min(255,err*mult);
      if (NULL != totalError)
        *totalError += err;
      if (NULL != maxError) {
        if (err > *maxError)
          *maxError = err;
         }
      }
    else
      surface[pos] = 0;
    }
  } // ComputeDifferences
//}}}
//{{{
void ShowSurface (int xpos, int ypos) {
// for debugging, show the global surface at given position
// draws straight to screen - not a good Win32 app :)

  HDC hdc = GetDC(NULL); // WIN32 stuff in here

  color_t c;
  for (int j = 0; j < height; j++)
    for (int i = 0; i < width; i++) {
      c = surface[i+j*width];
      SetPixel (hdc,xpos+i,ypos+j,RGB(c,c,c));
      }

  ReleaseDC(NULL,hdc);
  } // ShowSurface
//}}}

//{{{
/* now follow the interpolating polynomials */
double IPoly2 (double j, double x) {
// interpolating quadratic polynomial

  double j2 = j*j;
  return j + (-3.*j + 4.*sqrt(0.25 + j2) - 1.*sqrt(1. + j2))*x +
         (2.*j - 4.*sqrt(0.25 + j2) + 2.*sqrt(1. + j2))*x*x;
}
//}}}
//{{{
double IPoly3 (double j, double x) {
// interpolating cubic polynomial

  double j2 = j*j;
  return j + (-5.5*j + 9.*sqrt(0.1111111111111111 + j2) -
         4.5*sqrt(0.4444444444444444 + j2) + 1.*sqrt(1. + j2))*x +
         (9.*j - 22.5*sqrt(0.1111111111111111 + j2) +
         18.*sqrt(0.4444444444444444 + j2) - 4.5*sqrt(1. + j2))*x*x +
         (-4.5*j + 13.5*sqrt(0.1111111111111111 + j2) -
         13.5*sqrt(0.4444444444444444 + j2) + 4.5*sqrt(1. + j2))*x*x*x;
  }
//}}}
//{{{
double IPoly4 (double j, double x) {
// interpolating quartic polynomial

  double j2 = j*j,r1,r2,r3,r4;
  r1 = sqrt(0.0625 + j2);
  r2 = sqrt(0.25 + j2);
  r3 = sqrt(0.5625 + j2);
  r4 = sqrt(1 + j2);
  return j + (-8.333333333333332*j + 16*r1 - 12*r2 + 5.333333333333333*r3 -
         r4)*x + (23.333333333333332*j - 69.33333333333333*r1 + 76*r2 -
         37.33333333333333*r3 + 7.333333333333333*r4)*x*x +
         (-26.666666666666664*j + 96*r1 - 128*r2 + 74.66666666666667*r3 -
         15.999999999999998*r4)*x*x*x + (10.666666666666666*j -
         42.666666666666664*r1 + 64*r2 - 42.666666666666664*r3 +
         10.666666666666666*r4)*x*x*x*x;
  }
//}}}
//{{{
double TPoly2 (double j, double x) {
// Taylor series quadratic polynomial

  double j2,r2;
  j2 = j*j;
  r2 = sqrt(1+4*j2);
  x -= 0.5; // center it
  return r2/2 + x/r2 + 4*j2*x*x/(r2*r2*r2);
  }
//}}}
//{{{
double TPoly3 (double j, double x) {
// Taylor series cubic polynomial

  double j2,r2;
  j2 = j*j;
  r2 = sqrt(1+4*j2);
  x -= 0.5; // center it
  return r2/2 + x/r2 + 4*j2*x*x/(r2*r2*r2) - 8*j2*x*x*x/(r2*r2*r2*r2*r2);
  }
//}}}
//{{{
double TPoly4 (double j, double x) {
// Taylor series quartic polynomial

  double j2,r2;
  j2 = j*j;
  r2 = sqrt(1+4*j2);
  x -= 0.5; // center it
  return r2/2 + x/r2 + 4*j2*x*x/(r2*r2*r2) - 8*j2*x*x*x/(r2*r2*r2*r2*r2) -
         16*j2*(j2 -1)*x*x*x*x/(r2*r2*r2*r2*r2*r2*r2);
}
//}}}
//{{{
double CPoly2 (double j, double x) {
// Chebyshev point interpolating quadratic polynomial

  double j2 = j*j,r1,r2,r3;
  r1 = sqrt(0.00448729810778068 + j2);
  r2 = sqrt(0.25 + j2);
  r3 = sqrt(0.8705127018922193 + j2);
  return 1.2440169358562927*r1 - 0.3333333333333335*r2 +
         0.08931639747704095*r3 + (-2.4880338717125854*r1 +
         2.666666666666667*r2 - 0.1786327949540819*r3 -
         0.5*(2.666666666666667*r1 - 5.333333333333335*r2 +
         2.666666666666667*r3))*x + (2.666666666666667*r1 -
         5.333333333333335*r2 + 2.666666666666667*r3)*x*x;
  }
//}}}
//{{{
double CPoly3 (double j, double x) {
// Chebyshev point interpolating cubic polynomial

  double j2 = j*j,r1,r2,r3,r4;
  r1 = sqrt(0.0014485813926750633 + j2);
  r2 = sqrt(0.09526993616913669 + j2);
  r3 = sqrt(0.4779533685342265 + j2);
  r4 = sqrt(0.9253281139039617 + j2);
  return 1.2568348730314622*r1 - 0.37415144066637296*r2 +
         0.16704465947982494*r3 - 0.049728091844914335*r4 +
         (-1.3065629648763768*r1 + 0.3889551651687712*r2 -
         0.17365397017533368*r3 + 1.0912617698829394*r4 -
         0.9619397662556434*(6.122934917841437*r1 -
         10.782072520180591*r2 + 5.125218270688209*r3 -
         0.4660806683490568*r4))*x + (6.122934917841437*r1 -
         10.782072520180591*r2 + 5.125218270688209*r3 - 0.4660806683490568*r4 -
         0.9619397662556434*(-6.122934917841437*r1 + 14.78207252018059*r2 -
         14.78207252018059*r3 + 6.122934917841437*r4))*x*x +
         (-6.122934917841437*r1 + 14.78207252018059*r2 - 14.78207252018059*r3 +
         6.122934917841437*r4)*x*x*x;
  }
//}}}
//{{{
double CPoly4 (double j, double x) {
// Chebyshev point interpolating quartic polynomial

  double j2 = j*j,r1,r2,r3,r4,r5;
  r1 = sqrt(0.0005988661492916429 + j2);
  r2 = sqrt(0.04248024955689501 + j2);
  r3 = sqrt(0.25 + j2);
  r4 = sqrt(0.6302655018493681 + j2);
  r5 = sqrt(0.9516553824444453 + j2);
  return 1.2627503029350091*r1 - 0.3925221011010298*r2 +
         0.20000000000000007*r3 - 0.10190508989888575*r4 +
         0.031676888064907274*r5 + (-11.537170939541092*r1 +
         17.721650625490284*r2 - 9.6*r3 + 4.966893194508029*r4 -
         1.551372880457229*r5)*x + (33.65141886601196*r1 -
         71.0262271348634*r2 + 60.79999999999999*r3 -
         34.5056569091295*r4 + 11.08046517798098*r5)*x*x +
         (-39.166991453338284*r1 + 95.01686363257255*r2 -
         102.39999999999998*r3 + 70.66981681541661*r4 -
         24.11968899465095*r5)*x*x*x + (15.821670111997312*r1 -
         41.4216701119973*r2 + 51.19999999999999*r3 -
         41.42167011199729*r4 + 15.821670111997303*r5)*x*x*x*x;
  }
//}}}
//{{{
double SPoly2 (double y, double x) {
// the super quadratic function
// do not call with y = 0!

  double y2,y3,x2,r2;
  y2 = y*y; y3 = y2*y;
  x2 = x*x;
  r2 = sqrt(1 + y2);
  return (48*y3 - 256*x*y3 + 240*x2*y3 + 4*x*r2 - 33*y2*r2 + 166*x*y2*r2 -
         150*x2*y2*r2 + 3*y2*(-6 + 5*y2 + 10*x2*(-2 + 3*y2) -
         6*x*(-4 + 5*y2))*log(y) - 3*y2*(-6 + 5*y2 + 10*x2*(-2 + 3*y2) -
         6*x*(-4 + 5*y2))*log(1 + r2))/4;
  }
//}}}
//{{{
double SPoly3 (double y, double x) {
// the super cubic function
// do not call with y = 0!

  double y2,y3,y4,y5,x2,x3,r2;
  y2 = y*y; y3 = y2*y; y4 = y3*y; y5 = y4*y;
  x2 = x*x; x3 = x*x2;
  r2 = sqrt(1 + y2);
  return 40.*y3 - 400.*x*y3 + 900.*x2*y3 - 560.*x3*y3 - 18.666666666666664*y5 +
         224.*x*y5 - 560.*x2*y5 + 373.3333333333333*x3*y5 + x*r2 -
         19.333333333333332*y2*r2 + 174.5*x*y2*r2 - 370.*x2*y2*r2 +
         221.66666666666666*x3*y2*r2 + 18.666666666666664*y4*r2 - 224.*x*y4*r2 +
         560.*x2*y4*r2 - 373.3333333333333*x3*y4*r2 + y2*(-8. + 30.*y2 +
         x3*(70 - 525*y2) + x*(60. - 337.5*y2) + x2*(-120 + 810*y2))*log(y)+
         y2*(8 - 30*y2 + x2*(120. - 810.*y2) +x*(-60. + 337.5*y2) + x3*(-70. +
         525*y2))*log(1. + r2);
  } // SPoly3
//}}}
//{{{
double SPoly4 (double y, double x) {
// the super quartic function
// do not call with y = 0!

  double y2,y3,y4,y5,x2,x3,x4,r2;
  y2 = y*y; y3 = y2*y; y4 = y3*y; y5 = y4*y;
  x2 = x*x; x3 = x*x2; x4 = x*x3;
  r2 = sqrt(1 + y2);
  return (12*x*(-3200*y3 + 7168*y5 + 2*r2 + 1009*y2*r2 - 5593*y4*r2 +
         75*y2*(4 - 63*y2 + 21*y4)*log(y) - 75*y2*(4 - 63*y2 +
         21*y4)*log(1 + r2))+ 630*x4*y2*(160*y - 448*y3 - 44*r2 +
         343*y2*r2 - 3*(4 - 90*y2 + 35*y4)*log(y) + 3*(4 - 90*y2 + 35*y4)*
         log(1 + r2)) + 5*y2*(480*y - 896*y3 - 172*r2 + 707*y2*r2 -
         3*(20 - 210*y2 + 63*y4)*log(y) + 3*(20 - 210*y2 +
         63*y4)*log(1 + r2))- 140*x3*y2*(1536*y - 4096*y3 - 434*r2 +
         3151*y2*r2 - 15*(8 - 168*y2 + 63*y4)*log(y) + 15*(8 - 168*y2 +
         63*y4)*log(1 + r2))+ 210*x2*y2*(720*y - 1792*y3 - 212*r2 +
         1387*y2*r2 - 3*(20 - 378*y2 + 135*y4)*log(y) + 3*(20 - 378*y2 +
         135*y4)*log(1 + r2)))/24.;
  }
//}}}

//{{{
void GradientFill_1 (const color_t& c1, const color_t& c2) {
// basic circular gradient fill (assumes width and height are even!)
// see paper for description of variables, etc

  double r,x,y,M,dc,K,cx,cy;

  // the center of the surface
  cx = (double)width / 2.0;
  cy = (double)height / 2.0;

  // compute max distance M from center
  M = sqrt (cx*cx + cy*cy);

  // the color delta
  dc = c2 - c1;

  // and constant used in the code....
  K = dc / M;
  color_t ce; // the exact color computed for each square
  for (int j = 0; j < height; j++)
    for (int i = 0; i < width; i++) {
      // coodinates relative to center, shifted to pixel centers
      x = i - cx + 0.5;
      y = j - cy + 0.5;
      r = sqrt(x*x+y*y); // the distance

      // the "exact" color to place at this pixel
      ce = (color_t)(r*K+c1);
      SetPixel(i,j,ce);
      }

  } // GradientFill_1
//}}}
//{{{
void GradientFill_2 (const color_t& c1, const color_t& c2) {
// faster version using symmetry (assumes width and height are even!)

  double r,x,y,M,dc,K,cx,cy;

  // the center
  cx = (double)width/2.0;
  cy = (double)height/2.0;

  // compute max distance M from center
  M = sqrt(cx*cx+cy*cy);

  // the color delta
  dc = c2-c1;

  // and constant used in the code....
  K = dc/M;
  color_t ce; // the exact color computed for each square
  for (int j = 0; j < height/2; j++)
    for (int i = 0; i < width/2; i++) {
      // coodinates relative to center, shifted to pixel centers
      x = i - cx + 0.5;
      y = j - cy + 0.5;
      r = sqrt(x*x+y*y);
      ce = (color_t)(r*K+c1); // the "exact" color
      // now draw exact colors - 4 pixels
      SetPixel(i,j,ce);
      SetPixel(width - 1 - i,j,ce);
      SetPixel(i,height - 1 - j,ce);
      SetPixel(width - 1 - i,height - 1 - j,ce);
      }
  } // GradientFill_2
//}}}
//{{{
void GradientFill_3 (const color_t& c1, const color_t& c2) {
// faster version using symmetry and direct writing to memory
// (assumes width and height are even!)

  double r,x,y,M,dc,K,cx,cy;

  // the center
  cx = (double)width/2.0;
  cy = (double)height/2.0;

  // compute max distance M from center
  M = sqrt(cx*cx+cy*cy);

  // the color delta
  dc = c2-c1;

  // and constant used in the code....
  K = dc/M;
  color_t ce; // the exact color computed for each square
  color_t * p1, *p2, *p3, *p4; // 4 quadrant pointers
  for (int j = 0; j < height/2; j++) {
    p1 = surface + j*width;
    p2 = p1 + width - 1;
    p3 = surface + (height - 1 - j)*width;
    p4 = p3 + width - 1;
    for (int i = 0; i < width/2; i++) {
      // coodinates relative to center, shifted to pixel centers
      x = i - cx + 0.5;
      y = j - cy + 0.5;
      r = sqrt(x*x+y*y);
      ce = (color_t)(r*K+c1); // the "exact" color

      // now draw exact colors - 4 pixels
      *p1++ = ce;
      *p2-- = ce;
      *p3++ = ce;
      *p4-- = ce;
      }
    }
  } // GradientFill_3
//}}}
//{{{
void GradientFill_4 (const color_t& c1, const color_t& c2) {
// version using polynomial approximation

  double K,dc;

  // the color delta
  dc = c2-c1;
  color_t ce; // the exact color computed for each square
  color_t * p1, *p2, *p3, *p4; // 4 quadrant pointers
  double maxDimension = max(height,width);

  // and constant used in the code....
  double t1,t2; // temp values
  t1 = width/maxDimension;
  t2 = height/maxDimension;
  K = dc/(sqrt(t1*t1+t2*t2));
  for (int j = 0; j < height/2; j++) {
    double d,alpha, beta; // pixel coords in rectangle [-1,1]x[-1,1]
    beta = ((double)(height/2-1-j)+0.5)/(maxDimension/2.0);
    p1 = surface + (height/2-j)*width+width/2+1;
    p2 = p1 - 1;
    p3 = surface + (height/2+j)*width+width/2+1;
    p4 = p3 - 1;
    p1 = surface + j*width;
    p2 = p1 + width - 1;
    p3 = surface + (height - 1 - j)*width;
    p4 = p3 + width - 1;
    for (int i = 0; i < width/2; i++) {
      // get color and update forward differencing stuff
      alpha = ((double)(width/2-1-i)+0.5)/(maxDimension/2.0);
      d = interpolatingPoly(beta,alpha); // call the polynomial
      ce = (color_t)(d*K + c1);
      // now draw exact colors - 4 pixels
      *p1++ = ce;
      *p2-- = ce;
      *p3++ = ce;
      *p4-- = ce;
      }
    }
  } // GradientFill_4
//}}}
//{{{
void GradientFill_5 (const color_t& c1, const color_t& c2) {
// version using Chebyshev polynomial and constants precomputed

  double K,dc;

  // the color delta
  dc = c2-c1;
  color_t ce; // the exact color computed for each square
  color_t * p1, *p2, *p3, *p4; // 4 quadrant pointers
  double maxDimension = max(height,width);

  // and constant used in the code....
  double t1,t2; // temp values
  t1 = width/maxDimension;
  t2 = height/maxDimension;
  K = dc/(sqrt(t1*t1+t2*t2));
  for (int j = 0; j < height/2; j++) {
    double d,alpha, beta; // pixel coords in rectangle [-1,1]x[-1,1]
    beta = ((double)(height/2-1-j)+0.5)/(maxDimension/2.0);
    p1 = surface + j*width;
    p2 = p1 + width - 1;
    p3 = surface + (height - 1 - j)*width;
    p4 = p3 + width - 1;
    double a0,a1,a2,a3; // polynomial coefficients
    double j2,r1,r2,r3,r4; // temp values
    j2 = beta*beta;
    r1 = sqrt(0.0014485813926750633 + j2);
    r2 = sqrt(0.09526993616913669 + j2);
    r3 = sqrt(0.4779533685342265 + j2);
    r4 = sqrt(0.9253281139039617 + j2);
    a0 = 1.2568348730314625*r1 - 0.3741514406663722*r2 +
    0.16704465947982383*r3 - 0.04972809184491411*r4;
    a1 = -7.196457548543286*r1 + 10.760659484982682*r2 -
    5.10380523549030050*r3 + 1.53960329905090450*r4;
    a2 = 12.012829501508346*r1 - 25.001535905017075*r2 +
    19.3446816555246950*r3 - 6.35597525201596500*r4;
    a3 = -6.122934917841437*r1 + 14.782072520180590*r2 -
    14.7820725201805900*r3 + 6.12293491784143700*r4;
    for (int i = 0; i < width/2; i++) {
      // get color
      alpha = ((double)(width/2-1-i)+0.5)/(maxDimension/2.0);

      // evaluate approximating polynomial
      d = ((a3*alpha+a2)*alpha+a1)*alpha+a0;
      ce = (color_t)(d*K + c1);

      // now draw exact colors - 4 pixels
      *p1++ = ce;
      *p2-- = ce;
      *p3++ = ce;
      *p4-- = ce;
      }
    }
  } // GradientFill_5
//}}}
//{{{
void GradientFill_6 (const color_t& c1, const color_t& c2) {
// version using Chebyshev polynomial approximation, and forward differencing

  double K,dc;

  // the color delta
  dc = c2-c1;
  color_t ce; // the exact color computed for each square
  color_t * p1, *p2, *p3, *p4; // 4 quadrant pointers
  double maxDimension = max(height,width);

  // and constant used in the code....
  double t1,t2; // temp values
  t1 = width/maxDimension;
  t2 = height/maxDimension;
  K = dc/(sqrt(t1*t1+t2*t2));
  double delta = 1.0/(maxDimension/2.0); // stepsize

  // initial pixel relative x coord
  double alpha = (1.0)/maxDimension;
  for (int j = 0; j < height/2; j++) {
    double d, beta; // pixel coords in rectangle [-1,1]x[-1,1]
    beta = ((double)(height/2-1-j)+0.5)/(maxDimension/2.0);
    p1 = surface + j*width+width/2;
    p2 = p1 - 1;
    p3 = surface + (height - 1 - j)*width+width/2;
    p4 = p3 - 1;
    double a0,a1,a2,a3; // polynomial coefficients
    double j2,r1,r2,r3,r4; // temp values
    j2 = beta*beta;
    r1 = sqrt(0.0014485813926750633 + j2);
    r2 = sqrt(0.0952699361691366900 + j2);
    r3 = sqrt(0.4779533685342265000 + j2);
    r4 = sqrt(0.9253281139039617000 + j2);
    a0 = 1.2568348730314625*r1 - 0.3741514406663722*r2 +
    0.16704465947982383*r3 - 0.04972809184491411*r4;
    a1 = -7.196457548543286*r1 + 10.760659484982682*r2 -
    5.10380523549030050*r3 + 1.53960329905090450*r4;
    a2 = 12.012829501508346*r1 - 25.001535905017075*r2 +
    19.3446816555246950*r3 - 6.35597525201596500*r4;
    a3 = -6.122934917841437*r1 + 14.782072520180590*r2 -
    14.7820725201805900*r3 + 6.12293491784143700*r4;

    // forward differencing stuff
    double d1,d2,d3;

    // initial color value and differences
    d = ((a3*alpha+a2)*alpha+a1)*alpha+a0+c1/K;
    d1 = 3*a3*alpha*alpha*delta + alpha*delta*(2*a2+3*a3*delta) + delta*(a1+a2*delta+a3*delta*delta);
    d2 = 6*a3*alpha*delta*delta + 2*delta*delta*(a2 + 3*a3*delta);
    d3 = 6*a3*delta*delta*delta;
    d *= K; // we can prescale these here
    d1 *= K;
    d2 *= K;
    d3 *= K;
    for (int i = 0; i < width/2; i++) {
      // get color and update forward differencing stuff
      ce = (color_t)(d);
      d+=d1; d1+=d2; d2+=d3;

      // now draw 4 pixels
      *p1++ = ce;
      *p2-- = ce;
      *p3++ = ce;
      *p4-- = ce;
      }
    }
  } // GradientFill_6
//}}}
//{{{
void GradientFill_7 (const color_t& c1, const color_t& c2) {
// stuff above, with fixed point math

  double K,dc;
  // the color delta

  dc = c2-c1;
  color_t ce; // the exact color computed for each square
  color_t * p1, *p2, *p3, *p4; // 4 quadrant pointers
  double maxDimension = max(height,width);

  // and constants used in the code....
  double t1,t2; // temp values
  t1 = width/maxDimension;
  t2 = height/maxDimension;

  #define _BITS 24 // bits of fractional point stuff
  #define _SCALE (1<<_BITS) // size to scale it
  K = dc/sqrt(t1*t1+t2*t2)*_SCALE;

  double delta = 2.0/maxDimension; // stepsize
  double delta2,delta3; // powers of delta
  delta2 = delta*delta;
  delta3 = delta2*delta;

  // initial color value and differences
  double alpha = 1.0/maxDimension;
  for (int j = 0; j < height/2; j++) {
    double d, beta; // pixel coords in rectangle [-1,1]x[-1,1]
    beta = ((double)(height-1-(j<<1)))/maxDimension;
    p1 = surface + j*width+width/2;
    p2 = p1 - 1;
    p3 = surface + (height - 1 - j)*width+width/2;
    p4 = p3 - 1;
    double a0,a1,a2,a3; // polynomial coefficients
    double j2,r1,r2,r3,r4; // temp values
    j2 = beta*beta;

    // numbers from the analysis to create the polynomial
    r1 = sqrt(0.0014485813926750633 + j2);
    r2 = sqrt(0.0952699361691366900 + j2);
    r3 = sqrt(0.4779533685342265000 + j2);
    r4 = sqrt(0.9253281139039617000 + j2);
    a0 = 1.2568348730314625*r1 - 0.3741514406663722*r2 +
    0.16704465947982383*r3 - 0.04972809184491411*r4;
    a1 = -7.196457548543286*r1 + 10.760659484982682*r2 -
    5.10380523549030050*r3 + 1.53960329905090450*r4;
    a2 = 12.012829501508346*r1 - 25.001535905017075*r2 +
    19.3446816555246950*r3 - 6.35597525201596500*r4;
    a3 = -6.122934917841437*r1 + 14.782072520180590*r2 -
    14.7820725201805900*r3 + 6.12293491784143700*r4;

    // forward differencing variables
    double d1,d2,d3;

    // initial color value and differences
    d = ((a3*alpha+a2)*alpha+a1)*alpha+a0+c1/K*_SCALE;
    d1 = delta*(3*a3*alpha*alpha + alpha*(2*a2+3*a3*delta) + a2*delta + a3*delta2 + a1);
    d2 = 2*delta2*(3*a3*(alpha + delta) + a2);
    d3 = 6*a3*delta3;

    // now fixed point stuff
    int color,dc1,dc2,dc3;
    color = (int)(d*K+0.5); // round to nearest value
    dc1 = (int)(d1*K+0.5);
    dc2 = (int)(d2*K+0.5);
    dc3 = (int)(d3*K+0.5);
    for (int i = 0; i < width/2; i++) {
      // get color and update forward differencing stuff
      ce = (color>>_BITS);
      color += dc1; dc1 += dc2; dc2 += dc3;
      // now draw 4 pixels
      *p1++ = ce;
      *p2-- = ce;
      *p3++ = ce;
      *p4-- = ce;
      }
    }

  #undef _BITS // remove these defines
  #undef _SCALE
  } // GradientFill_7
//}}}

//{{{
unsigned long TimeFunction (void (func)(const color_t& c1, const color_t& c2),
                                       const color_t & c1, const color_t & c2) {
// call gradient function the number of times, and return ms elapsed

  unsigned long startTime=0, endTime=0;
  memset(surface,0,sizeof(surface)); // clear it out

  startTime = timeGetTime(); // WIN32
  for (int pos = 0; pos < count; pos++)
    func(c1,c2);

  endTime = timeGetTime(); // WIN32
  return endTime - startTime;
  }
//}}}
//{{{
void ShowPolyErrors() {
// show the polynomial approximants and their error patterns
// and output error data to cout

  int mult = 60; // error brightness
  color_t c2 = 255, c1 = 0; // gradient color
  double totalError, maxError;
  //{{{
  // function pointers to polynomials to test for drawing
  static const PolyPtr interpolatingPolys[] = {
  TPoly2, TPoly3, TPoly4,
  IPoly2, IPoly3, IPoly4,
  CPoly2, CPoly3, CPoly4,
  SPoly2, SPoly3, SPoly4,
  };
  //}}}
  //{{{
  static const char * polyNames[] = {
  "TPoly2", "TPoly3", "TPoly4",
  "IPoly2", "IPoly3", "IPoly4",
  "CPoly2", "CPoly3", "CPoly4",
  "SPoly2", "SPoly3", "SPoly4",
  };
  //}}}

  // do default method - make backup - this is our target image
  GradientFill_1(c1,c2);
  memcpy(backupSurface,surface,sizeof(surface));
  cout << "Error information\n";
  cout << "Polynomial name : total error , max error, error/pixel\n";
  double area = width*height;
  for (int pos = 0; pos < 12; pos++) {
    // show all polys
    int xpos,ypos; // drawing positions
    xpos = 2*(pos%3)*width + 10;
    ypos = (pos/3)*height + 10;
    interpolatingPoly = interpolatingPolys[pos]; // test this one
    GradientFill_4(c1,c2);
    ShowSurface(xpos,ypos);
    // show difference between surface and backup, and get error data
    ComputeDifferences(true,mult, &totalError, &maxError);
    cout << polyNames[pos] << ": " << totalError << ",\t";
    cout << maxError << ",\t" << (totalError/area) << endl;
    ShowSurface(xpos+width,ypos);
    }

  }
//}}}
//{{{
void CompareMethods (GradientFillPtr func1, GradientFillPtr func2) {
// compare two methods, show errors between and main image

unsigned long timeElapsed;
int count = 50, mult = 50;
color_t c1 = 255, c2 = 0;
// do default method
timeElapsed = TimeFunction(GradientFill_1,c1,c2,count);
cout << "Time for Gradient_1: " << timeElapsed << " ms\n";
ShowSurface(10,10+2*height); // make backup - this is our target image
memcpy(backupSurface,surface,sizeof(surface));
timeElapsed = TimeFunction(func1,c1,c2,count);
cout << "Time for function 1: " << timeElapsed << " ms\n";
ShowSurface(10,10);
ComputeDifferences(true,mult); // difference between surface and backup
ShowSurface(10+width,10);
timeElapsed = TimeFunction(func2,c1,c2,count);
cout << "Time for function 2: " << timeElapsed << " ms\n";
ShowSurface(10,10+height);
ComputeDifferences(true,mult); // difference between surface and backup
ShowSurface(10+width,10+height);
// lastly, compare top two methods
double totalError, maxError;
func1(c1,c2);
memcpy(backupSurface,surface,sizeof(surface)); // backup of func1 image
func2(c1,c2);
// show difference between function 2 and function 1
ComputeDifferences(false,2*mult, &totalError, &maxError);
ShowSurface(10+2*width,10);
double area = width*height;
cout << "Total, max, and per pixel error: " << totalError << ",\t";
cout << maxError << ",\t" << (totalError/area) << endl;
} // CompareMethods
//}}}
//{{{
void TimeMethods() {
// time the various methods

unsigned long timeElapsed, baseTimeElapsed;
int count = 250, mult = 60;
cout << "Timing: size = " << width << 'x' << height;
cout << " count = " << count << endl;
//{{{
static const GradientFillPtr gradientFunctions[] = {
GradientFill_1,
GradientFill_2,
GradientFill_3,
GradientFill_4,
GradientFill_5,
GradientFill_6,
GradientFill_7
};
//}}}
color_t c1 = 255, c2 = 0;
// the base time
//{{{
//}}}
baseTimeElapsed = TimeFunction(GradientFill_1,c1,c2,count);
for (int pos = 0; pos < 7; pos++)
{
int xpos, ypos;
xpos = (pos % 3)*width;
ypos = (pos / 3)*height;
timeElapsed = TimeFunction(gradientFunctions[pos],c1,c2,count);
cout << "Time for GradientFill_" << pos+1 << ": ";
cout << timeElapsed << " ms, ";
cout << baseTimeElapsed/(double)(timeElapsed) << " fold increase\n";
}
} // TimeMethods
//}}}

//{{{
int main() {

  // make this the default poly for functions needing it
  interpolatingPoly = CPoly3;

  // call this to show the basic gradient, and the polynomial approximations
  // also output error numbers to cout
  ShowPolyErrors();

  // call this to time the various methods
  TimeMethods();
  // use this to show time and errors between any two methods versus baseline
  CompareMethods(GradientFill_1,GradientFill_2); // identical output
  CompareMethods(GradientFill_2,GradientFill_3); // identical output
  CompareMethods(GradientFill_3,GradientFill_4); // switch to poly - errors
  CompareMethods(GradientFill_4,GradientFill_5); // identical - same polys
  CompareMethods(GradientFill_5,GradientFill_6); // identical - same polys + forward diff
  CompareMethods(GradientFill_6,GradientFill_7); // switch to integers - some error
  CompareMethods(GradientFill_7,GradientFill_1); // final comparison

  return 0;
  }
//}}}
