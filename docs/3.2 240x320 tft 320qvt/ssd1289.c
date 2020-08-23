// ssd1289.c
#include "display.h"
#include "displayHw.h"

#define LANDSCAPE

#ifdef LANDSCAPE
#define TFTWIDTH  320
#define TFTHEIGHT 240
#else
#define TFTWIDTH  240
#define TFTHEIGHT 320
#endif

//{{{  commands
#define SSD1289_REG_OSCILLATION      0x00
#define SSD1289_REG_DRIVER_OUT_CTRL  0x01
#define SSD1289_REG_LCD_DRIVE_AC     0x02
#define SSD1289_REG_POWER_CTRL_1     0x03
#define SSD1289_REG_DISPLAY_CTRL     0x07
#define SSD1289_REG_FRAME_CYCLE      0x0b
#define SSD1289_REG_POWER_CTRL_2     0x0c
#define SSD1289_REG_POWER_CTRL_3     0x0d
#define SSD1289_REG_POWER_CTRL_4     0x0e
#define SSD1289_REG_GATE_SCAN_START  0x0f

#define SSD1289_REG_SLEEP_MODE       0x10
#define SSD1289_REG_ENTRY_MODE       0x11
#define SSD1289_REG_POWER_CTRL_5     0x1e

#define SSD1289_REG_GDDRAM_DATA      0x22
#define SSD1289_REG_WR_DATA_MASK_1   0x23
#define SSD1289_REG_WR_DATA_MASK_2   0x24
#define SSD1289_REG_FRAME_FREQUENCY  0x25

#define SSD1289_REG_GAMMA_CTRL_1     0x30
#define SSD1289_REG_GAMME_CTRL_2     0x31
#define SSD1289_REG_GAMMA_CTRL_3     0x32
#define SSD1289_REG_GAMMA_CTRL_4     0x33
#define SSD1289_REG_GAMMA_CTRL_5     0x34
#define SSD1289_REG_GAMMA_CTRL_6     0x35
#define SSD1289_REG_GAMMA_CTRL_7     0x36
#define SSD1289_REG_GAMMA_CTRL_8     0x37
#define SSD1289_REG_GAMMA_CTRL_9     0x3a
#define SSD1289_REG_GAMMA_CTRL_10    0x3b

#define SSD1289_REG_V_SCROLL_CTRL_1  0x41
#define SSD1289_REG_V_SCROLL_CTRL_2  0x42
#define SSD1289_REG_H_RAM_ADR_POS    0x44
#define SSD1289_REG_V_RAM_ADR_START  0x45
#define SSD1289_REG_V_RAM_ADR_END    0x46
#define SSD1289_REG_FIRST_WIN_START  0x48
#define SSD1289_REG_FIRST_WIN_END    0x49
#define SSD1289_REG_SECND_WIN_START  0x4a
#define SSD1289_REG_SECND_WIN_END    0x4b
#define SSD1289_REG_GDDRAM_X_ADDR    0x4e
#define SSD1289_REG_GDDRAM_Y_ADDR    0x4f
//}}}

//{{{
bool getMono() {

  return false;
}
//}}}
//{{{
uint16_t getWidth() {

  return TFTWIDTH;
}
//}}}
//{{{
uint16_t getHeight() {

  return TFTHEIGHT;
}
//}}}

//{{{
void vertScroll (uint16_t yorg) {

  writeCommandData (SSD1289_REG_V_SCROLL_CTRL_1, yorg & 0x01FF);
  }
//}}}
//{{{
bool writeWindow (int16_t xorg, int16_t yorg, uint16_t xlen, uint16_t ylen) {

  uint16_t xend = xorg + xlen - 1;
  uint16_t yend = yorg + ylen - 1;

  if ((xend < TFTWIDTH) && (yend < TFTHEIGHT)) {
    #ifdef LANDSCAPE
    writeCommandData (SSD1289_REG_H_RAM_ADR_POS, (yend<<8) | yorg);
    writeCommandData (SSD1289_REG_V_RAM_ADR_START, xorg);
    writeCommandData (SSD1289_REG_V_RAM_ADR_END, xend);
    writeCommandData (SSD1289_REG_GDDRAM_X_ADDR, yorg);
    writeCommandData (SSD1289_REG_GDDRAM_Y_ADDR, xorg);
    #else
    writeCommandData (SSD1289_REG_H_RAM_ADR_POS, (xend<<8) | xorg);
    writeCommandData (SSD1289_REG_V_RAM_ADR_START, yorg);
    writeCommandData (SSD1289_REG_V_RAM_ADR_END, yend);
    writeCommandData (SSD1289_REG_GDDRAM_X_ADDR, xorg);
    writeCommandData (SSD1289_REG_GDDRAM_Y_ADDR, yorg);
    #endif

    writeCommand (SSD1289_REG_GDDRAM_DATA);
    return true;
    }

  return false;
  }
