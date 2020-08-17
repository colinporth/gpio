// cSharpLcd.h
#pragma once

class cSharpLcd {
public:
  cSharpLcd();
  ~cSharpLcd();

  int getDisplayWidth() { return kWidth; }
  int getDisplayHeight() { return kHeight; }

  void clearDisplay();
  void turnOff();
  void turnOn();

  void writeLinesToDisplay (int lineNumber, int numLines, char* linesData);
  void writeLineToDisplay (int lineNumber, char* lineData);
  void writeLineBufferToDisplay (int lineNumber);
  void writeFrameBufferToDisplay();

  // line buffer
  void clearLineBuffer();
  void setLineBuffer();
  void writeByteToLineBuffer (int byteNumber, char byteToWrite);
  void writePixelToLineBuffer (int pixel, bool on);

  // frame buffer
  void clearFrameBuffer();
  void setFrameBuffer();
  void writeByteToFrameBuffer (int byteNumber, int lineNumber, char byteToWrite);
  void writePixelToFrameBuffer (unsigned int pixel, int lineNumber, bool on);

private:
  static const int kWidth = 400;
  static const int kHeight = 240;

  char reverseByte (int b);
  static void* toggleVcomThread (void* arg);

  int mHandle;
  char mCommandByte;
  char mClearByte;
  char mPaddingByte;
  char mLineBuffer [kWidth/8];
  char mFrameBuffer [kWidth*kHeight/8];
  };
