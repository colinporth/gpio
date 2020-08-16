#pragma once

#define LCDWIDTH    (96)
#define LCDHEIGHT   (96)

class cSharpLcd {
public:
  cSharpLcd (char SCSpin, char DISPpin, char EXTCOMINpin, bool useEXTCOMIN);
  ~cSharpLcd();

  // return display parameters
  unsigned int getDisplayWidth() { return LCDWIDTH; }
  unsigned int getDisplayHeight() { return LCDHEIGHT; }

  // Write data direct to display
  void writeLineToDisplay (char lineNumber, char* line);
  void writeMultipleLinesToDisplay (char lineNumber, char numLines, char* lines);

  // Write data to line buffer
  void writePixelToLineBuffer (unsigned int pixel, bool isWhite);
  void writeByteToLineBuffer (char byteNumber, char byteToWrite);
  void copyByteWithinLineBuffer (char sourceByte, char destinationByte);
  void setLineBufferBlack();
  void setLineBufferWhite();

  // write data from line buffer to display
  void writeLineBufferToDisplay (char lineNumber);
  void writeLineBufferToDisplayRepeatedly (char lineNumber, char numLines);

  // Write data to frame buffer
  void writePixelToFrameBuffer (unsigned int pixel, char lineNumber, bool isWhite);
  void writeByteToFrameBuffer (char byteNumber, char lineNumber, char byteToWrite);
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

  char commandByte;
  char vcomByte;
  char clearByte;
  char paddingByte;
  char DISP;
  char SCS;
  char SI;
  char SCLK;
  char EXTCOMIN;
  bool enablePWM;
  char lineBuffer [LCDWIDTH/8];
  char frameBuffer [LCDWIDTH*LCDHEIGHT/8];
  };
