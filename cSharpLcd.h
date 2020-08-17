// cSharpLcd.h
#pragma once

class cSharpLcd {
public:
  cSharpLcd();
  ~cSharpLcd();

  // return display parameters
  unsigned int getDisplayWidth() { return kWidth; }
  unsigned int getDisplayHeight() { return kHeight; }

  // Write data direct to display
  void writeMultipleLinesToDisplay (int lineNumber, int numLines, char* lines);
  void writeLineToDisplay (int lineNumber, char* line);

  // Write data to line buffer
  void writePixelToLineBuffer (unsigned int pixel, bool isWhite);
  void writeByteToLineBuffer (int byteNumber, char byteToWrite);
  void setLineBufferBlack();
  void setLineBufferWhite();

  // write data from line buffer to display
  void writeLineBufferToDisplay (int lineNumber);
  void writeLineBufferToDisplayRepeatedly (int lineNumber, int numLines);

  // Write data to frame buffer
  void writePixelToFrameBuffer (unsigned int pixel, int lineNumber, bool isWhite);
  void writeByteToFrameBuffer (int byteNumber, int lineNumber, char byteToWrite);
  void setFrameBufferBlack();
  void setFrameBufferWhite();

  // write data from frame buffer to display
  void writeFrameBufferToDisplay();

  // clear functions
  void clearLineBuffer();
  void clearFrameBuffer();
  void clearDisplay();

  // turn display on/off
  void turnOff();
  void turnOn();

  // software VCOM control
  void softToggleVCOM();

private:
  static const int kWidth = 400;
  static const int kHeight = 240;

  char reverseByte (int b);
  static void* hardToggleVCOM (void* arg);

  int handle;
  char commandByte;
  char vcomByte;
  char clearByte;
  char paddingByte;
  char SI;
  char SCLK;
  bool enablePWM;
  char lineBuffer [kWidth/8];
  char frameBuffer [kWidth*kHeight/8];
  };
