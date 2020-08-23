// ili9341.c - LTDC and spi5 - SPI_DISPLAY, LANDSCAPE defines
//  PC02 - CS  - lo - can be left lo
//  PD13 - DC        (tft wrx)
//  PF07 - SCK  SPI5 (tft dcx)
//  PF09 - MOSI SPI5
//  PD11 - TE        (unused)
//  PD12 - RDX       (unused)
//  PF08 - MISO SPI5 (unused)
//  PA04 = VS
//  PC06 = HS
//  PF10 = DE
//  PG07 = CK
//  PC10 = R2   PA06 = G2   PD06 = B2
//  PB00 = R3   PG10 = G3   PG11 = B3
//  PA11 = R4   PB10 = G4   PG12 = B4
//  PA12 = R5   PB11 = G5   PA03 = B5
//  PB01 = R6   PC07 = G6   PB08 = B6
//  PG06 = R7   PD03 = G7   PB09 = B7
//{{{  includes
#include "display.h"
#include "delay.h"
#include "sdram.h"

#include "stm32f4xx.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_spi.h"
#include "stm32f4xx_ltdc.h"
#include "stm32f4xx_dma2d.h"
//}}}
//{{{  defines
#define LCD_RESET          0x01  // reset

#define LCD_SLEEP_OUT      0x11  // Sleep out register

#define LCD_GAMMA          0x26  // Gamma register

#define LCD_DISPLAY_OFF    0x28  // Display off register
#define LCD_DISPLAY_ON     0x29  // Display on register

#define LCD_COLUMN_ADDR    0x2A  // Colomn address register
#define LCD_PAGE_ADDR      0x2B  // Page address register
#define LCD_GRAM           0x2C  // GRAM register

#define LCD_MAC            0x36  // Memory Access Control register
#define LCD_PIXEL_FORMAT   0x3A  // Pixel Format register

#define LCD_WDB            0x51  // Write Brightness Display register
#define LCD_WCD            0x53  // Write Control Display register
#define LCD_CABC           0x55  // Write Content adaptive brightness reg
#define LCD_CABC_MIN       0x5E  // Write Conttent adaptive minimum brightness reg

#define LCD_RGB_INTERFACE  0xB0  // RGB Interface Signal Control
#define LCD_FRC            0xB1  // Frame Rate Control register
#define LCD_BPC            0xB5  // Blanking Porch Control register
#define LCD_DFC            0xB6  // Display Function Control register

#define LCD_POWER1         0xC0  // Power Control 1 register
#define LCD_POWER2         0xC1  // Power Control 2 register
#define LCD_VCOM1          0xC5  // VCOM Control 1 register
#define LCD_VCOM2          0xC7  // VCOM Control 2 register

#define LCD_xxx            0xCA  // ***

#define LCD_POWERA         0xCB  // Power control A register
#define LCD_POWERB         0xCF  // Power control B register
#define LCD_PGAMMA         0xE0  // Positive Gamma Correction register
#define LCD_NGAMMA         0xE1  // Negative Gamma Correction register

#define LCD_DTCA           0xE8  // Driver timing control A
#define LCD_DTCB           0xEA  // Driver timing control B

#define LCD_POWER_SEQ      0xED  // Power on sequence register
#define LCD_3GAMMA_EN      0xF2  // 3 Gamma enable register
#define LCD_INTERFACE      0xF6  // Interface control register

#define LCD_PRC            0xF7  // Pump ratio control register
//}}}
//#define USE_CS
//#define SPI_DISPLAY
//#define LANDSCAPE
//#define LAYER2
//{{{
#ifdef LANDSCAPE
  #define TFTWIDTH  320
  #define TFTHEIGHT 240
#else
  #define TFTWIDTH  240
  #define TFTHEIGHT 320
#endif
//}}}

#define FRAME_BUFFER   SDRAM
#define BUFFER_OFFSET  TFTWIDTH*TFTHEIGHT*2

#define BACKGROUND_LAYER  0x0000
#define FOREGROUND_LAYER  0x0001


#ifdef LAYER2
static uint32_t CurLayer = FOREGROUND_LAYER;
static uint32_t CurFrameBuffer = FRAME_BUFFER+ BUFFER_OFFSET;
#else
static uint32_t CurLayer = BACKGROUND_LAYER;
static uint32_t CurFrameBuffer = FRAME_BUFFER;
#endif

static bool dma2dWait = false;

//{{{
static void writeCommand (uint8_t command) {
  // DC lo
  GPIOD->BSRRH = GPIO_Pin_13;

  #ifdef USE_CS
  // CS lo
  GPIOC->BSRRH = GPIO_Pin_2;
  #endif

  SPI5->DR = command;
  while (!(SPI5->SR & SPI_I2S_FLAG_TXE));

  #ifdef USE_CS
  while (SPI5->SR & SPI_I2S_FLAG_BSY);

  // CS hi
  GPIOC->BSRRL = GPIO_Pin_2;
  #endif
  }
//}}}
//{{{
static void writeData (uint8_t value) {
  // DC hi
  GPIOD->BSRRL = GPIO_Pin_13;

  #ifdef USE_CS
  // CS lo
  GPIOC->BSRRH = GPIO_Pin_2;
  #endif

  SPI5->DR = value;
  while (!(SPI5->SR & SPI_I2S_FLAG_TXE));

  #ifdef USE_CS
  while (SPI5->SR & SPI_I2S_FLAG_BSY);

  // CS hi
  GPIOC->BSRRL = GPIO_Pin_2;
  #endif
  }
