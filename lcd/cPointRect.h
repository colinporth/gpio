// cPointRect.h
#pragma once
#include <cstdint>
#include <string>
#include <math.h>
//#include "../shared/utils/utils.h"

//{{{
struct cPoint {
public:
  //{{{
  cPoint()  {
    x = 0;
    y = 0;
    }
  //}}}
  //{{{
  cPoint (const int16_t x, const int16_t y) {
    this->x = x;
    this->y = y;
    }
  //}}}

  //{{{
  const cPoint operator - (const cPoint& point) const {
    return cPoint (x - point.x, y - point.y);
    }
  //}}}
  //{{{
  const cPoint operator + (const cPoint& point) const {
    return cPoint (x + point.x, y + point.y);
    }
  //}}}
  //{{{
  const cPoint operator * (const int16_t f) const {
    return cPoint (x * f, y * f);
    }
  //}}}
  //{{{
  const cPoint operator * (const cPoint& point) const {
    return cPoint (x * point.x, y * point.y);
    }
  //}}}
  //{{{
  const cPoint operator / (const int16_t f) const {
    return cPoint (x / f, y / f);
    }
  //}}}
  //{{{
  const cPoint& operator += (const cPoint& point) {
    x += point.x;
    y += point.y;
    return *this;
    }
  //}}}
  //{{{
  const cPoint& operator -= (const cPoint& point) {
    x -= point.x;
    y -= point.y;
    return *this;
    }
  //}}}
  //{{{
  bool inside (const cPoint& pos) {
  // return pos inside rect formed by us as size
    return pos.x >= 0 && pos.x < x && pos.y >= 0 && pos.y < y;
    }
  //}}}

  int16_t x;
  int16_t y;
  };
//}}}
//{{{
struct cPointF {
public:
  //{{{
  cPointF()  {
    x = 0;
    y = 0;
    }
  //}}}
  //{{{
  cPointF (const cPoint& p) {
    this->x = p.x;
    this->y = p.y;
    }
  //}}}
  //{{{
  cPointF (float x, float y) {
    this->x = x;
    this->y = y;
    }
  //}}}

  //{{{
  cPointF operator - (const cPointF& point) const {
    return cPointF (x - point.x, y - point.y);
    }
  //}}}
  //{{{
  cPointF operator + (const cPointF& point) const {
    return cPointF (x + point.x, y + point.y);
    }
  //}}}
  //{{{
  cPointF operator * (const float s) const {
    return cPointF (x * s, y * s);
    }
  //}}}
  //{{{
  cPointF operator / (const float s) const {
    return cPointF (x / s, y / s);
    }
  //}}}
  //{{{
  const cPointF& operator += (const cPoint& point)  {
    x += point.x;
    y += point.y;
    return *this;
    }
  //}}}
  //{{{
  const cPointF& operator -= (const cPoint& point)  {
    x -= point.x;
    y -= point.y;
    return *this;
    }
  //}}}
  //{{{
  const cPointF& operator *= (const float s)  {
    x *= s;
    y *= s;
    return *this;
    }
  //}}}
  //{{{
  const cPointF& operator /= (const float s)  {
    x /= s;
    y /= s;
    return *this;
    }
  //}}}

  //{{{
  bool inside (const cPointF& pos) const {
  // return pos inside rect formed by us as size
    return pos.x >= 0 && pos.x < x && pos.y >= 0 && pos.y < y;
    }
  //}}}
  //{{{
  float magnitude() const {
  // return magnitude of point as vector
    return sqrt ((x*x) + (y*y));
    }
  //}}}
  //{{{
  cPointF perp() {
    float mag = magnitude();
    return cPointF (-y / mag, x / mag);
    }
  //}}}

  float x;
  float y;
  };
//}}}
//{{{
struct cRect {
public:
  //{{{
  cRect() {
    left = 0;
    bottom = 0;
    right = 0;
    bottom = 0;
    }
  //}}}
  //{{{
  cRect (const int16_t l, const int16_t t, const int16_t r, const int16_t b)  {
    left = l;
    top = t;
    right = r;
    bottom = b;
    }
  //}}}
  //{{{
  cRect (const cPoint& topLeft, const cPoint& bottomRight)  {
    left = topLeft.x;
    top = topLeft.y;
    right = bottomRight.x;
    bottom = bottomRight.y;
    }
  //}}}

  int16_t getWidth() { return right - left; }
  int16_t getHeight() { return bottom - top; }
  int getNumPixels() { return getWidth() * getHeight(); }

  cPoint getTL() { return cPoint(left, top); }
  cPoint getTL (const int16_t offset) { return cPoint(left+offset, top+offset); }
  cPoint getTR() { return cPoint(right, top); }
  cPoint getBL() { return cPoint(left, bottom); }
  cPoint getBR() { return cPoint(right, bottom); }

  cPoint getSize() { return cPoint(right-left, bottom-top); }
  cPoint getCentre() { return cPoint(getCentreX(), getCentreY()); }
  int16_t getCentreX() { return (left + right)/2; }
  int16_t getCentreY() { return (top + bottom)/2; }

  //{{{
  cRect operator + (const cPoint& point) {
    return cRect (left + point.x, top + point.y, right + point.x, bottom + point.y);
    }
  //}}}
  //{{{
  bool inside (const cPoint& pos) {
  // return pos inside rect
    return (pos.x >= left) && (pos.x < right) && (pos.y >= top) && (pos.y < bottom);
    }
  //}}}
  //{{{
  //std::string cRect::getString() {
    //return "l:" + dec(left) + " r:" + dec(right) + " t:" + dec(top) + " b:" + dec(bottom);
    //}
  //}}}
  //{{{
  //std::string cRect::getYfirstString() {
    //return "t:" + dec(top) + " b:" + dec(bottom) + " l:" + dec(left) + " r:" + dec(right);
    //}
  //}}}

  int16_t left;
  int16_t top;
  int16_t right;
  int16_t bottom;
  };
//}}}
//{{{
struct sSpan {
  cRect r;
  uint16_t lastScanRight; // scanline bottom-1 can be partial, ends in lastScanRight, ??? where is this used ???
  uint32_t size;          // ??? where is this used, speedup for merge ???
  sSpan* next;   // linked skip list in array for fast pruning
  };
//}}}