//}}}

//{{{
void setPixel (uint16_t colour, int16_t x, int16_t y) {

  drawRect (colour, x, y, 1, 1);
  }
//}}}
//{{{
void setTTPixels (uint16_t colour, uint8_t* bytes, int16_t x, int16_t y, uint16_t pitch, uint16_t rows) {

  if (writeWindow (x, y, pitch, rows))
    for (int i = 0; i < rows*pitch; i++) {
      uint8_t grey = *bytes++;
      uint16_t rgb = ((grey << 8) & 0xF800) | // b 15:11
                     ((grey << 3) & 0x07E0) | // g 10:5
                     ((grey >> 3) & 0x001F);  // r 4:0
      writeColour (rgb, 1);
      }
  }
//}}}
void setBitPixels (uint16_t colour, int16_t xorg, int16_t yorg, uint16_t xlen, uint16_t ylen) {}
void drawLines (uint16_t yorg, uint16_t yend) {}

//{{{
void drawRect (uint16_t colour, int16_t xorg, int16_t yorg, uint16_t xlen, uint16_t ylen) {

  if (writeWindow (xorg, yorg, xlen, ylen))
    writeColour (colour, xlen * ylen);
}
//}}}
//{{{
void drawImage (image_t* image, int16_t xorg, int16_t yorg) {

  if (writeWindow (xorg, yorg, image->width, image->height))
    writePixels (image->pixels, image->width * image->height);
}
//}}}

//{{{
void displayInit() {

  displayHwInit (USE_RES_PIN | USE_16BIT_DATA);

  writeCommandData (SSD1289_REG_OSCILLATION, 0x0001);
  writeCommandData (SSD1289_REG_POWER_CTRL_1, 0xA8A4);
  writeCommandData (SSD1289_REG_POWER_CTRL_2, 0x0000);
  writeCommandData (SSD1289_REG_POWER_CTRL_3, 0x080C);
  writeCommandData (SSD1289_REG_POWER_CTRL_4, 0x2B00);
  writeCommandData (SSD1289_REG_POWER_CTRL_5, 0x00B7);

  #ifdef LANDSCAPE
  writeCommandData (SSD1289_REG_DRIVER_OUT_CTRL,0x293F);
  #else
  writeCommandData (SSD1289_REG_DRIVER_OUT_CTRL,0x2B3F);
  #endif

  writeCommandData (SSD1289_REG_LCD_DRIVE_AC, 0x0600);
  writeCommandData (SSD1289_REG_SLEEP_MODE, 0x0000);

  #ifdef LANDSCAPE
  writeCommandData (SSD1289_REG_ENTRY_MODE, 0x6078);
  #else
  writeCommandData (SSD1289_REG_ENTRY_MODE, 0x6070);
  #endif

  writeCommandData (SSD1289_REG_DISPLAY_CTRL, 0x0233);
  writeCommandData (SSD1289_REG_FRAME_CYCLE, 0x0000);
  writeCommandData (SSD1289_REG_GATE_SCAN_START,0x0000);

  writeCommandData (SSD1289_REG_V_SCROLL_CTRL_1,0x0000);
  writeCommandData (SSD1289_REG_FIRST_WIN_START,0x0000);
  writeCommandData (SSD1289_REG_FIRST_WIN_END,  0x013F);

  writeCommandData (SSD1289_REG_GAMMA_CTRL_1, 0x0707);
  writeCommandData (SSD1289_REG_GAMME_CTRL_2, 0x0204);
  writeCommandData (SSD1289_REG_GAMMA_CTRL_3, 0x0204);
  writeCommandData (SSD1289_REG_GAMMA_CTRL_4, 0x0502);
  writeCommandData (SSD1289_REG_GAMMA_CTRL_5, 0x0507);
  writeCommandData (SSD1289_REG_GAMMA_CTRL_6, 0x0204);
  writeCommandData (SSD1289_REG_GAMMA_CTRL_7, 0x0204);
  writeCommandData (SSD1289_REG_GAMMA_CTRL_8, 0x0502);
  writeCommandData (SSD1289_REG_GAMMA_CTRL_9, 0x0302);
  writeCommandData (SSD1289_REG_GAMMA_CTRL_10, 0x0302);

  writeCommandData (SSD1289_REG_WR_DATA_MASK_1, 0x0000);
  writeCommandData (SSD1289_REG_WR_DATA_MASK_2, 0x0000);
  writeCommandData (SSD1289_REG_FRAME_FREQUENCY, 0x8000);
  }
//}}}