//}}}

//{{{
static void lcdSpiInit() {

  writeCommand (LCD_RESET);
  delayMs (5);
  //{{{
  writeCommand (0xCA);          // undocumented
  writeData (0xC3);
  writeData (0x08);
  writeData (0x50);
  //}}}
  //{{{
  writeCommand (LCD_POWERB);
  writeData (0x00);
  writeData (0xC1);
  writeData (0x30);
  //}}}
  //{{{
  writeCommand (LCD_POWER_SEQ); // undocumented
  writeData (0x64);
  writeData (0x03);
  writeData (0x12);
  writeData (0x81);
  //}}}
  //{{{
  writeCommand (LCD_DTCA);
  writeData (0x85);
  writeData (0x00);
  writeData (0x78);
  //}}}
  //{{{
  writeCommand (LCD_POWERA);
  writeData (0x39);
  writeData (0x2C);
  writeData (0x00);
  writeData (0x34);
  writeData (0x02);
  //}}}
  //{{{
  writeCommand (LCD_PRC);
  writeData (0x20);
  //}}}
  //{{{
  writeCommand (LCD_DTCB);
  writeData (0x00);
  writeData (0x00);
  //}}}
  //{{{
  writeCommand (LCD_FRC);
  writeData (0x00);
  writeData (0x1B);
  //}}}
  //{{{
  writeCommand (LCD_DFC);
  writeData (0x0A);
  writeData (0xA2);
  //}}}
  //{{{
  writeCommand (LCD_POWER1);
  writeData (0x10);
  //}}}
  //{{{
  writeCommand (LCD_POWER2);
  writeData (0x10);
  //}}}
  //{{{
  writeCommand (LCD_VCOM1);
  writeData (0x45);
  writeData (0x15);
  //}}}
  //{{{
  writeCommand (LCD_VCOM2);
  writeData (0x90);
  //}}}
  //{{{
  writeCommand (LCD_MAC);
  #ifdef LANDSCAPE
  writeData (0x68);
  #else
  writeData (0xc8);
  #endif
  //}}}
  //{{{
  writeCommand (LCD_PIXEL_FORMAT);
  writeData (0x55);
  //}}}
  //{{{
  writeCommand (LCD_COLUMN_ADDR);
  writeData (0x00);
  writeData (0x00);
  writeData (0x00);
  writeData (0xEF);
  //}}}
  //{{{
  writeCommand (LCD_PAGE_ADDR);
  writeData (0x00);
  writeData (0x00);
  writeData (0x01);
  writeData (0x3F);
  //}}}
  //{{{
  writeCommand (LCD_3GAMMA_EN); // undocumented
  writeData (0x00);
  //}}}
  //{{{
  writeCommand (LCD_GAMMA);
  writeData (0x01);
  //}}}
  //{{{
  writeCommand (LCD_PGAMMA);
  writeData (0x0F);
  writeData (0x29);
  writeData (0x24);
  writeData (0x0C);
  writeData (0x0E);
  writeData (0x09);
  writeData (0x4E);
  writeData (0x78);
  writeData (0x3C);
  writeData (0x09);
  writeData (0x13);
  writeData (0x05);
  writeData (0x17);
  writeData (0x11);
  writeData (0x00);

  //writeCommand (LCD_PGAMMA);
  //writeData (0x0F);
  //writeData (0x31);
  //writeData (0x2B);
  //writeData (0x0C);
  //writeData (0x0E);
  //writeData (0x08);
  //writeData (0x4E);
  //writeData (0xF1);
  //writeData (0x37);
  //writeData (0x07);
  //writeData (0x10);
  //writeData (0x03);
  //writeData (0x0E);
  //writeData (0x09);
  //writeData (0x00);
  //}}}
  //{{{
  writeCommand (LCD_NGAMMA);
  writeData (0x00);
  writeData (0x16);
  writeData (0x1B);
  writeData (0x04);
  writeData (0x11);
  writeData (0x07);
  writeData (0x31);
  writeData (0x33);
  writeData (0x42);
  writeData (0x05);
  writeData (0x0C);
  writeData (0x0A);
  writeData (0x28);
  writeData (0x2F);
  writeData (0x0F);

  //alternative gamma
  //writeCommand (LCD_NGAMMA);
  //writeData (0x00);
  //writeData (0x0E);
  //writeData (0x14);
  //writeData (0x03);
  //writeData (0x11);
  //writeData (0x07);
  //writeData (0x31);
  //writeData (0xC1);
  //writeData (0x48);
  //writeData (0x08);
  //writeData (0x0F);
  //writeData (0x0C);
  //writeData (0x31);
  //writeData (0x36);
  //writeData (0x0F);
  //}}}
  writeCommand (LCD_SLEEP_OUT);
  delayMs (120);
  writeCommand (LCD_DISPLAY_ON);
  }
