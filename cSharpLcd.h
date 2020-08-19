// cSharpLcd.h
#pragma once
#include <stdint.h>

class cSharpLcd {
public:
  cSharpLcd();
  ~cSharpLcd();

  int getDisplayWidth() { return kWidth; }
  int getDisplayHeight() { return kHeight; }

  void clearDisplay();
  void turnOff();
  void turnOn();

  void displayFrame();

  // line buffer
  void setLine();
  void clearLine();
  void writeByteToLine (uint8_t byteNumber, uint8_t byteToWrite);
  void writePixelToLine (uint16_t pixel, bool on);
  void lineToFrame (uint8_t lineNumber);

  // frame buffer
  void setFrame();
  void clearFrame();
  void writeByteToFrame (uint8_t byteNumber, uint8_t lineNumber, uint8_t byteToWrite);
  void writePixelToFrame (uint16_t pixel, uint8_t lineNumber, bool on);

private:
  static const int kWidth = 96;
  static const int kHeight = 96;
  static const int kRowHeader = 2;
  static const int kRowDataBytes = kWidth/8;
  static const int kRowBytes = kRowHeader + kRowDataBytes;
  static const int kFrameBytes = (kRowBytes * kHeight) + 1;

  uint8_t reverseByte (uint8_t b);
  static void* toggleVcomThread (void* arg);

  int mHandle;
  char mClear[2] = { 0b00100000, 0 };
  uint8_t mLineBuffer [kRowDataBytes];
  char mFrameBuffer [kFrameBytes];
  };
