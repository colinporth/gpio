#pragma once

#define LCDWIDTH    (96)
#define LCDHEIGHT   (96)

class cSharpLcd {
public:
  cSharpLcd();
  ~cSharpLcd();

  // return display parameters
  unsigned int getDisplayWidth() { return LCDWIDTH; }
  unsigned int getDisplayHeight() { return LCDHEIGHT; }

  // Write data direct to display
  void writeLineToDisplay (char lineNumber, char* line);
  void writeMultipleLinesToDisplay (char lineNumber, char numLines, char* lines);

  // Write data to line buffer
  void writePixelToLineBuffer (unsigned int pixel, bool isWhite);
  void writeByteToLineBuffer (int byteNumber, char byteToWrite);
  void copyByteWithinLineBuffer (int sourceByte, int destinationByte);
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

  // software VCOM control - NOT YET PROPERLY IMPLEMENTED
  void softToggleVCOM();

private:
  static void* hardToggleVCOM (void* arg);
  char reverseByte (char b);

  int handle;
  char commandByte;
  char vcomByte;
  char clearByte;
  char paddingByte;
  char SI;
  char SCLK;
  bool enablePWM;
  char lineBuffer [LCDWIDTH/8];
  char frameBuffer [LCDWIDTH*LCDHEIGHT/8];
  };