//}}}
//{{{
static void lcdLtdcInit() {

  writeCommand (LCD_RESET);
  delayMs (5);
  //{{{
  writeCommand (0xCA);
  writeData (0xC3);
  writeData (0x08);
  writeData (0x50);
  //}}}
  //{{{
  writeCommand (LCD_POWERB);
  writeData (0x00);
  writeData (0xC1);
  writeData (0x30);
  //}}}
  //{{{
  writeCommand (LCD_POWER_SEQ);
  writeData (0x64);
  writeData (0x03);
  writeData (0x12);
  writeData (0x81);
  //}}}
  //{{{
  writeCommand (LCD_DTCA);
  writeData (0x85);
  writeData (0x00);
  writeData (0x78);
  //}}}
  //{{{
  writeCommand (LCD_POWERA);
  writeData (0x39);
  writeData (0x2C);
  writeData (0x00);
  writeData (0x34);
  writeData (0x02);
  //}}}
  //{{{
  writeCommand (LCD_PRC);
  writeData (0x20);
  //}}}
  //{{{
  writeCommand (LCD_DTCB);
  writeData (0x00);
  writeData (0x00);
  //}}}
  //{{{
  writeCommand (LCD_FRC);
  writeData (0x00);
  writeData (0x1B);
  //}}}
  //{{{
  writeCommand (LCD_DFC);
  writeData (0x0A);
  writeData (0xA2);
  //}}}
  //{{{
  writeCommand (LCD_POWER1);
  writeData (0x10);
  //}}}
  //{{{
  writeCommand (LCD_POWER2);
  writeData (0x10);
  //}}}
  //{{{
  writeCommand (LCD_VCOM1);
  writeData (0x45);
  writeData (0x15);
  //}}}
  //{{{
  writeCommand (LCD_VCOM2);
  writeData (0x90);
  //}}}
  //{{{
  writeCommand (LCD_MAC);
  #ifdef LANDSCAPE
  writeData (0x68);
  #else
  writeData (0xc8);
  #endif
  //}}}
  //{{{
  writeCommand (LCD_3GAMMA_EN);
  writeData (0x00);
  //}}}
  //{{{
  writeCommand (LCD_RGB_INTERFACE);
  writeData (0xC2);
  //}}}
  //{{{
  writeCommand (LCD_DFC);
  writeData (0x0A);
  writeData (0xA7);
  writeData (0x27);
  writeData (0x04);
  //}}}
  //{{{
  writeCommand (LCD_COLUMN_ADDR);
  writeData (0x00);
  writeData (0x00);
  writeData (0x00);
  writeData (0xEF);
  //}}}
  //{{{
  writeCommand (LCD_PAGE_ADDR);
  writeData (0x00);
  writeData (0x00);
  writeData (0x01);
  writeData (0x3F);
  //}}}
  //{{{
  writeCommand (LCD_INTERFACE);
  writeData (0x01);
  writeData (0x00);
  writeData (0x06);
  //}}}
  //{{{
  writeCommand (LCD_GAMMA);
  writeData (0x01);
  //}}}
  //{{{
  writeCommand (LCD_PGAMMA);
  writeData (0x0F);
  writeData (0x29);
  writeData (0x24);
  writeData (0x0C);
  writeData (0x0E);
  writeData (0x09);
  writeData (0x4E);
  writeData (0x78);
  writeData (0x3C);
  writeData (0x09);
  writeData (0x13);
  writeData (0x05);
  writeData (0x17);
  writeData (0x11);
  writeData (0x00);
  //}}}
  //{{{
  writeCommand (LCD_NGAMMA);
  writeData (0x00);
  writeData (0x16);
  writeData (0x1B);
  writeData (0x04);
  writeData (0x11);
  writeData (0x07);
  writeData (0x31);
  writeData (0x33);
  writeData (0x42);
  writeData (0x05);
  writeData (0x0C);
  writeData (0x0A);
  writeData (0x28);
  writeData (0x2F);
  writeData (0x0F);
  //}}}
  writeCommand (LCD_SLEEP_OUT);
  delayMs (120);
  writeCommand (LCD_DISPLAY_ON);

  // GRAM start writing
  writeCommand (LCD_GRAM);
  }
//}}}

//{{{
void lcdSpiDisplay (int16_t xorg, int16_t yorg, uint16_t xlen, uint16_t ylen) {

  uint16_t xend = xorg + xlen - 1;
  uint16_t yend = yorg + ylen - 1;

  if ((xend < TFTWIDTH) && (yend < TFTHEIGHT)) {
    writeCommand (LCD_COLUMN_ADDR);
    writeData (xorg >> 8);
    writeData (xorg);
    writeData (xend >> 8);
    writeData (xend);

    writeCommand (LCD_PAGE_ADDR);
    writeData (yorg >> 8);
    writeData (yorg);
    writeData (yend >> 8);
    writeData (yend);

    writeCommand (LCD_GRAM);

    #ifdef USE_CS
    GPIOC->BSRRH = GPIO_Pin_2; // CS lo
    #endif

    GPIOD->BSRRL = GPIO_Pin_13; // DC hi

    uint16_t* pixels = (uint16_t*)(CurFrameBuffer) + (yorg*TFTWIDTH) + xorg;
    for (uint16_t y = 0; y < ylen; y++) {
      for (uint16_t x = 0; x < xlen; x++) {
        SPI5->DR = *pixels >> 8;
        while (!(SPI5->SR & SPI_I2S_FLAG_TXE));
        SPI5->DR = *pixels++;
        while (!(SPI5->SR & SPI_I2S_FLAG_TXE));
        }
      pixels += TFTWIDTH - xlen;
      }

    #ifdef USE_CS
    while (SPI5->SR & SPI_I2S_FLAG_BSY);

    // CS hi
    GPIOC->BSRRL = GPIO_Pin_2;
    #endif
    }
  }
