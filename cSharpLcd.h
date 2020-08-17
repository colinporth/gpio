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

  void writeLinesToDisplay (uint8_t lineNumber, uint8_t numLines, uint8_t* linesData);
  void writeLineToDisplay (uint8_t lineNumber, uint8_t* lineData);
  void writeLineBufferToDisplay (uint8_t lineNumber);
  void writeFrameBufferToDisplay();

  // line buffer
  void setLineBuffer();
  void clearLineBuffer();
  void writeByteToLineBuffer (uint8_t byteNumber, uint8_t byteToWrite);
  void writePixelToLineBuffer (uint16_t pixel, bool on);

  // frame buffer
  void setFrameBuffer();
  void clearFrameBuffer();
  void writeByteToFrameBuffer (uint8_t byteNumber, uint8_t lineNumber, uint8_t byteToWrite);
  void writePixelToFrameBuffer (uint16_t pixel, uint8_t lineNumber, bool on);

private:
  static const int kWidth = 400;
  static const int kHeight = 240;

  uint8_t reverseByte (uint8_t b);
  static void* toggleVcomThread (void* arg);

  int mHandle;
  uint8_t mCommandByte;
  uint8_t mClearByte;
  uint8_t mPaddingByte;
  uint8_t mLineBuffer [kWidth/8];
  uint8_t mFrameBuffer [kWidth*kHeight/8];
  };
