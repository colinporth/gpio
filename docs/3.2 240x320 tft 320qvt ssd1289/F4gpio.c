// F4gpio.c - same pins as FSMC
//{{{  includes
#include "displayHw.h"

#include <stdio.h>

#include "stm32f4xx.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"

void delayMs (__IO uint32_t ms);
//}}}
//  PD04 = NOE = RD hi
//  PD05 = NWE = WR hi
//  PD07 = NE1 = CS hi
//  PD11 = A16 = DC lo
//  PE01 = RES hi
//  PD14:15 - D0:1
//  PD00:01 - D2:3
//  PE07:15 - D4:12
//  PD08:19 - D13:15
#define RD_PIN   GPIO_Pin_4
#define WR_PIN   GPIO_Pin_5
#define CS_PIN   GPIO_Pin_7
#define DC_PIN   GPIO_Pin_11
#define RES_PIN  GPIO_Pin_1

static uint16_t options;

//{{{
static inline uint16_t readValue() {

  // DO:7 inputs
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_14 | GPIO_Pin_15;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
  GPIO_Init (GPIOD, &GPIO_InitStruct);
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
  GPIO_Init (GPIOE, &GPIO_InitStruct);

  // read GPIOD, GPIOE
  GPIOD->BSRRH = GPIO_Pin_4;  // RD lo
  uint16_t dataD = GPIOD->IDR & 0xC703;
  uint16_t dataE = GPIOE->IDR & 0xFF80;
  GPIOD->BSRRL = GPIO_Pin_4;  // RD hi

  // DO:7 outputsinputs
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_14 | GPIO_Pin_15;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_Init (GPIOD, &GPIO_InitStruct);
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
  GPIO_Init (GPIOE, &GPIO_InitStruct);

  return (dataD >> 14) | ((dataD & 0x0003) << 2) | ((dataD & 0x0700) << 5) |
         ((dataE &0xFF80) >> 3);
  }
//}}}
//{{{
static inline void writeValue (uint16_t value) {
// extra mask need for 8 bit mode ***

  // write gpioE pins
  GPIOE->BSRRL =  (value << 3) & 0xFF80; // D4:12 -> PE7:15
  GPIOE->BSRRH = (~value << 3) & 0xFF80;

  // shift to gpioD pins
  uint16_t valued = ((value & 0x0003) << 14) | ((value & 0xE000) >> 5) | ((value & 0x000C) >> 2);

  // write gpioD pins
  GPIOD->BSRRL =  valued & 0xC703; // D0:3,13:15 -> PE0:1,7:9,14:15
  GPIOD->BSRRH = ~valued & 0xC703;

  // strobe WR lo
  GPIOD->BSRRH = GPIO_Pin_5;  // PD5 WR lo
  GPIOD->BSRRL = GPIO_Pin_5;  // PD5 WR hi
  }
//}}}

//{{{
uint16_t readStatus(void) {

  GPIOD->BSRRH = DC_PIN; // DC lo
  uint16_t data = readValue();
  return data;
}
//}}}
//{{{
uint16_t readData(void) {

  GPIOD->BSRRL = DC_PIN; // DC hi
  uint16_t data = readValue();
  return data;
}
//}}}

//{{{
void writeCommand (uint16_t command) {

  GPIOD->BSRRH = DC_PIN; // DC lo
  writeValue (command);
  }
//}}}
//{{{
void writeData (uint16_t data) {

  GPIOD->BSRRL = DC_PIN; // DC hi
  writeValue (data);
  }
//}}}
//{{{
void writeCommandData (uint16_t command, uint16_t data) {

  writeCommand (command);
  writeData (data);
  }
//}}}

//{{{
void writeColour (uint16_t colour, uint32_t length) {

  GPIOD->BSRRL = DC_PIN; // DC hi
  for (int i = 0; i < length; i++)
    writeValue (colour);
  }
//}}}
//{{{
void writePixels (uint16_t* pixels, uint32_t length) {

  GPIOD->BSRRL = DC_PIN; // DC hi
  for (int i = 0; i < length; i++)
    writeValue (*pixels++);
  }
//}}}

//{{{
void writeBytes (uint8_t* bytes, uint32_t length) {

  GPIOD->BSRRL = DC_PIN; // DC hi
  for (int i = 0; i < length; i++)
    writeValue (*bytes++);
  }
//}}}
//{{{
void writeColourBytes (uint16_t colour, uint32_t length) {

  GPIOD->BSRRL = DC_PIN; // DC hi
  for (int i = 0; i < length; i++) {
    writeValue (colour >> 8);
    writeValue (colour & 0xFF);
    }
  }
//}}}
//{{{
void writePixelBytes (uint16_t* bytes, uint32_t length) {

  GPIOD->BSRRL = DC_PIN; // DC hi
  for (int i = 0; i < length; i++) {
    uint16_t pixel = *bytes++;
    writeValue (pixel >> 8);
    writeValue (pixel & 0xFF);
    }
  }
//}}}

//{{{
void displayHwInit (uint16_t useOptions) {

  options = useOptions;

  // enable port clocks
  RCC_AHB1PeriphClockCmd (RCC_AHB1Periph_GPIOD | RCC_AHB1Periph_GPIOE, ENABLE);

  // GPIOD
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.GPIO_Pin = CS_PIN | WR_PIN | RD_PIN | DC_PIN |
                             GPIO_Pin_0 | GPIO_Pin_1 |
                             GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
  if (options & USE_16BIT_DATA)
    GPIO_InitStruct.GPIO_Pin |= GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_25MHz;
  GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init (GPIOD, &GPIO_InitStruct);

  GPIOD->BSRRL = WR_PIN | RD_PIN | DC_PIN; // WR,RD,DC hi
  GPIOD->BSRRH = CS_PIN; // CS lo

  // GPIOE
  GPIO_InitStruct.GPIO_Pin = RES_PIN | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
  if (options & USE_16BIT_DATA)
    GPIO_InitStruct.GPIO_Pin |= GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_14 | GPIO_Pin_15;
  GPIO_Init (GPIOE, &GPIO_InitStruct);

  GPIOE->BSRRH = RES_PIN; // RES lo
  delayMs (1);
  GPIOE->BSRRL = RES_PIN; // RES hi
  delayMs (1);
  }
//}}}