//}}}
//{{{
void lcdSpiDisplay1 (uint32_t frameBuffer, int16_t xorg, int16_t yorg, uint16_t xpitch) {

  uint16_t xend = TFTWIDTH - 1;
  uint16_t yend = TFTHEIGHT - 1;

  writeCommand (LCD_COLUMN_ADDR);
  writeData (0);
  writeData (0);
  writeData (xend >> 8);
  writeData (xend);

  writeCommand (LCD_PAGE_ADDR);
  writeData (0);
  writeData (0);
  writeData (yend >> 8);
  writeData (yend);

  writeCommand (LCD_GRAM);

  #ifdef USE_CS
  GPIOC->BSRRH = GPIO_Pin_2; // CS lo
  #endif

  GPIOD->BSRRL = GPIO_Pin_13; // DC hi

  uint16_t* pixels = (uint16_t*)(frameBuffer) + (yorg*xpitch) + xorg;
  for (uint16_t y = 0; y < TFTHEIGHT; y++) {
    for (uint16_t x = 0; x < TFTWIDTH; x++) {
      SPI5->DR = *pixels >> 8;
      while (!(SPI5->SR & SPI_I2S_FLAG_TXE));
      SPI5->DR = *pixels++;
      while (!(SPI5->SR & SPI_I2S_FLAG_TXE));
      }
    pixels += xpitch - TFTWIDTH;
    }

  #ifdef USE_CS
  while (SPI5->SR & SPI_I2S_FLAG_BSY);

  // CS hi
  GPIOC->BSRRL = GPIO_Pin_2;
  #endif
  }
//}}}
//{{{
void lcdSpiDisplay2 (uint32_t frameBuffer, int16_t xorg, int16_t yorg, uint16_t xpitch) {

  uint16_t xend = xorg + TFTWIDTH - 1;
  uint16_t yend = yorg + TFTHEIGHT - 1;

  if ((xend < TFTWIDTH) && (yend < TFTHEIGHT)) {
    writeCommand (LCD_COLUMN_ADDR);
    writeData (xorg >> 8);
    writeData (xorg);
    writeData (xend >> 8);
    writeData (xend);

    writeCommand (LCD_PAGE_ADDR);
    writeData (yorg >> 8);
    writeData (yorg);
    writeData (yend >> 8);
    writeData (yend);

    writeCommand (LCD_GRAM);

    #ifdef USE_CS
    GPIOC->BSRRH = GPIO_Pin_2; // CS lo
    #endif

    GPIOD->BSRRL = GPIO_Pin_13; // DC hi

    uint8_t* pixels = (uint8_t*)(frameBuffer) + ((yorg*TFTWIDTH) + xorg)*2;
    for (uint16_t y = 0; y < TFTHEIGHT; y++) {
      for (uint16_t x = 0; x < TFTWIDTH*2; x++) {
        SPI5->DR = *pixels++;
        while (!(SPI5->SR & SPI_I2S_FLAG_TXE));
        }
      pixels += (TFTWIDTH - xpitch)*2;
      }

    #ifdef USE_CS
    while (SPI5->SR & SPI_I2S_FLAG_BSY);

    // CS hi
    GPIOC->BSRRL = GPIO_Pin_2;
    #endif
    }
  }
//}}}
//{{{
void lcdSpiDisplayMono (uint32_t frameBuffer, int16_t xorg, int16_t yorg, uint16_t xpitch) {

  uint16_t xend = TFTWIDTH - 1;
  uint16_t yend = TFTHEIGHT - 1;

  writeCommand (LCD_COLUMN_ADDR);
  writeData (0);
  writeData (0);
  writeData (xend >> 8);
  writeData (xend);

  writeCommand (LCD_PAGE_ADDR);
  writeData (0);
  writeData (0);
  writeData (yend >> 8);
  writeData (yend);

  writeCommand (LCD_GRAM);

  #ifdef USE_CS
  GPIOC->BSRRH = GPIO_Pin_2; // CS lo
  #endif

  GPIOD->BSRRL = GPIO_Pin_13; // DC hi

  uint8_t* pixels = (uint8_t*)(frameBuffer) + (yorg*xpitch) + xorg;
  for (uint16_t y = 0; y < TFTHEIGHT; y++) {
    for (uint16_t x = 0; x < TFTWIDTH; x++) {
      uint16_t luma = *pixels++;
      //uint16_t luma = (x * 255) / TFTWIDTH;
      uint8_t bg = (luma & 0xF8) | (luma >> 5);        // b:7:3 : g:7:5
      uint8_t gr = ((luma << 3) & 0xE0) | (luma >> 3); // g:4:2 : r:7:3

      SPI5->DR = bg;
      while (!(SPI5->SR & SPI_I2S_FLAG_TXE));

      SPI5->DR = gr;
      while (!(SPI5->SR & SPI_I2S_FLAG_TXE));
      }

    pixels += xpitch - TFTWIDTH;
    }

  #ifdef USE_CS
  while (SPI5->SR & SPI_I2S_FLAG_BSY);

  // CS hi
  GPIOC->BSRRL = GPIO_Pin_2;
  #endif
  }
//}}}

//{{{
void setLayer (uint32_t Layerx) {

  if (Layerx == BACKGROUND_LAYER) {
    CurFrameBuffer = FRAME_BUFFER;
    CurLayer = BACKGROUND_LAYER;
    }

  else {
    CurFrameBuffer = FRAME_BUFFER + BUFFER_OFFSET;
    CurLayer = FOREGROUND_LAYER;
    }
  }
//}}}
//{{{
void setTransparency (uint8_t transparency) {

  if (CurLayer == BACKGROUND_LAYER)
    LTDC_LayerAlpha(LTDC_Layer1, transparency);
  else
    LTDC_LayerAlpha(LTDC_Layer2, transparency);

  LTDC_ReloadConfig(LTDC_IMReload);
  }
//}}}
//{{{
void setColorKeying (uint32_t RGBValue) {

  // configure the color Keying
  LTDC_ColorKeying_InitTypeDef LTDC_colorkeying_InitStruct;
  LTDC_colorkeying_InitStruct.LTDC_ColorKeyBlue = 0x0000FF & RGBValue;
  LTDC_colorkeying_InitStruct.LTDC_ColorKeyGreen = (0x00FF00 & RGBValue) >> 8;
  LTDC_colorkeying_InitStruct.LTDC_ColorKeyRed = (0xFF0000 & RGBValue) >> 16;

  if (CurLayer == BACKGROUND_LAYER) {
    /* Enable the color Keying for Layer1 */
    LTDC_ColorKeyingConfig (LTDC_Layer1, &LTDC_colorkeying_InitStruct, ENABLE);
    LTDC_ReloadConfig (LTDC_IMReload);
    }
  else {
    /* Enable the color Keying for Layer2 */
    LTDC_ColorKeyingConfig (LTDC_Layer2, &LTDC_colorkeying_InitStruct, ENABLE);
    LTDC_ReloadConfig (LTDC_IMReload);
    }
  }
//}}}
//{{{
void reSetColorKeying() {

  LTDC_ColorKeying_InitTypeDef LTDC_colorkeying_InitStruct;

  if (CurLayer == BACKGROUND_LAYER) {
    /* Disable the color Keying for Layer1 */
    LTDC_ColorKeyingConfig(LTDC_Layer1, &LTDC_colorkeying_InitStruct, DISABLE);
    LTDC_ReloadConfig(LTDC_IMReload);
    }
  else {
    /* Disable the color Keying for Layer2 */
    LTDC_ColorKeyingConfig(LTDC_Layer2, &LTDC_colorkeying_InitStruct, DISABLE);
    LTDC_ReloadConfig(LTDC_IMReload);
    }
  }
//}}}
//{{{
void setDisplayWindow (uint16_t Xpos, uint16_t Ypos, uint16_t Height, uint16_t Width) {

  if (CurLayer == BACKGROUND_LAYER) {
    // reconfigure the layer1 position
    LTDC_LayerPosition (LTDC_Layer1, Xpos, Ypos);
    LTDC_ReloadConfig (LTDC_IMReload);

    // reconfigure the layer1 size
    LTDC_LayerSize (LTDC_Layer1, Width, Height);
    LTDC_ReloadConfig (LTDC_IMReload);
    }

  else {
    // reconfigure the layer2 position
    LTDC_LayerPosition (LTDC_Layer2, Xpos, Ypos);
    LTDC_ReloadConfig (LTDC_IMReload);

    // reconfigure the layer2 size
    LTDC_LayerSize (LTDC_Layer2, Width, Height);
    LTDC_ReloadConfig (LTDC_IMReload);
    }
  }
//}}}
//{{{
void windowModeDisable() {

  setDisplayWindow (0, 0, TFTHEIGHT, TFTWIDTH);
  }
//}}}

bool getMono() { return false; }
uint16_t getWidth() { return TFTWIDTH; }
uint16_t getHeight() { return TFTHEIGHT; }

void setBitPixels (uint16_t colour, int16_t xorg, int16_t yorg, uint16_t xlen, uint16_t ylen) {}
void drawLines (uint16_t yorg, uint16_t yend) {}

//{{{
void setPixel (uint16_t colour, int16_t x, int16_t y) {

  *((uint16_t*)(CurFrameBuffer) + (y*TFTWIDTH) + x) = colour;

  #ifdef SPI_DISPLAY
    lcdSpiDisplay (x, y, 1, 1);
  #endif
  }
//}}}
//{{{
void setTTPixels (uint16_t colour, uint8_t* bytes, int16_t x, int16_t y, uint16_t pitch, uint16_t rows) {

  #ifndef SPI_DISPLAY
    if (dma2dWait) {
      while (!(DMA2D->ISR & DMA2D_FLAG_TC)) {}
      DMA2D->IFCR |= DMA2D_IFSR_CTEIF | DMA2D_IFSR_CTCIF | DMA2D_IFSR_CTWIF|
                     DMA2D_IFSR_CCAEIF | DMA2D_IFSR_CCTCIF | DMA2D_IFSR_CCEIF;
      dma2dWait = false;
      }
  #endif

  // Config dma2d - memory to memory with blending
  DMA2D->CR = (DMA2D->CR & 0xFFFCE0FC) | DMA2D_M2M_BLEND;
  DMA2D->OPFCCR = DMA2D_RGB565;
  DMA2D->OCOLR = 0;
  DMA2D->OMAR = CurFrameBuffer + ((y*TFTWIDTH) + x)*2;
  DMA2D->OOR = TFTWIDTH - pitch;
  DMA2D->NLR = (pitch << 16) | rows;

  DMA2D->FGMAR = (uint32_t)bytes;
  DMA2D->FGOR = 0;
  DMA2D->FGPFCCR = CM_A8;
  DMA2D->FGCOLR = (colour & 0xF800)<<8 | (colour & 0x07E0)<<5 | (colour & 0x001f)<<3;
  DMA2D->FGCMAR = 0;

  DMA2D->BGMAR = CurFrameBuffer + ((y * TFTWIDTH) + x)*2;
  DMA2D->BGOR = TFTWIDTH - pitch;
  DMA2D->BGPFCCR = CM_RGB565;
  DMA2D->BGCOLR = 0;
  DMA2D->BGCMAR = 0;

  // start dma2d
  DMA2D->CR |= DMA2D_CR_START;

  #ifdef SPI_DISPLAY
    while (!(DMA2D->ISR & DMA2D_FLAG_TC)) {}
    DMA2D->IFCR |= DMA2D_IFSR_CTEIF | DMA2D_IFSR_CTCIF | DMA2D_IFSR_CTWIF|
                   DMA2D_IFSR_CCAEIF | DMA2D_IFSR_CCTCIF | DMA2D_IFSR_CCEIF;
    lcdSpiDisplay (x, y, pitch, rows);
  #else
    dma2dWait = true;
  #endif
  }
//}}}
//{{{
void drawRect (uint16_t colour, int16_t xorg, int16_t yorg, uint16_t xlen, uint16_t ylen) {

  #ifndef SPI_DISPLAY
    if (dma2dWait) {
      while (!(DMA2D->ISR & DMA2D_FLAG_TC)) {}
      DMA2D->IFCR |= DMA2D_IFSR_CTEIF | DMA2D_IFSR_CTCIF | DMA2D_IFSR_CTWIF|
                     DMA2D_IFSR_CCAEIF | DMA2D_IFSR_CCTCIF | DMA2D_IFSR_CCEIF;
      dma2dWait = false;
      }
  #endif

  // config dma2d - register to memory
  DMA2D->CR = (DMA2D->CR & 0xFFFCE0FC) | DMA2D_R2M;          // reset flags, set mode
  DMA2D->OPFCCR = DMA2D_RGB565;                              // colour format
  DMA2D->OCOLR = colour;                                     // fixed colour
  DMA2D->OMAR = CurFrameBuffer + ((yorg*TFTWIDTH) + xorg)*2; // output memory address
  DMA2D->OOR = TFTWIDTH - xlen;                              // line offset
  DMA2D->NLR = (xlen << 16) | ylen;                          // xlen:ylen

  // start dma2d
  DMA2D->CR |= DMA2D_CR_START;

  #ifdef SPI_DISPLAY
    while (!(DMA2D->ISR & DMA2D_FLAG_TC)) {}
    DMA2D->IFCR |= DMA2D_IFSR_CTEIF | DMA2D_IFSR_CTCIF | DMA2D_IFSR_CTWIF|
                   DMA2D_IFSR_CCAEIF | DMA2D_IFSR_CCTCIF | DMA2D_IFSR_CCEIF;
    lcdSpiDisplay (xorg, yorg, xlen, ylen);
  #else
    dma2dWait = true;
  #endif
  }
//}}}
//{{{
void drawImage (image_t* image, int16_t xorg, int16_t yorg) {

  uint16_t* fromPixels = image->pixels;
  uint16_t* toPixels = (uint16_t*)(CurFrameBuffer) + (yorg*TFTWIDTH) + xorg;

  for (uint16_t y = 0; y < image->height; y++) {
    for (uint16_t x = 0; x < image->width; x++)
      *toPixels++ = *fromPixels++;
    toPixels += TFTWIDTH - image->width;
    }

  #ifdef SPI_DISPLAY
    lcdSpiDisplay (xorg, yorg, image->width, image->height);
  #endif
  }
//}}}

//{{{
void displayInit() {
  // enable clocks
  RCC->AHB1ENR |= RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOC |
                  RCC_AHB1Periph_GPIOD | RCC_AHB1Periph_GPIOF | RCC_AHB1Periph_GPIOG |
                  RCC_AHB1Periph_DMA2D;
  RCC->APB2ENR |= RCC_APB2Periph_LTDC | RCC_APB2Periph_SPI5;

  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;

  #ifdef USE_CS
    // config CS GPIO
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2;
    GPIO_Init (GPIOC, &GPIO_InitStruct);
    GPIOC->BSRRL = GPIO_Pin_2; // init hi
  #endif

  // config DC GPIO, init lo
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_13;
  GPIO_Init (GPIOD, &GPIO_InitStruct);
  GPIOD->BSRRH = GPIO_Pin_13;

  // config SPI SCK, MOSI GPIO pins
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_9;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
  GPIO_Init (GPIOF, &GPIO_InitStruct);

  GPIO_PinAFConfig (GPIOF, GPIO_PinSource7, GPIO_AF_SPI5);
  GPIO_PinAFConfig (GPIOF, GPIO_PinSource9, GPIO_AF_SPI5);

  // SPI - mode0, 48Mhz
  SPI_InitTypeDef SPI_InitStruct;
  SPI_InitStruct.SPI_Direction = SPI_Direction_1Line_Tx;
  SPI_InitStruct.SPI_Mode = SPI_Mode_Master;
  SPI_InitStruct.SPI_DataSize = SPI_DataSize_8b;
  SPI_InitStruct.SPI_CPOL = SPI_CPOL_Low;
  SPI_InitStruct.SPI_CPHA = SPI_CPHA_1Edge;
  SPI_InitStruct.SPI_NSS = SPI_NSS_Soft;
  SPI_InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
  SPI_InitStruct.SPI_FirstBit = SPI_FirstBit_MSB;
  SPI_InitStruct.SPI_CRCPolynomial = 7;
  SPI_Init(SPI5, &SPI_InitStruct);

  SPI_Cmd (SPI5, ENABLE);

  #ifdef SPI_DISPLAY
    lcdSpiInit();
  #else
    //{{{  config PLLSAI clock
    // PLLSAI_VCO In  = HSE_VALUE/PLL_M = 1 Mhz
    // PLLSAI_VCO Out = PLLSAI_VCO In * PLLSAI_N = 192 Mhz
    // PLLLCDCLK      = PLLSAI_VCO Out / PLLSAI_R = 192/4 = 48 Mhz
    // LTDC clock     = PLLLCDCLK / RCC_PLLSAIDivR = 48/8 = 6 Mhz
    RCC_PLLSAIConfig (192, 7, 4);
    RCC_LTDCCLKDivConfig (RCC_PLLSAIDivR_Div8);

    // Enable PLLSAI Clock, wait for PLLSAI activation
    RCC_PLLSAICmd (ENABLE);
    while (RCC_GetFlagStatus (RCC_FLAG_PLLSAIRDY) == RESET) {}
    //}}}
    //{{{  config GPIOA LTDC pins
    GPIO_PinAFConfig (GPIOA, GPIO_PinSource3, GPIO_AF_LTDC);
    GPIO_PinAFConfig (GPIOA, GPIO_PinSource4, GPIO_AF_LTDC);
    GPIO_PinAFConfig (GPIOA, GPIO_PinSource6, GPIO_AF_LTDC);
    GPIO_PinAFConfig (GPIOA, GPIO_PinSource11, GPIO_AF_LTDC);
    GPIO_PinAFConfig (GPIOA, GPIO_PinSource12, GPIO_AF_LTDC);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_6 | GPIO_Pin_11 | GPIO_Pin_12;
    GPIO_Init (GPIOA, &GPIO_InitStruct);
    //}}}
    //{{{  config GPIOB LTDC pins
    GPIO_PinAFConfig (GPIOB, GPIO_PinSource0, 0x09);
    GPIO_PinAFConfig (GPIOB, GPIO_PinSource1, 0x09);
    GPIO_PinAFConfig (GPIOB, GPIO_PinSource8, GPIO_AF_LTDC);
    GPIO_PinAFConfig (GPIOB, GPIO_PinSource9, GPIO_AF_LTDC);
    GPIO_PinAFConfig (GPIOB, GPIO_PinSource10, GPIO_AF_LTDC);
    GPIO_PinAFConfig (GPIOB, GPIO_PinSource11, GPIO_AF_LTDC);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_8 |
                               GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_Init (GPIOB, &GPIO_InitStruct);
    //}}}
    //{{{  config GPIOC LTDC pins
    GPIO_PinAFConfig (GPIOC, GPIO_PinSource6, GPIO_AF_LTDC);
    GPIO_PinAFConfig (GPIOC, GPIO_PinSource7, GPIO_AF_LTDC);
    GPIO_PinAFConfig (GPIOC, GPIO_PinSource10, GPIO_AF_LTDC);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_10;
    GPIO_Init (GPIOC, &GPIO_InitStruct);
    //}}}
    //{{{  config GPIOD LTDC pins
    GPIO_PinAFConfig (GPIOD, GPIO_PinSource3, GPIO_AF_LTDC);
    GPIO_PinAFConfig (GPIOD, GPIO_PinSource6, GPIO_AF_LTDC);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_6;
    GPIO_Init (GPIOD, &GPIO_InitStruct);
    //}}}
    //{{{  config GPIOF LTDC pins
    GPIO_PinAFConfig (GPIOF, GPIO_PinSource10, GPIO_AF_LTDC);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;
    GPIO_Init (GPIOF, &GPIO_InitStruct);
    //}}}
    //{{{  config GPIOG LTDC pins, strange af_ltdc mapping for PG10 PG12
    GPIO_PinAFConfig (GPIOG, GPIO_PinSource6, GPIO_AF_LTDC);
    GPIO_PinAFConfig (GPIOG, GPIO_PinSource7, GPIO_AF_LTDC);
    GPIO_PinAFConfig (GPIOG, GPIO_PinSource10, 0x09);
    GPIO_PinAFConfig (GPIOG, GPIO_PinSource11, GPIO_AF_LTDC);
    GPIO_PinAFConfig (GPIOG, GPIO_PinSource12, 0x09);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12;
    GPIO_Init (GPIOG, &GPIO_InitStruct);
    //}}}
    //{{{  config LTDC
    LTDC_InitTypeDef LTDC_InitStruct;
    LTDC_InitStruct.LTDC_HSPolarity = LTDC_HSPolarity_AL;
    LTDC_InitStruct.LTDC_VSPolarity = LTDC_VSPolarity_AL;
    LTDC_InitStruct.LTDC_DEPolarity = LTDC_DEPolarity_AL;
    LTDC_InitStruct.LTDC_PCPolarity = LTDC_PCPolarity_IPC;
    LTDC_InitStruct.LTDC_BackgroundRedValue = 0;
    LTDC_InitStruct.LTDC_BackgroundGreenValue = 0;
    LTDC_InitStruct.LTDC_BackgroundBlueValue = 0;
    LTDC_InitStruct.LTDC_HorizontalSync = 9;
    LTDC_InitStruct.LTDC_VerticalSync = 1;
    LTDC_InitStruct.LTDC_AccumulatedHBP = 29;
    LTDC_InitStruct.LTDC_AccumulatedVBP = 3;
    LTDC_InitStruct.LTDC_AccumulatedActiveW = 269;
    LTDC_InitStruct.LTDC_AccumulatedActiveH = 323;
    LTDC_InitStruct.LTDC_TotalWidth = 279;
    LTDC_InitStruct.LTDC_TotalHeigh = 327;
    LTDC_Init (&LTDC_InitStruct);

    // all the active display area is used to display a picture then :
    // - Horizontal start = horizontal synchronization + Horizontal back porch = 30
    // - Horizontal stop  = Horizontal start + window width -1 = 30 + 240 -1
    LTDC_Layer_InitTypeDef LTDC_Layer_InitStruct;
    LTDC_Layer_InitStruct.LTDC_HorizontalStart = 30;
    LTDC_Layer_InitStruct.LTDC_HorizontalStop = (TFTWIDTH + 30 - 1);

    // - Vertical start   = vertical synchronization + vertical back porch     = 4
    // - Vertical stop    = Vertical start + window height -1  = 4 + 320 -1
    LTDC_Layer_InitStruct.LTDC_VerticalStart = 4;
    LTDC_Layer_InitStruct.LTDC_VerticalStop = (TFTHEIGHT + 4 - 1);

    // Pixel Format
    LTDC_Layer_InitStruct.LTDC_PixelFormat = LTDC_Pixelformat_RGB565;
    LTDC_Layer_InitStruct.LTDC_ConstantAlpha = 255;

    // Default Color, A opaque
    LTDC_Layer_InitStruct.LTDC_DefaultColorBlue = 0;
    LTDC_Layer_InitStruct.LTDC_DefaultColorGreen = 0;
    LTDC_Layer_InitStruct.LTDC_DefaultColorRed = 0;
    LTDC_Layer_InitStruct.LTDC_DefaultColorAlpha = 0;

    // the length of one line of pixels in bytes + 3
    LTDC_Layer_InitStruct.LTDC_CFBLineLength = (TFTWIDTH * 2) + 3;
    LTDC_Layer_InitStruct.LTDC_CFBPitch = TFTWIDTH * 2;
    LTDC_Layer_InitStruct.LTDC_CFBLineNumber = TFTHEIGHT;

    LTDC_Layer_InitStruct.LTDC_BlendingFactor_1 = LTDC_BlendingFactor1_CA;
    LTDC_Layer_InitStruct.LTDC_BlendingFactor_2 = LTDC_BlendingFactor2_CA;
    LTDC_Layer_InitStruct.LTDC_CFBStartAdress = FRAME_BUFFER;
    LTDC_LayerInit (LTDC_Layer1, &LTDC_Layer_InitStruct);

    #ifdef LAYER2
    LTDC_Layer_InitStruct.LTDC_CFBStartAdress = FRAME_BUFFER + BUFFER_OFFSET;
    LTDC_Layer_InitStruct.LTDC_BlendingFactor_1 = LTDC_BlendingFactor1_PAxCA;
    LTDC_Layer_InitStruct.LTDC_BlendingFactor_2 = LTDC_BlendingFactor2_PAxCA;
    LTDC_LayerInit (LTDC_Layer2, &LTDC_Layer_InitStruct);
    #endif

    // LTDC configuration reload
    LTDC_ReloadConfig (LTDC_IMReload);

    // Enable layers
    LTDC_LayerCmd (LTDC_Layer1, ENABLE);

    #ifdef LAYER2
    LTDC_LayerCmd (LTDC_Layer2, ENABLE);
    #endif

    LTDC_ReloadConfig (LTDC_IMReload);
    //LTDC_DitherCmd (ENABLE);
    LTDC_Cmd (ENABLE);
    //}}}
    lcdLtdcInit();
  #endif
  }
//}}}
