// pigpio.c - lightweight pigpio
//{{{  bits
 // 0 GPFSEL0   GPIO Function Select 0
 // 1 GPFSEL1   GPIO Function Select 1
 // 2 GPFSEL2   GPIO Function Select 2
 // 3 GPFSEL3   GPIO Function Select 3
 // 4 GPFSEL4   GPIO Function Select 4
 // 5 GPFSEL5   GPIO Function Select 5
 // 6 -         Reserved
 // 7 GPSET0    GPIO Pin Output Set 0
 // 8 GPSET1    GPIO Pin Output Set 1
 // 9 -         Reserved
// 10 GPCLR0    GPIO Pin Output Clear 0
// 11 GPCLR1    GPIO Pin Output Clear 1
// 12 -         Reserved
// 13 GPLEV0    GPIO Pin Level 0
// 14 GPLEV1    GPIO Pin Level 1
// 15 -         Reserved
// 16 GPEDS0    GPIO Pin Event Detect Status 0
// 17 GPEDS1    GPIO Pin Event Detect Status 1
// 18 -         Reserved
// 19 GPREN0    GPIO Pin Rising Edge Detect Enable 0
// 20 GPREN1    GPIO Pin Rising Edge Detect Enable 1
// 21 -         Reserved
// 22 GPFEN0    GPIO Pin Falling Edge Detect Enable 0
// 23 GPFEN1    GPIO Pin Falling Edge Detect Enable 1
// 24 -         Reserved
// 25 GPHEN0    GPIO Pin High Detect Enable 0
// 26 GPHEN1    GPIO Pin High Detect Enable 1
// 27 -         Reserved
// 28 GPLEN0    GPIO Pin Low Detect Enable 0
// 29 GPLEN1    GPIO Pin Low Detect Enable 1
// 30 -         Reserved
// 31 GPAREN0   GPIO Pin Async. Rising Edge Detect 0
// 32 GPAREN1   GPIO Pin Async. Rising Edge Detect 1
// 33 -         Reserved
// 34 GPAFEN0   GPIO Pin Async. Falling Edge Detect 0
// 35 GPAFEN1   GPIO Pin Async. Falling Edge Detect 1
// 36 -         Reserved
// 37 GPPUD     GPIO Pin Pull-up/down Enable
// 38 GPPUDCLK0 GPIO Pin Pull-up/down Enable Clock 0
// 39 GPPUDCLK1 GPIO Pin Pull-up/down Enable Clock 1
// 40 -         Reserved
// 41 -         Test
// 42-56        Reserved
// 57 GPPUPPDN1 Pin pull-up/down for pins 15:0
// 58 GPPUPPDN1 Pin pull-up/down for pins 31:16
// 59 GPPUPPDN2 Pin pull-up/down for pins 47:32
// 60 GPPUPPDN3 Pin pull-up/down for pins 57:48
//}}}
//{{{  dma
// 0 CS           DMA Channel 0 Control and Status
// 1 CPI_ONBLK_AD DMA Channel 0 Control Block Address
// 2 TI           DMA Channel 0 CB Word 0 (Transfer Information)
// 3 SOURCE_AD    DMA Channel 0 CB Word 1 (Source Address)
// 4 DEST_AD      DMA Channel 0 CB Word 2 (Destination Address)
// 5 TXFR_LEN     DMA Channel 0 CB Word 3 (Transfer Length)
// 6 STRIDE       DMA Channel 0 CB Word 4 (2D Stride)
// 7 NEXTCPI_ONBK DMA Channel 0 CB Word 5 (Next CB Address)
// 8 DEBUG        DMA Channel 0 Debug
//}}}
//{{{  DEBUG register bits
// bit 2 READ_ERROR
  // Slave Read Response Error RW 0x0
  // Set if the read operation returned an error value on
  // the read response bus. It can be cleared by writing a 1.

// bit 1 FIFO_ERROR
  // Fifo Error RW 0x0
  // Set if the optional read Fifo records an error
  // condition. It can be cleared by writing a 1.

// bit 0 READ_LAST_NOT_SET_ERROR
  // Read Last Not Set Error RW 0x0
  // If the AXI read last signal was not set when
  // expected, then this error bit will be set. It can be
  // cleared by writing a 1.

// 0 CTL        PWM Control
// 1 STA        PWM Status
// 2 DMAC       PWM DMA Configuration
// 4 RNG1       PWM Channel 1 Range
// 5 DAT1       PWM Channel 1 Data
// 6 FIF1       PWM FIFO Input
// 8 RNG2       PWM Channel 2 Range
// 9 DAT2       PWM Channel 2 Data

// 0 PCM_CS     PCM Control and Status
// 1 PCM_FIFO   PCM FIFO Data
// 2 PCM_MODE   PCM Mode
// 3 PCM_RXC    PCM Receive Configuration
// 4 PCM_TXC    PCM Transmit Configuration
// 5 PCM_DREQ   PCM DMA Request Level
// 6 PCM_INTEN  PCM Interrupt Enables
// 7 PCM_INTSTC PCM Interrupt Status & Clear
// 8 PCM_GRAY   PCM Gray Mode Control

// 0 CS  System Timer Control/Status
// 1 CLO System Timer Counter Lower 32 bits
// 2 CHI System Timer Counter Higher 32 bits
// 3 C0  System Timer Compare 0
// 4 C1  System Timer Compare 1
// 5 C2  System Timer Compare 2
// 6 C3  System Timer Compare 3
//}}}
//{{{  includes
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <pthread.h>

#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/sysmacros.h>

#include <arpa/inet.h>

#include "pigpio.h"
//}}}
//{{{  defines
#define THOUSAND 1000
#define MILLION  1000000
#define BILLION  1000000000

#define BANK (gpio>>5)
#define BIT  (1<<(gpio&0x1F))

//{{{  dbg_level
#define DBG_MIN_LEVEL 0
#define DBG_ALWAYS    0
#define DBG_STARTUP   1
#define DBG_DMACBS    2
#define DBG_SCRIPT    3
#define DBG_USER      4
#define DBG_INTERNAL  5
#define DBG_SLOW_TICK 6
#define DBG_FAST_TICK 7
#define DBG_MAX_LEVEL 8
//}}}
//{{{
#define DO_DBG(level, format, arg...) { \
  if ((gpioCfg.dbgLevel >= level) && (!(gpioCfg.internals & PI_CFG_NOSIGHANDLER)))  \
    fprintf(stderr, "%s %s: " format "\n" , myTimeStamp(), __FUNCTION__ , ## arg);  \
  }
//}}}
#define DBG(level, format, arg...) DO_DBG(level, format, ## arg)
//{{{
#define SOFT_ERROR(x, format, arg...)  \
  do { \
    DBG(DBG_ALWAYS, format, ## arg); \
    return x; \
    } while (0);
//}}}

//{{{
#define TIMER_ADD(a, b, result) \
  do {  \
    (result)->tv_sec =  (a)->tv_sec  + (b)->tv_sec;  \
    (result)->tv_nsec = (a)->tv_nsec + (b)->tv_nsec; \
    if ((result)->tv_nsec >= BILLION) {              \
      ++(result)->tv_sec;                            \
      (result)->tv_nsec -= BILLION;                  \
      }                                              \
    } while (0)
//}}}
//{{{
#define TIMER_SUB(a, b, result)        \
   do                                                              \
   {                                                               \
      (result)->tv_sec =  (a)->tv_sec  - (b)->tv_sec;              \
      (result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec;             \
      if ((result)->tv_nsec < 0)                                   \
      {                                                            \
         --(result)->tv_sec;                                       \
         (result)->tv_nsec += BILLION;                             \
      }                                                            \
   }                                                               \
   while (0)
//}}}

#define PI_PERI_BUS 0x7E000000
//{{{  base addresses
#define AUX_BASE   (pi_peri_phys + 0x00215000)
#define BSCS_BASE  (pi_peri_phys + 0x00214000)
#define CLK_BASE   (pi_peri_phys + 0x00101000)
#define DMA_BASE   (pi_peri_phys + 0x00007000)
#define DMA15_BASE (pi_peri_phys + 0x00E05000)
#define GPIO_BASE  (pi_peri_phys + 0x00200000)
#define PADS_BASE  (pi_peri_phys + 0x00100000)
#define PCM_BASE   (pi_peri_phys + 0x00203000)
#define PWM_BASE   (pi_peri_phys + 0x0020C000)
#define SPI_BASE   (pi_peri_phys + 0x00204000)
#define SYST_BASE  (pi_peri_phys + 0x00003000)
//}}}
//{{{  lens
#define AUX_LEN   0xD8
#define BSCS_LEN  0x40
#define CLK_LEN   0xA8
#define DMA_LEN   0x1000 /* allow access to all channels */
#define GPIO_LEN  0xF4   /* 2711 has more registers */
#define PADS_LEN  0x38
#define PCM_LEN   0x24
#define PWM_LEN   0x28
#define SPI_LEN   0x18
#define SYST_LEN  0x1C
//}}}
//{{{  gpio flags
#define GPFSEL0    0

#define GPSET0     7
#define GPSET1     8

#define GPCLR0    10
#define GPCLR1    11

#define GPLEV0    13
#define GPLEV1    14

#define GPEDS0    16
#define GPEDS1    17

#define GPREN0    19
#define GPREN1    20
#define GPFEN0    22
#define GPFEN1    23
#define GPHEN0    25
#define GPHEN1    26
#define GPLEN0    28
#define GPLEN1    29
#define GPAREN0   31
#define GPAREN1   32
#define GPAFEN0   34
#define GPAFEN1   35

#define GPPUD     37
#define GPPUDCLK0 38
#define GPPUDCLK1 39
//}}}
//{{{  BCM2711 has different pulls
#define GPPUPPDN0 57
#define GPPUPPDN1 58
#define GPPUPPDN2 59
#define GPPUPPDN3 60
//}}}
//{{{  dma
#define DMA_ENABLE (0xFF0/4)
#define DMA_CS        0
#define DMA_CONBLK_AD 1
#define DMA_DEBUG     8

// DMA CS Control and Status bits
#define DMA_CHANNEL_RESET       (1<<31)
#define DMA_CHANNEL_ABORT       (1<<30)
#define DMA_WAIT_ON_WRITES      (1<<28)
#define DMA_PANIC_PRIORITY(x) ((x)<<20)
#define DMA_PRIORITY(x)       ((x)<<16)
#define DMA_INTERRUPT_STATUS    (1<< 2)
#define DMA_END_FLAG            (1<< 1)
#define DMA_ACTIVE              (1<< 0)

// DMA control block "info" field bits
#define DMA_NO_WIDE_BURSTS          (1<<26)
#define DMA_PERIPHERAL_MAPPING(x) ((x)<<16)
#define DMA_BURST_LENGTH(x)       ((x)<<12)
#define DMA_SRC_IGNORE              (1<<11)
#define DMA_SRC_DREQ                (1<<10)
#define DMA_SRC_WIDTH               (1<< 9)
#define DMA_SRC_INC                 (1<< 8)
#define DMA_DEST_IGNORE             (1<< 7)
#define DMA_DEST_DREQ               (1<< 6)
#define DMA_DEST_WIDTH              (1<< 5)
#define DMA_DEST_INC                (1<< 4)
#define DMA_WAIT_RESP               (1<< 3)
#define DMA_TDMODE                  (1<< 1)

#define DMA_DEBUG_READ_ERR           (1<<2)
#define DMA_DEBUG_FIFO_ERR           (1<<1)
#define DMA_DEBUG_RD_LST_NOT_SET_ERR (1<<0)

#define DMA_LITE_FIRST 7
#define DMA_LITE_MAX 0xfffc
//}}}
//{{{  pwm
#define PWM_CTL      0
#define PWM_STA      1
#define PWM_DMAC     2
#define PWM_RNG1     4
#define PWM_DAT1     5
#define PWM_FIFO     6
#define PWM_RNG2     8
#define PWM_DAT2     9

#define PWM_CTL_MSEN2 (1<<15)
#define PWM_CTL_PWEN2 (1<<8)
#define PWM_CTL_MSEN1 (1<<7)
#define PWM_CTL_CLRF1 (1<<6)
#define PWM_CTL_USEF1 (1<<5)
#define PWM_CTL_MODE1 (1<<1)
#define PWM_CTL_PWEN1 (1<<0)

#define PWM_DMAC_ENAB      (1 <<31)
#define PWM_DMAC_PANIC(x) ((x)<< 8)
#define PWM_DMAC_DREQ(x)   (x)
//}}}
//{{{  pcm
#define PCM_CS     0
#define PCM_FIFO   1
#define PCM_MODE   2
#define PCM_RXC    3
#define PCM_TXC    4
#define PCM_DREQ   5
#define PCM_INTEN  6
#define PCM_INTSTC 7
#define PCM_GRAY   8

#define PCM_CS_STBY     (1 <<25)
#define PCM_CS_SYNC     (1 <<24)
#define PCM_CS_RXSEX    (1 <<23)
#define PCM_CS_RXERR    (1 <<16)
#define PCM_CS_TXERR    (1 <<15)
#define PCM_CS_DMAEN    (1  <<9)
#define PCM_CS_RXTHR(x) ((x)<<7)
#define PCM_CS_TXTHR(x) ((x)<<5)
#define PCM_CS_RXCLR    (1  <<4)
#define PCM_CS_TXCLR    (1  <<3)
#define PCM_CS_TXON     (1  <<2)
#define PCM_CS_RXON     (1  <<1)
#define PCM_CS_EN       (1  <<0)

#define PCM_MODE_CLK_DIS  (1  <<28)
#define PCM_MODE_PDMN     (1  <<27)
#define PCM_MODE_PDME     (1  <<26)
#define PCM_MODE_FRXP     (1  <<25)
#define PCM_MODE_FTXP     (1  <<24)
#define PCM_MODE_CLKM     (1  <<23)
#define PCM_MODE_CLKI     (1  <<22)
#define PCM_MODE_FSM      (1  <<21)
#define PCM_MODE_FSI      (1  <<20)
#define PCM_MODE_FLEN(x)  ((x)<<10)
#define PCM_MODE_FSLEN(x) ((x)<< 0)

#define PCM_RXC_CH1WEX    (1  <<31)
#define PCM_RXC_CH1EN     (1  <<30)
#define PCM_RXC_CH1POS(x) ((x)<<20)
#define PCM_RXC_CH1WID(x) ((x)<<16)
#define PCM_RXC_CH2WEX    (1  <<15)
#define PCM_RXC_CH2EN     (1  <<14)
#define PCM_RXC_CH2POS(x) ((x)<< 4)
#define PCM_RXC_CH2WID(x) ((x)<< 0)

#define PCM_TXC_CH1WEX    (1  <<31)
#define PCM_TXC_CH1EN     (1  <<30)
#define PCM_TXC_CH1POS(x) ((x)<<20)
#define PCM_TXC_CH1WID(x) ((x)<<16)
#define PCM_TXC_CH2WEX    (1  <<15)
#define PCM_TXC_CH2EN     (1  <<14)
#define PCM_TXC_CH2POS(x) ((x)<< 4)
#define PCM_TXC_CH2WID(x) ((x)<< 0)

#define PCM_DREQ_TX_PANIC(x) ((x)<<24)
#define PCM_DREQ_RX_PANIC(x) ((x)<<16)
#define PCM_DREQ_TX_REQ_L(x) ((x)<< 8)
#define PCM_DREQ_RX_REQ_L(x) ((x)<< 0)

#define PCM_INTEN_RXERR (1<<3)
#define PCM_INTEN_TXERR (1<<2)
#define PCM_INTEN_RXR   (1<<1)
#define PCM_INTEN_TXW   (1<<0)

#define PCM_INTSTC_RXERR (1<<3)
#define PCM_INTSTC_TXERR (1<<2)
#define PCM_INTSTC_RXR   (1<<1)
#define PCM_INTSTC_TXW   (1<<0)

#define PCM_GRAY_FLUSH (1<<2)
#define PCM_GRAY_CLR   (1<<1)
#define PCM_GRAY_EN    (1<<0)
//}}}
#define BCM_PASSWD  (0x5A<<24)
//{{{  clk
#define CLK_CTL_MASH(x)((x)<<9)
#define CLK_CTL_BUSY    (1 <<7)
#define CLK_CTL_KILL    (1 <<5)
#define CLK_CTL_ENAB    (1 <<4)
#define CLK_CTL_SRC(x) ((x)<<0)

#define CLK_SRCS 2

#define CLK_CTL_SRC_OSC  1
#define CLK_CTL_SRC_PLLD 6

#define CLK_OSC_FREQ        19200000
#define CLK_OSC_FREQ_2711   54000000
#define CLK_PLLD_FREQ      500000000
#define CLK_PLLD_FREQ_2711 750000000

#define CLK_DIV_DIVI(x) ((x)<<12)
#define CLK_DIV_DIVF(x) ((x)<< 0)

#define CLK_GP0_CTL 28
#define CLK_GP0_DIV 29
#define CLK_GP1_CTL 30
#define CLK_GP1_DIV 31
#define CLK_GP2_CTL 32
#define CLK_GP2_DIV 33

#define CLK_PCMCTL 38
#define CLK_PCMDIV 39

#define CLK_PWMCTL 40
#define CLK_PWMDIV 41
//}}}
//{{{  syst
#define SYST_CS      0
#define SYST_CLO     1
#define SYST_CHI     2
//}}}
//{{{  SPI
#define SPI_CS   0
#define SPI_FIFO 1
#define SPI_CLK  2
#define SPI_DLEN 3
#define SPI_LTOH 4
#define SPI_DC   5

#define SPI_CS_LEN_LONG    (1<<25)
#define SPI_CS_DMA_LEN     (1<<24)
#define SPI_CS_CSPOLS(x) ((x)<<21)
#define SPI_CS_RXF         (1<<20)
#define SPI_CS_RXR         (1<<19)
#define SPI_CS_TXD         (1<<18)
#define SPI_CS_RXD         (1<<17)
#define SPI_CS_DONE        (1<<16)
#define SPI_CS_LEN         (1<<13)
#define SPI_CS_REN         (1<<12)
#define SPI_CS_ADCS        (1<<11)
#define SPI_CS_INTR        (1<<10)
#define SPI_CS_INTD        (1<<9)
#define SPI_CS_DMAEN       (1<<8)
#define SPI_CS_TA          (1<<7)
#define SPI_CS_CSPOL(x)  ((x)<<6)
#define SPI_CS_CLEAR(x)  ((x)<<4)
#define SPI_CS_MODE(x)   ((x)<<2)
#define SPI_CS_CS(x)     ((x)<<0)

#define SPI_DC_RPANIC(x) ((x)<<24)
#define SPI_DC_RDREQ(x)  ((x)<<16)
#define SPI_DC_TPANIC(x) ((x)<<8)
#define SPI_DC_TDREQ(x)  ((x)<<0)

#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3

#define SPI_CS0     0
#define SPI_CS1     1
#define SPI_CS2     2

/* standard SPI gpios (ALT0) */
#define PI_SPI_CE0   8
#define PI_SPI_CE1   7
#define PI_SPI_SCLK 11
#define PI_SPI_MISO  9
#define PI_SPI_MOSI 10

/* auxiliary SPI gpios (ALT4) */
#define PI_ASPI_CE0  18
#define PI_ASPI_CE1  17
#define PI_ASPI_CE2  16
#define PI_ASPI_MISO 19
#define PI_ASPI_MOSI 20
#define PI_ASPI_SCLK 21
//}}}
//{{{  AUX
#define AUX_IRQ     0
#define AUX_ENABLES 1

#define AUX_MU_IO_REG   16
#define AUX_MU_IER_REG  17
#define AUX_MU_IIR_REG  18
#define AUX_MU_LCR_REG  19
#define AUX_MU_MCR_REG  20
#define AUX_MU_LSR_REG  21
#define AUX_MU_MSR_REG  22
#define AUX_MU_SCRATCH  23
#define AUX_MU_CNTL_REG 24
#define AUX_MU_STAT_REG 25
#define AUX_MU_BAUD_REG 26

#define AUX_SPI0_CNTL0_REG 32
#define AUX_SPI0_CNTL1_REG 33
#define AUX_SPI0_STAT_REG  34
#define AUX_SPI0_PEEK_REG  35

#define AUX_SPI0_IO_REG    40
#define AUX_SPI0_TX_HOLD   44

#define AUX_SPI1_CNTL0_REG 48
#define AUX_SPI1_CNTL1_REG 49
#define AUX_SPI1_STAT_REG  50
#define AUX_SPI1_PEEK_REG  51

#define AUX_SPI1_IO_REG    56
#define AUX_SPI1_TX_HOLD   60

#define AUXENB_SPI2 (1<<2)
#define AUXENB_SPI1 (1<<1)
#define AUXENB_UART (1<<0)

#define AUXSPI_CNTL0_SPEED(x)      ((x)<<20)
#define AUXSPI_CNTL0_CS(x)         ((x)<<17)
#define AUXSPI_CNTL0_POSTINP         (1<<16)
#define AUXSPI_CNTL0_VAR_CS          (1<<15)
#define AUXSPI_CNTL0_VAR_WIDTH       (1<<14)
#define AUXSPI_CNTL0_DOUT_HOLD(x)  ((x)<<12)
#define AUXSPI_CNTL0_ENABLE          (1<<11)
#define AUXSPI_CNTL0_IN_RISING(x)  ((x)<<10)
#define AUXSPI_CNTL0_CLR_FIFOS       (1<<9)
#define AUXSPI_CNTL0_OUT_RISING(x) ((x)<<8)
#define AUXSPI_CNTL0_INVERT_CLK(x) ((x)<<7)
#define AUXSPI_CNTL0_MSB_FIRST(x)  ((x)<<6)
#define AUXSPI_CNTL0_SHIFT_LEN(x)  ((x)<<0)

#define AUXSPI_CNTL1_CS_HIGH(x)  ((x)<<8)
#define AUXSPI_CNTL1_TX_IRQ        (1<<7)
#define AUXSPI_CNTL1_DONE_IRQ      (1<<6)
#define AUXSPI_CNTL1_MSB_FIRST(x)((x)<<1)
#define AUXSPI_CNTL1_KEEP_INPUT    (1<<0)

#define AUXSPI_STAT_TX_FIFO(x) ((x)<<28)
#define AUXSPI_STAT_RX_FIFO(x) ((x)<<20)
#define AUXSPI_STAT_TX_FULL      (1<<10)
#define AUXSPI_STAT_TX_EMPTY     (1<<9)
#define AUXSPI_STAT_RX_EMPTY     (1<<7)
#define AUXSPI_STAT_BUSY         (1<<6)
#define AUXSPI_STAT_BITS(x)    ((x)<<0)
//}}}

#define NORMAL_DMA (DMA_NO_WIDE_BURSTS | DMA_WAIT_RESP)
#define TWO_BEAT_DMA (DMA_TDMODE | DMA_BURST_LENGTH(1))
#define TIMED_DMA(x) (DMA_DEST_DREQ | DMA_PERIPHERAL_MAPPING(x))
#define PCM_TIMER (((PCM_BASE + PCM_FIFO*4) & 0x00ffffff) | PI_PERI_BUS)
#define PWM_TIMER (((PWM_BASE + PWM_FIFO*4) & 0x00ffffff) | PI_PERI_BUS)

//{{{  gpio type
#define GPIO_UNDEFINED 0
#define GPIO_WRITE     1
#define GPIO_PWM       2
#define GPIO_SERVO     3
#define GPIO_HW_CLK    4
#define GPIO_HW_PWM    5
#define GPIO_SPI       6
#define GPIO_I2C       7
//}}}

#define STACK_SIZE (256*1024)
#define PAGE_SIZE 4096
#define PWM_FREQS 18
#define CYCLES_PER_BLOCK 80
//{{{  pages
#define PAGES_PER_BLOCK 53

#define CBS_PER_IPAGE 117
#define LVS_PER_IPAGE  38
#define OFF_PER_IPAGE  38
#define TCK_PER_IPAGE   2
#define ON_PER_IPAGE    2
#define PAD_PER_IPAGE   7

#define CBS_PER_OPAGE 118
#define OOL_PER_OPAGE  79
//}}}

#define SUPERCYCLE 800
#define SUPERLEVEL 20000

#define BLOCK_SIZE (PAGES_PER_BLOCK*PAGE_SIZE)

#define DMAI_PAGES (PAGES_PER_BLOCK * bufferBlocks)
#define DMAO_PAGES (PAGES_PER_BLOCK * PI_WAVE_BLOCKS)

#define TICKSLOTS 50
#define SRX_BUF_SIZE 8192

//{{{  open closed
#define PI_I2C_CLOSED   0
#define PI_I2C_RESERVED 1
#define PI_I2C_OPENED   2

#define PI_SPI_CLOSED   0
#define PI_SPI_RESERVED 1
#define PI_SPI_OPENED   2

#define PI_SER_CLOSED   0
#define PI_SER_RESERVED 1
#define PI_SER_OPENED   2

#define PI_FILE_CLOSED   0
#define PI_FILE_RESERVED 1
#define PI_FILE_OPENED   2

#define PI_NOTIFY_CLOSED   0
#define PI_NOTIFY_RESERVED 1
#define PI_NOTIFY_CLOSING  2
#define PI_NOTIFY_OPENED   3
#define PI_NOTIFY_RUNNING  4
#define PI_NOTIFY_PAUSED   5
//}}}
//{{{  wfrx
#define PI_WFRX_NONE     0
#define PI_WFRX_SERIAL   1
#define PI_WFRX_I2C_SDA  2
#define PI_WFRX_I2C_SCL  3
#define PI_WFRX_SPI_SCLK 4
#define PI_WFRX_SPI_MISO 5
#define PI_WFRX_SPI_MOSI 6
#define PI_WFRX_SPI_CS   7
//}}}

#define PI_WF_MICROS   1
#define DEFAULT_PWM_IDX 5

#define PULSE_PER_CYCLE  25
#define CBS_PER_CYCLE ((PULSE_PER_CYCLE*3)+2)
#define NUM_CBS (CBS_PER_CYCLE * bufferCycles)

//{{{  i2c
#define PI_I2C_RETRIES 0x0701
#define PI_I2C_TIMEOUT 0x0702
#define PI_I2C_SLAVE   0x0703
#define PI_I2C_FUNCS   0x0705
#define PI_I2C_RDWR    0x0707
#define PI_I2C_SMBUS   0x0720

#define PI_I2C_SMBUS_READ  1
#define PI_I2C_SMBUS_WRITE 0

#define PI_I2C_SMBUS_QUICK            0
#define PI_I2C_SMBUS_BYTE             1
#define PI_I2C_SMBUS_BYTE_DATA        2
#define PI_I2C_SMBUS_WORD_DATA        3
#define PI_I2C_SMBUS_PROC_CALL        4
#define PI_I2C_SMBUS_BLOCK_DATA       5
#define PI_I2C_SMBUS_I2C_BLOCK_BROKEN 6
#define PI_I2C_SMBUS_BLOCK_PROC_CALL  7
#define PI_I2C_SMBUS_I2C_BLOCK_DATA   8

#define PI_I2C_SMBUS_BLOCK_MAX     32
#define PI_I2C_SMBUS_I2C_BLOCK_MAX 32

#define PI_I2C_FUNC_SMBUS_QUICK            0x00010000
#define PI_I2C_FUNC_SMBUS_READ_BYTE        0x00020000
#define PI_I2C_FUNC_SMBUS_WRITE_BYTE       0x00040000
#define PI_I2C_FUNC_SMBUS_READ_BYTE_DATA   0x00080000
#define PI_I2C_FUNC_SMBUS_WRITE_BYTE_DATA  0x00100000
#define PI_I2C_FUNC_SMBUS_READ_WORD_DATA   0x00200000
#define PI_I2C_FUNC_SMBUS_WRITE_WORD_DATA  0x00400000
#define PI_I2C_FUNC_SMBUS_PROC_CALL        0x00800000
#define PI_I2C_FUNC_SMBUS_READ_BLOCK_DATA  0x01000000
#define PI_I2C_FUNC_SMBUS_WRITE_BLOCK_DATA 0x02000000
#define PI_I2C_FUNC_SMBUS_READ_I2C_BLOCK   0x04000000
#define PI_I2C_FUNC_SMBUS_WRITE_I2C_BLOCK  0x08000000
//}}}

#define PI_MASH_MAX_FREQ 23800000
#define FLUSH_PAGES 1024
//{{{  mailbox
#define MB_DEV_MAJOR 100

#define MB_IOCTL _IOWR(MB_DEV_MAJOR, 0, char *)

#define MB_DEV1 "/dev/vcio"
#define MB_DEV2 "/dev/pigpio-mb"

#define BUS_TO_PHYS(x) ((x)&~0xC0000000)

#define MB_END_TAG 0
#define MB_PROCESS_REQUEST 0

#define MB_ALLOCATE_MEMORY_TAG 0x3000C
#define MB_LOCK_MEMORY_TAG     0x3000D
#define MB_UNLOCK_MEMORY_TAG   0x3000E
#define MB_RELEASE_MEMORY_TAG  0x3000F
//}}}
//{{{  spi flags
#define PI_SPI_FLAGS_CHANNEL(x)    ((x&7)<<29)

#define PI_SPI_FLAGS_GET_CHANNEL(x) (((x)>>29)&7)
#define PI_SPI_FLAGS_GET_BITLEN(x)  (((x)>>16)&63)
#define PI_SPI_FLAGS_GET_RX_LSB(x)  (((x)>>15)&1)
#define PI_SPI_FLAGS_GET_TX_LSB(x)  (((x)>>14)&1)
#define PI_SPI_FLAGS_GET_3WREN(x)   (((x)>>10)&15)
#define PI_SPI_FLAGS_GET_3WIRE(x)   (((x)>>9)&1)
#define PI_SPI_FLAGS_GET_AUX_SPI(x) (((x)>>8)&1)
#define PI_SPI_FLAGS_GET_RESVD(x)   (((x)>>5)&7)
#define PI_SPI_FLAGS_GET_CSPOLS(x)  (((x)>>2)&7)
#define PI_SPI_FLAGS_GET_MODE(x)     ((x)&3)

#define PI_SPI_FLAGS_GET_CPHA(x)  ((x)&1)
#define PI_SPI_FLAGS_GET_CPOL(x)  ((x)&2)
#define PI_SPI_FLAGS_GET_CSPOL(x) ((x)&4)
//}}}
//}}}
//{{{  typedef
typedef void (*callbk_t) ();

//{{{
typedef struct
{
   rawCbs_t cb           [128];
} dmaPage_t;
//}}}
//{{{
typedef struct
{
   rawCbs_t cb           [CBS_PER_IPAGE];
   uint32_t level        [LVS_PER_IPAGE];
   uint32_t gpioOff      [OFF_PER_IPAGE];
   uint32_t tick         [TCK_PER_IPAGE];
   uint32_t gpioOn       [ON_PER_IPAGE];
   uint32_t periphData;
   uint32_t pad          [PAD_PER_IPAGE];
} dmaIPage_t;
//}}}
//{{{
typedef struct
{
   rawCbs_t cb     [CBS_PER_OPAGE];
   uint32_t OOL    [OOL_PER_OPAGE];
   uint32_t periphData;
} dmaOPage_t;

//}}}
//{{{
typedef struct
{
   uint8_t  is;
   uint8_t  pad;
   uint16_t width;
   uint16_t range; /* dutycycles specified by 0 .. range */
   uint16_t freqIdx;
   uint16_t deferOff;
   uint16_t deferRng;
} gpioInfo_t;
//}}}
//{{{
typedef struct
{
   unsigned gpio;
   pthread_t *pth;
   callbk_t func;
   unsigned edge;
   int timeout;
   unsigned ex;
   void *userdata;
   int fd;
   int inited;
} gpioISR_t;
//}}}
//{{{
typedef struct
{
   callbk_t func;
   unsigned ex;
   void *userdata;
} gpioSignal_t;
//}}}
//{{{
typedef struct
{
   callbk_t func;
   unsigned ex;
   void *userdata;
   uint32_t bits;
} gpioGetSamples_t;
//}}}
//{{{
typedef struct
{
   callbk_t func;
   unsigned ex;
   void *userdata;
   unsigned id;
   unsigned running;
   unsigned millis;
   pthread_t pthId;
} gpioTimer_t;
//}}}
//{{{
typedef struct
{
   uint16_t valid;
   uint16_t servoIdx;
} clkCfg_t;
//}}}
//{{{
typedef struct
{
   uint16_t seqno;
   uint16_t state;
   uint32_t bits;
   uint32_t eventBits;
   uint32_t lastReportTick;
   int      fd;
   int      pipe;
   int      max_emits;
} gpioNotify_t;
//}}}
//{{{
typedef struct
{
   uint16_t state;
   int16_t  fd;
   uint32_t mode;
} fileInfo_t;
//}}}
//{{{
typedef struct
{
   uint16_t state;
   int16_t  fd;
   uint32_t addr;
   uint32_t flags;
   uint32_t funcs;
} i2cInfo_t;
//}}}
//{{{
typedef struct
{
   uint16_t state;
   int16_t  fd;
   uint32_t flags;
} serInfo_t;
//}}}
//{{{
typedef struct
{
   uint16_t state;
   unsigned speed;
   uint32_t flags;
} spiInfo_t;
//}}}
//{{{
typedef struct
{
   unsigned bufferMilliseconds;
   unsigned clockMicros;
   unsigned clockPeriph;
   unsigned DMAprimaryChannel;
   unsigned DMAsecondaryChannel;
   unsigned ifFlags;
   unsigned memAllocMode;
   unsigned dbgLevel;
   unsigned alertFreq;
   uint32_t internals;
      /*
      0-3: dbgLevel
      4-7: alertFreq
      */
} gpioCfg_t;
//}}}
//{{{
typedef struct
{
   uint32_t micros;
   uint32_t highMicros;
   uint32_t maxMicros;
   uint32_t pulses;
   uint32_t highPulses;
   uint32_t maxPulses;
   uint32_t cbs;
   uint32_t highCbs;
   uint32_t maxCbs;
} wfStats_t;
//}}}
//{{{
typedef struct
{
   char    *buf;
   uint32_t bufSize;
   int      readPos;
   int      writePos;
   uint32_t fullBit; /* nanoseconds */
   uint32_t halfBit; /* nanoseconds */
   int      timeout; /* millisconds */
   uint32_t startBitTick; /* microseconds */
   uint32_t nextBitDiff; /* nanoseconds */
   int      bit;
   uint32_t data;
   int      bytes; /* 1, 2, 4 */
   int      level;
   int      dataBits; /* 1-32 */
   int      invert; /* 0, 1 */
} wfRxSerial_t;
//}}}
//{{{
typedef struct
{
   int SDA;
   int SCL;
   int delay;
   int SDAMode;
   int SCLMode;
   int started;
} wfRxI2C_t;
//}}}
//{{{
typedef struct
{
   int CS;
   int MISO;
   int MOSI;
   int SCLK;
   int usage;
   int delay;
   int spiFlags;
   int MISOMode;
   int MOSIMode;
   int CSMode;
   int SCLKMode;
} wfRxSPI_t;
//}}}
//{{{
typedef struct
{
   int      mode;
   int      gpio;
   uint32_t baud;
   pthread_mutex_t mutex;
   union
   {
      wfRxSerial_t s;
      wfRxI2C_t    I;
      wfRxSPI_t    S;
   };
} wfRx_t;
//}}}
//{{{
union my_smbus_data
{
   uint8_t  byte;
   uint16_t word;
   uint8_t  block[PI_I2C_SMBUS_BLOCK_MAX + 2];
};
//}}}
//{{{
struct my_smbus_ioctl_data
{
   uint8_t read_write;
   uint8_t command;
   uint32_t size;
   union my_smbus_data *data;
};
//}}}
//{{{
typedef struct
{
   pi_i2c_msg_t *msgs; /* pointers to pi_i2c_msgs */
   uint32_t     nmsgs; /* number of pi_i2c_msgs */
} my_i2c_rdwr_ioctl_data_t;
//}}}
//{{{
typedef struct
{
   unsigned div;
   unsigned frac;
   unsigned clock;
} clkInf_t;
//}}}
//{{{
typedef struct
{
   unsigned  handle;        /* mbAllocateMemory() */
   uintptr_t bus_addr;      /* mbLockMemory() */
   uintptr_t *virtual_addr; /* mbMapMem() */
   unsigned  size;          /* in bytes */
} DMAMem_t;
//}}}
//}}}
//{{{  static var
//{{{  static var - initialise once then preserve
static volatile uint32_t piCores       = 0;
static volatile uint32_t pi_peri_phys  = 0x20000000;
static volatile uint32_t pi_dram_bus   = 0x40000000;
static volatile uint32_t pi_mem_flag   = 0x0C;
static volatile uint32_t pi_ispi       = 0;
static volatile uint32_t pi_is_2711    = 0;

static volatile uint32_t clk_osc_freq  = CLK_OSC_FREQ;
static volatile uint32_t clk_plld_freq = CLK_PLLD_FREQ;

static volatile uint32_t hw_pwm_max_freq = PI_HW_PWM_MAX_FREQ;
static volatile uint32_t hw_clk_min_freq = PI_HW_CLK_MIN_FREQ;
static volatile uint32_t hw_clk_max_freq = PI_HW_CLK_MAX_FREQ;
//}}}
//{{{  static var - initialise every gpioInitialise
static struct timespec libStarted;

static int PWMClockInited = 0;

static int gpioMaskSet = 0;
//}}}
//{{{  static var - initialise if not libInitialised
static uint64_t gpioMask;

static int wfc[3] = {0, 0, 0};
static int wfcur = 0;
static wfStats_t wfStats = {
  0, 0, PI_WAVE_MAX_MICROS,
  0, 0, PI_WAVE_MAX_PULSES,
  0, 0, (DMAO_PAGES * CBS_PER_OPAGE)
  };
static wfRx_t wfRx[PI_MAX_USER_GPIO+1];

static gpioGetSamples_t gpioGetSamples;

static gpioInfo_t gpioInfo   [PI_MAX_GPIO+1];
static gpioNotify_t gpioNotify [PI_NOTIFY_SLOTS];

static i2cInfo_t i2cInfo    [PI_I2C_SLOTS];
static serInfo_t serInfo    [PI_SER_SLOTS];
static spiInfo_t spiInfo    [PI_SPI_SLOTS];

static gpioSignal_t gpioSignal [PI_MAX_SIGNUM+1];
static gpioTimer_t gpioTimer  [PI_MAX_TIMER+1];

static int pwmFreq[PWM_FREQS];
//}}}
//{{{  static var - reset after gpioTerminated
// resources which must be released on gpioTerminate */
static int fdMem = -1;
static int fdPmap = -1;
static int fdMbox = -1;

static DMAMem_t* dmaMboxBlk = MAP_FAILED;
static uintptr_t** dmaPMapBlk = MAP_FAILED;
static dmaPage_t** dmaVirt = MAP_FAILED;
static dmaPage_t** dmaBus = MAP_FAILED;

static dmaIPage_t** dmaIVirt = MAP_FAILED;
static dmaIPage_t** dmaIBus = MAP_FAILED;

static dmaOPage_t** dmaOVirt = MAP_FAILED;
static dmaOPage_t** dmaOBus = MAP_FAILED;

static volatile uint32_t* auxReg  = MAP_FAILED;
static volatile uint32_t* bscsReg = MAP_FAILED;
static volatile uint32_t* clkReg  = MAP_FAILED;
static volatile uint32_t* dmaReg  = MAP_FAILED;
static volatile uint32_t* gpioReg = MAP_FAILED;
static volatile uint32_t* padsReg = MAP_FAILED;
static volatile uint32_t* pcmReg  = MAP_FAILED;
static volatile uint32_t* pwmReg  = MAP_FAILED;
static volatile uint32_t* spiReg  = MAP_FAILED;
static volatile uint32_t* systReg = MAP_FAILED;

static volatile uint32_t* dmaIn   = MAP_FAILED;
static volatile uint32_t* dmaOut  = MAP_FAILED;

static uint32_t hw_clk_freq[3];
static uint32_t hw_pwm_freq[2];
static uint32_t hw_pwm_duty[2];
static uint32_t hw_pwm_real_range[2];

static volatile gpioCfg_t gpioCfg = {
  PI_DEFAULT_BUFFER_MILLIS,
  PI_DEFAULT_CLK_MICROS,
  PI_DEFAULT_CLK_PERIPHERAL,
  PI_DEFAULT_DMA_NOT_SET, /* primary DMA */
  PI_DEFAULT_DMA_NOT_SET, /* secondary DMA */
  PI_DEFAULT_IF_FLAGS,
  PI_DEFAULT_MEM_ALLOC_MODE,
  0, /* dbgLevel */
  0, /* alertFreq */
  0, /* internals */
  };
//}}}
//{{{  static var - no initialisation
static unsigned bufferBlocks; /* number of blocks in buffer */
static unsigned bufferCycles; /* number of cycles */

static uint32_t spi_dummy;

static unsigned old_mode_ce0;
static unsigned old_mode_ce1;
static unsigned old_mode_sclk;
static unsigned old_mode_miso;
static unsigned old_mode_mosi;

static uint32_t old_spi_cs;
static uint32_t old_spi_clk;

static unsigned old_mode_ace0;
static unsigned old_mode_ace1;
static unsigned old_mode_ace2;
static unsigned old_mode_asclk;
static unsigned old_mode_amiso;
static unsigned old_mode_amosi;

static uint32_t old_spi_cntl0;
static uint32_t old_spi_cntl1;

static uint32_t bscFR;
//}}}

static uint32_t spiDummyRead;
//}}}
//{{{  static const
//{{{
static const uint8_t clkDef[PI_MAX_GPIO + 1] = {
  /*         0     1     2     3     4     5     6     7     8     9 */
  /* 0 */  0x00, 0x00, 0x00, 0x00, 0x84, 0x94, 0xA4, 0x00, 0x00, 0x00,
  /* 1 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 2 */  0x82, 0x92, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 3 */  0x00, 0x00, 0x84, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 4 */  0x00, 0x00, 0x94, 0xA4, 0x94, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 5 */  0x00, 0x00, 0x00, 0x00,
  };

/*
 7 6 5 4 3 2 1 0
 V . C C . M M M

 V: 0 no clock, 1 has a clock
CC: 00 CLK0, 01 CLK1, 10 CLK2
 M: 100 ALT0, 010 ALT5

 gpio4  GPCLK0 ALT0
 gpio5  GPCLK1 ALT0 B+ and compute module only (reserved for system use)
 gpio6  GPCLK2 ALT0 B+ and compute module only
 gpio20 GPCLK0 ALT5 B+ and compute module only
 gpio21 GPCLK1 ALT5 Not available on Rev.2 B (reserved for system use)

 gpio32 GPCLK0 ALT0 Compute module only
 gpio34 GPCLK0 ALT0 Compute module only
 gpio42 GPCLK1 ALT0 Compute module only (reserved for system use)
 gpio43 GPCLK2 ALT0 Compute module only
 gpio44 GPCLK1 ALT0 Compute module only (reserved for system use)
*/
//}}}
//{{{
static const clkCfg_t clkCfg[]= {
  // valid servo
  { 0,  0}, /*  0 */
  { 1, 17}, /*  1 */
  { 1, 16}, /*  2 */
  { 0,  0}, /*  3 */
  { 1, 15}, /*  4 */
  { 1, 14}, /*  5 */
  { 0,  0}, /*  6 */
  { 0,  0}, /*  7 */
  { 1, 13}, /*  8 */
  { 0,  0}, /*  9 */
  { 1, 12}, /* 10 */
  };
//}}}

//{{{
static const uint8_t PWMDef[PI_MAX_GPIO + 1] = {
  /*         0     1     2     3     4     5     6     7     8     9 */
  /* 0 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 1 */  0x00, 0x00, 0x84, 0x94, 0x00, 0x00, 0x00, 0x00, 0x82, 0x92,
  /* 2 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 3 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 4 */  0x84, 0x94, 0x00, 0x00, 0x00, 0x94, 0x00, 0x00, 0x00, 0x00,
  /* 5 */  0x00, 0x00, 0x85, 0x95,
  };

/*
 7 6 5 4 3 2 1 0
 V . . P . M M M

 V: 0 no PWM, 1 has a PWM
 P: 0 PWM0, 1 PWM1
 M: 010 ALT5, 100 ALT0, 101 ALT1

 gpio12 pwm0 ALT0
 gpio13 pwm1 ALT0
 gpio18 pwm0 ALT5
 gpio19 pwm1 ALT5
 gpio40 pwm0 ALT0
 gpio41 pwm1 ALT0
 gpio45 pwm1 ALT0
 gpio52 pwm0 ALT1
 gpio53 pwm1 ALT1
*/
//}}}
//{{{
static const uint16_t pwmCycles[PWM_FREQS] = {
   1,    2,    4,    5,    8,   10,   16,    20,    25,
  32,   40,   50,   80,  100,  160,  200,   400,   800
  };
//}}}
//{{{
static const uint16_t pwmRealRange[PWM_FREQS] = {
   25,   50,  100,  125,  200,  250,  400,   500,   625,
  800, 1000, 1250, 2000, 2500, 4000, 5000, 10000, 20000
  };
//}}}
//}}}

//{{{  helpers
//{{{
static char* myTimeStamp() {

  static struct timeval last;
  static char buf[32];

  struct timeval now;
  gettimeofday(&now, NULL);

  if (now.tv_sec != last.tv_sec) {
    struct tm tmp;
    localtime_r(&now.tv_sec, &tmp);
    strftime(buf, sizeof(buf), "%F %T", &tmp);
    last.tv_sec = now.tv_sec;
    }

  return buf;
  }
//}}}
//{{{
static char* myBuf2Str (unsigned count, char* buf) {

  static char str[128];

  int c;
  if (count && buf) {
    if (count > 40)
      c = 40;
    else
      c = count;

    for (int i = 0; i < c; i++)
      sprintf (str+(3*i), "%02X ", buf[i]);
    str[(3*c)-1] = 0;
    }
  else
    str[0] = 0;

  return str;
  }
//}}}
//{{{
static void sigHandler (int signum) {

  if ((signum >= PI_MIN_SIGNUM) && (signum <= PI_MAX_SIGNUM)) {
    if (gpioSignal[signum].func) {
     if (gpioSignal[signum].ex)
       (gpioSignal[signum].func)(signum, gpioSignal[signum].userdata);
     else
        (gpioSignal[signum].func)(signum);
      }

    else {
      switch (signum) {
        case SIGUSR1:
          if (gpioCfg.dbgLevel > DBG_MIN_LEVEL)
            --gpioCfg.dbgLevel;
          else
            gpioCfg.dbgLevel = DBG_MIN_LEVEL;
          DBG (DBG_USER, "Debug level %d\n", gpioCfg.dbgLevel);
          break;

        case SIGUSR2:
          if (gpioCfg.dbgLevel < DBG_MAX_LEVEL)
            ++gpioCfg.dbgLevel;
          else
            gpioCfg.dbgLevel = DBG_MAX_LEVEL;
          DBG (DBG_USER, "Debug level %d\n", gpioCfg.dbgLevel);
          break;

        case SIGPIPE:
        case SIGWINCH:
          DBG (DBG_USER, "signal %d ignored", signum);
          break;

        case SIGCHLD:
          /* Used to notify threads of events */
          break;

        default:
          DBG (DBG_ALWAYS, "Unhandled signal %d, terminating\n", signum);
          gpioTerminate();
          exit (-1);
        }
      }
    }

  else {
    // exit
    DBG (DBG_ALWAYS, "Unhandled signal %d, terminating\n", signum);
    gpioTerminate();
    exit(-1);
    }
  }
//}}}
//{{{
static void sigSetHandler() {

  struct sigaction new;
  for (int i = PI_MIN_SIGNUM; i <= PI_MAX_SIGNUM; i++) {
    memset (&new, 0, sizeof(new));
    new.sa_handler = sigHandler;
    sigaction (i, &new, NULL);
    }
  }
//}}}

//{{{
static void myOffPageSlot (int pos, int* page, int* slot) {
  *page = pos / OFF_PER_IPAGE;
  *slot = pos % OFF_PER_IPAGE;
  }
//}}}
//{{{
static void myLvsPageSlot (int pos, int* page, int* slot) {
  *page = pos/LVS_PER_IPAGE;
  *slot = pos%LVS_PER_IPAGE;
  }
//}}}
//{{{
static void myTckPageSlot (int pos, int* page, int* slot) {
  *page = pos / TCK_PER_IPAGE;
  *slot = pos % TCK_PER_IPAGE;
  }
//}}}
//{{{
static int my_smbus_access (int fd, char rw, uint8_t cmd, int size, union my_smbus_data *data) {

  DBG (DBG_INTERNAL, "rw=%d reg=%d cmd=%d data=%s", rw, cmd, size, myBuf2Str(data->byte+1, (char*)data));

  struct my_smbus_ioctl_data args;
  args.read_write = rw;
  args.command = cmd;
  args.size = size;
  args.data = data;
  return ioctl (fd, PI_I2C_SMBUS, &args);
  }
//}}}

//{{{
static int myI2CGetPar (char* inBuf, int* inPos, int inLen, int* esc)
{
   int bytes;

   if (*esc) bytes = 2; else bytes = 1;

   *esc = 0;

   if (*inPos <= (inLen - bytes))
   {
      if (bytes == 1)
      {
         return inBuf[(*inPos)++];
      }
      else
      {
         (*inPos) += 2;
         return inBuf[*inPos-2] + (inBuf[*inPos-1]<<8);
      }
   }
   return -1;
}
//}}}

//{{{
static void flushMemory() {

  static int val = 0;

  void* dummy = mmap (0, (FLUSH_PAGES * PAGE_SIZE),
                      PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_SHARED | MAP_ANONYMOUS |MAP_NORESERVE | MAP_LOCKED,
                      -1, 0);

  if (dummy == MAP_FAILED) {
    DBG (DBG_STARTUP, "mmap dummy failed (%m)");
    }
  else {
    memset (dummy, val++, (FLUSH_PAGES*PAGE_SIZE));
    memset (dummy, val++, (FLUSH_PAGES*PAGE_SIZE));
    munmap (dummy, FLUSH_PAGES*PAGE_SIZE);
    }
  }
//}}}
//{{{
static void wfRx_lock (int i) {
  pthread_mutex_lock (&wfRx[i].mutex);
  }
//}}}
//{{{
static void wfRx_unlock (int i) {
  pthread_mutex_unlock(&wfRx[i].mutex);
  }
//}}}

// mailbox
//{{{
/*
https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
*/
static int mbCreate (char *dev)
{
   /* <0 error */

   unlink(dev);

   return mknod(dev, S_IFCHR|0600, makedev(MB_DEV_MAJOR, 0));
}
//}}}
//{{{
static int mbOpen()
{
   /* <0 error */

   int fd;

   fd = open(MB_DEV1, 0);

   if (fd < 0)
   {
      mbCreate(MB_DEV2);
      fd = open(MB_DEV2, 0);
   }
   return fd;
}
//}}}
//{{{
static void mbClose (int fd)
{
   close(fd);
}
//}}}
//{{{
static int mbProperty (int fd, void *buf)
{
   return ioctl(fd, MB_IOCTL, buf);
}
//}}}
//{{{
static unsigned mbAllocateMemory (int fd, unsigned size, unsigned align, unsigned flags)
{
   int i=1;
   unsigned p[32];

   p[i++] = MB_PROCESS_REQUEST;
   p[i++] = MB_ALLOCATE_MEMORY_TAG;
   p[i++] = 12;
   p[i++] = 12;
   p[i++] = size;
   p[i++] = align;
   p[i++] = flags;
   p[i++] = MB_END_TAG;
   p[0] = i*sizeof(*p);

   mbProperty(fd, p);

   return p[5];
}
//}}}
//{{{
static unsigned mbLockMemory (int fd, unsigned handle)
{
   int i=1;
   unsigned p[32];

   p[i++] = MB_PROCESS_REQUEST;
   p[i++] = MB_LOCK_MEMORY_TAG;
   p[i++] = 4;
   p[i++] = 4;
   p[i++] = handle;
   p[i++] = MB_END_TAG;
   p[0] = i*sizeof(*p);

   mbProperty(fd, p);

   return p[5];
}
//}}}
//{{{
static unsigned mbUnlockMemory (int fd, unsigned handle)
{
   int i=1;
   unsigned p[32];

   p[i++] = MB_PROCESS_REQUEST;
   p[i++] = MB_UNLOCK_MEMORY_TAG;
   p[i++] = 4;
   p[i++] = 4;
   p[i++] = handle;
   p[i++] = MB_END_TAG;
   p[0] = i*sizeof(*p);

   mbProperty(fd, p);

   return p[5];
}
//}}}
//{{{
static unsigned mbReleaseMemory (int fd, unsigned handle)
{
   int i=1;
   unsigned p[32];

   p[i++] = MB_PROCESS_REQUEST;
   p[i++] = MB_RELEASE_MEMORY_TAG;
   p[i++] = 4;
   p[i++] = 4;
   p[i++] = handle;
   p[i++] = MB_END_TAG;
   p[0] = i*sizeof(*p);

   mbProperty(fd, p);

   return p[5];
}
//}}}
//{{{
static void* mbMapMem (unsigned base, unsigned size)
{
   void *mem = MAP_FAILED;

   mem = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, fdMem, base);

   return mem;
}
//}}}
//{{{
static int mbUnmapMem (void *addr, unsigned size)
{
   /* 0 okay, -1 fail */
   return munmap(addr, size);
}
//}}}
//{{{
static void mbDMAFree (DMAMem_t *DMAMemP)
{
   if (DMAMemP->handle)
   {
      mbUnmapMem(DMAMemP->virtual_addr, DMAMemP->size);
      mbUnlockMemory(fdMbox, DMAMemP->handle);
      mbReleaseMemory(fdMbox, DMAMemP->handle);
      DMAMemP->handle = 0;
   }
}
//}}}
//{{{
static int mbDMAAlloc (DMAMem_t *DMAMemP, unsigned size, uint32_t pi_mem_flag)
{
   DMAMemP->size = size;

   DMAMemP->handle =
      mbAllocateMemory(fdMbox, size, PAGE_SIZE, pi_mem_flag);

   if (DMAMemP->handle)
   {
      DMAMemP->bus_addr = mbLockMemory(fdMbox, DMAMemP->handle);

      DMAMemP->virtual_addr =
         mbMapMem(BUS_TO_PHYS(DMAMemP->bus_addr), size);

      return 1;
   }
   return 0;
}
//}}}

// dma
//{{{
static rawCbs_t* dmaCB2adr(int pos)
{
   int page, slot;

   page = pos/CBS_PER_IPAGE;
   slot = pos%CBS_PER_IPAGE;

   return &dmaIVirt[page]->cb[slot];
}
//}}}
//{{{
static uint32_t dmaPwmDataAdr(int pos)
{
   //cast twice to suppress compiler warning, I belive this cast is ok
   //because dmaIbus contains bus addresses, not user addresses. --plugwash
   return (uint32_t)(uintptr_t) &dmaIBus[pos]->periphData;
}
//}}}
//{{{
static uint32_t dmaGpioOnAdr(int pos) {

  int page = pos/ON_PER_IPAGE;
  int slot = pos%ON_PER_IPAGE;

  //cast twice to suppress compiler warning, I belive this cast is ok
  //because dmaIbus contains bus addresses, not user addresses. --plugwash
  return (uint32_t)(uintptr_t) &dmaIBus[page]->gpioOn[slot];
  }
//}}}
//{{{
static uint32_t dmaGpioOffAdr (int pos) {

  int page, slot;
  myOffPageSlot(pos, &page, &slot);

  //cast twice to suppress compiler warning, I belive this cast is ok
  //because dmaIbus contains bus addresses, not user addresses. --plugwash
  return (uint32_t)(uintptr_t) &dmaIBus[page]->gpioOff[slot];
  }
//}}}
//{{{
static uint32_t dmaTickAdr(int pos) {

  int page, slot;
  myTckPageSlot (pos, &page, &slot);

  //cast twice to suppress compiler warning, I belive this cast is ok
  //because dmaIbus contains bus addresses, not user addresses. --plugwash
  return (uint32_t)(uintptr_t) &dmaIBus[page]->tick[slot];
  }
//}}}
//{{{
static uint32_t dmaReadLevelsAdr(int pos)
{
   int page, slot;

   myLvsPageSlot(pos, &page, &slot);

   //cast twice to suppress compiler warning, I belive this cast is ok
   //because dmaIbus contains bus addresses, not user addresses. --plugwash
   return (uint32_t)(uintptr_t) &dmaIBus[page]->level[slot];
}
//}}}
//{{{
static uint32_t dmaCbAdr(int pos)
{
   int page, slot;

   page = (pos/CBS_PER_IPAGE);
   slot = (pos%CBS_PER_IPAGE);

   //cast twice to suppress compiler warning, I belive this cast is ok
   //because dmaIbus contains bus addresses, not user addresses. --plugwash
   return (uint32_t)(uintptr_t) &dmaIBus[page]->cb[slot];
}
//}}}

//{{{
static void dmaGpioOnCb(int b, int pos)
{
   rawCbs_t * p;

   p = dmaCB2adr(b);

   p->info   = NORMAL_DMA;
   p->src    = dmaGpioOnAdr(pos);
   p->dst    = ((GPIO_BASE + (GPSET0*4)) & 0x00ffffff) | PI_PERI_BUS;
   p->length = 4;
   p->next   = dmaCbAdr(b+1);
}
//}}}
//{{{
static void dmaTickCb(int b, int pos)
{
   rawCbs_t * p;

   p = dmaCB2adr(b);

   p->info   = NORMAL_DMA;
   p->src    = ((SYST_BASE + (SYST_CLO*4)) & 0x00ffffff) | PI_PERI_BUS;
   p->dst    = dmaTickAdr(pos);
   p->length = 4;
   p->next   = dmaCbAdr(b+1);
}
//}}}
//{{{
static void dmaGpioOffCb(int b, int pos)
{
   rawCbs_t * p;

   p = dmaCB2adr(b);

   p->info   = NORMAL_DMA;
   p->src    = dmaGpioOffAdr(pos);
   p->dst    = ((GPIO_BASE + (GPCLR0*4)) & 0x00ffffff) | PI_PERI_BUS;
   p->length = 4;
   p->next   = dmaCbAdr(b+1);
}
//}}}
//{{{
static void dmaReadLevelsCb(int b, int pos)
{
   rawCbs_t * p;

   p = dmaCB2adr(b);

   p->info   = NORMAL_DMA;
   p->src    = ((GPIO_BASE + (GPLEV0*4)) & 0x00ffffff) | PI_PERI_BUS;
   p->dst    = dmaReadLevelsAdr(pos);
   p->length = 4;
   p->next   = dmaCbAdr(b+1);
}
//}}}
//{{{
static void dmaDelayCb(int b)
{
   rawCbs_t * p;

   p = dmaCB2adr(b);

   if (gpioCfg.clockPeriph == PI_CLOCK_PCM)
   {
      p->info = NORMAL_DMA | TIMED_DMA(2);
      p->dst  = PCM_TIMER;
   }
   else
   {
      p->info = NORMAL_DMA | TIMED_DMA(5);
      p->dst  = PWM_TIMER;
   }

   p->src    = dmaPwmDataAdr(b%DMAI_PAGES);
   p->length = 4;
   p->next   = dmaCbAdr(b+1);
}
//}}}
//{{{
static void dmaInitCbs() {


  /* set up the DMA control blocks */
  DBG (DBG_STARTUP, "");

  int b = -1;
  int level = 0;

  for (int cycle = 0; cycle < bufferCycles; cycle++) {
    b++; dmaGpioOnCb(b, cycle%SUPERCYCLE); /* gpio on slot */
    b++; dmaTickCb(b, cycle);              /* tick slot */
    for (int pulse = 0; pulse < PULSE_PER_CYCLE; pulse++) {
      b++; dmaReadLevelsCb(b, level);               /* read levels slot */
      b++; dmaDelayCb(b);                           /* delay slot */
      b++; dmaGpioOffCb(b, (level%SUPERLEVEL)+1);   /* gpio off slot */
      ++level;
      }
    }

  // point last cb back to first for continuous loop
  rawCbs_t* p = dmaCB2adr(b);
  p->next = dmaCbAdr(0);

  DBG (DBG_STARTUP, "DMA page type count = %zd", sizeof(dmaIPage_t));
  DBG (DBG_STARTUP, "%d control blocks (exp=%d)", b+1, NUM_CBS);
  }
//}}}
//}}}
//{{{  time
//{{{
static void myGpioSleep (int seconds, int micros) {

  struct timespec ts;
  ts.tv_sec  = seconds;
  ts.tv_nsec = micros * 1000;

  struct timespec rem;
  while (clock_nanosleep(CLOCK_REALTIME, 0, &ts, &rem))
    // copy remaining time to ts */
    ts = rem;
  }
//}}}
//{{{
static uint32_t myGpioDelay (uint32_t micros) {

  uint32_t start= systReg[SYST_CLO];

  if (micros <= PI_MAX_BUSY_DELAY) {
    while ((systReg[SYST_CLO] - start) <= micros);
    }
  else
    myGpioSleep (micros/MILLION, micros % MILLION);

  return (systReg[SYST_CLO] - start);
  }
//}}}

//{{{
double timeTime() {

  struct timeval tv;
  gettimeofday(&tv, 0);
  return (double)tv.tv_sec + ((double)tv.tv_usec / 1E6);
  }
//}}}
//{{{
void timeSleep (double seconds)
{
   struct timespec ts, rem;

   if (seconds > 0.0)
   {
      ts.tv_sec = seconds;
      ts.tv_nsec = (seconds-(double)ts.tv_sec) * 1E9;

      while (clock_nanosleep(CLOCK_REALTIME, 0, &ts, &rem))
      {
         /* copy remaining time to ts */
         ts.tv_sec  = rem.tv_sec;
         ts.tv_nsec = rem.tv_nsec;
      }
   }
}
//}}}
//{{{
int gpioTime (unsigned timetype, int* seconds, int* micros) {

  struct timespec ts;

  if (timetype > PI_TIME_ABSOLUTE)
    SOFT_ERROR(PI_BAD_TIMETYPE, "bad timetype (%d)", timetype);

  if (timetype == PI_TIME_ABSOLUTE) {
    clock_gettime(CLOCK_REALTIME, &ts);
    *seconds = ts.tv_sec;
    *micros  = ts.tv_nsec/1000;
    }
  else {
    clock_gettime (CLOCK_REALTIME, &ts);
    TIMER_SUB (&ts, &libStarted, &ts);
    *seconds = ts.tv_sec;
    *micros  = ts.tv_nsec/1000;
    }

   return 0;
  }
//}}}
//{{{
int gpioSleep (unsigned timetype, int seconds, int micros) {

  if (timetype > PI_TIME_ABSOLUTE)
    SOFT_ERROR (PI_BAD_TIMETYPE, "bad timetype (%d)", timetype);

  if (seconds < 0)
    SOFT_ERROR (PI_BAD_SECONDS, "bad seconds (%d)", seconds);

  if ((micros < 0) || (micros > 999999))
    SOFT_ERROR (PI_BAD_MICROS, "bad micros (%d)", micros);

  struct timespec ts;
  ts.tv_sec  = seconds;
  ts.tv_nsec = micros * 1000;

  struct timespec rem;
  if (timetype == PI_TIME_ABSOLUTE) {
    while (clock_nanosleep (CLOCK_REALTIME, TIMER_ABSTIME, &ts, &rem)) {}
    }
  else {
    while (clock_nanosleep (CLOCK_REALTIME, 0, &ts, &rem)) {
      // copy remaining time to ts
      ts.tv_sec  = rem.tv_sec;
      ts.tv_nsec = rem.tv_nsec;
      }
    }

  return 0;
 }
//}}}
//{{{
uint32_t gpioDelay (uint32_t micros) {

  uint32_t start;

  start = systReg[SYST_CLO];

  if (micros <= PI_MAX_BUSY_DELAY)
    while ((systReg[SYST_CLO] - start) <= micros) {}
  else
    gpioSleep (PI_TIME_RELATIVE, (micros/MILLION), (micros%MILLION));

  return (systReg[SYST_CLO] - start);
  }
//}}}
//{{{
uint32_t gpioTick() {
  return systReg[SYST_CLO];
  }
//}}}
//}}}

//{{{  init
//{{{
static void initClearGlobals() {

  wfc[0] = 0;
  wfc[1] = 0;
  wfc[2] = 0;
  wfcur = 0;

  wfStats.micros     = 0;
  wfStats.highMicros = 0;
  wfStats.maxMicros  = PI_WAVE_MAX_MICROS;
  wfStats.pulses     = 0;
  wfStats.highPulses = 0;
  wfStats.maxPulses  = PI_WAVE_MAX_PULSES;
  wfStats.cbs        = 0;
  wfStats.highCbs    = 0;
  wfStats.maxCbs     = (PI_WAVE_BLOCKS * PAGES_PER_BLOCK * CBS_PER_OPAGE);

  gpioGetSamples.func     = NULL;
  gpioGetSamples.ex       = 0;
  gpioGetSamples.userdata = NULL;
  gpioGetSamples.bits     = 0;

  for (int i = 0; i <= PI_MAX_USER_GPIO; i++) {
    wfRx[i].mode = PI_WFRX_NONE;
    pthread_mutex_init (&wfRx[i].mutex, NULL);
    }

  for (int i = 0; i <= PI_MAX_GPIO; i++) {
    gpioInfo [i].is = GPIO_UNDEFINED;
    gpioInfo [i].width = 0;
    gpioInfo [i].range = PI_DEFAULT_DUTYCYCLE_RANGE;
    gpioInfo [i].freqIdx = DEFAULT_PWM_IDX;
    }

  for (int i = 0; i < PI_NOTIFY_SLOTS; i++) {
    gpioNotify[i].seqno = 0;
    gpioNotify[i].state = PI_NOTIFY_CLOSED;
    }

  for (int i = 0; i <= PI_MAX_SIGNUM; i++) {
    gpioSignal[i].func = NULL;
    gpioSignal[i].ex = 0;
    gpioSignal[i].userdata = NULL;
    }

  for (int i = 0; i <= PI_MAX_TIMER; i++) {
    gpioTimer[i].running = 0;
    gpioTimer[i].func = NULL;
    }

  // calculate the usable PWM frequencies */
  for (int i = 0; i < PWM_FREQS; i++) {
    pwmFreq[i]= (1000000.0 / ((float)PULSE_PER_CYCLE * gpioCfg.clockMicros * pwmCycles[i])) + 0.5;
    DBG (DBG_STARTUP, "f%d is %d", i, pwmFreq[i]);
    }

  fdMem = -1;
  dmaMboxBlk = MAP_FAILED;
  dmaPMapBlk = MAP_FAILED;
  dmaVirt = MAP_FAILED;
  dmaBus  = MAP_FAILED;
  auxReg  = MAP_FAILED;
  clkReg  = MAP_FAILED;
  dmaReg  = MAP_FAILED;
  gpioReg = MAP_FAILED;
  pcmReg  = MAP_FAILED;
  pwmReg  = MAP_FAILED;
  systReg = MAP_FAILED;
  spiReg  = MAP_FAILED;
  }
//}}}
//{{{
static int initCheckPermitted() {

  if (!pi_ispi) {
    DBG (DBG_ALWAYS,
         "\n" \
         "+---------------------------------------------------------+\n" \
         "|Sorry, this system does not appear to be a raspberry pi. |\n" \
         "|aborting.                                                |\n" \
         "+---------------------------------------------------------+\n\n");
    return -1;
    }

  if ((fdMem = open("/dev/mem", O_RDWR | O_SYNC) ) < 0) {
    DBG (DBG_ALWAYS,
         "\n" \
         "+---------------------------------------------------------+\n" \
         "|Sorry, you don't have permission to run this program.    |\n" \
         "|Try running as root, e.g. precede the command with sudo. |\n" \
         "+---------------------------------------------------------+\n\n");
    return -1;
    }

  return 0;
  }
//}}}

//{{{
static uint32_t* initMapMem (int fd, uint32_t addr, uint32_t len) {
  return (uint32_t*) mmap (0, len, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_SHARED|MAP_LOCKED, fd, addr);
  }
//}}}
//{{{
static int initPeripherals() {

  gpioReg = initMapMem (fdMem, GPIO_BASE, GPIO_LEN);
  if (gpioReg == MAP_FAILED)
    SOFT_ERROR (PI_INIT_FAILED, "mmap gpio failed (%m)");

  dmaReg = initMapMem (fdMem, DMA_BASE, DMA_LEN);
  if (dmaReg == MAP_FAILED)
    SOFT_ERROR (PI_INIT_FAILED, "mmap dma failed (%m)");

  // we should know if we are running on a BCM2711 by now
  if (gpioCfg.DMAprimaryChannel == PI_DEFAULT_DMA_NOT_SET) {
    if (pi_is_2711)
      gpioCfg.DMAprimaryChannel = PI_DEFAULT_DMA_PRIMARY_CH_2711;
    else
      gpioCfg.DMAprimaryChannel = PI_DEFAULT_DMA_PRIMARY_CHANNEL;
    }

  if (gpioCfg.DMAsecondaryChannel == PI_DEFAULT_DMA_NOT_SET) {
    if (pi_is_2711)
      gpioCfg.DMAsecondaryChannel = PI_DEFAULT_DMA_SECONDARY_CH_2711;
    else
      gpioCfg.DMAsecondaryChannel = PI_DEFAULT_DMA_SECONDARY_CHANNEL;
    }

  dmaIn =  dmaReg + (gpioCfg.DMAprimaryChannel   * 0x40);
  dmaOut = dmaReg + (gpioCfg.DMAsecondaryChannel * 0x40);
  DBG (DBG_STARTUP, "DMA #%d @ %08"PRIXPTR, gpioCfg.DMAprimaryChannel, (uintptr_t)dmaIn);
  DBG (DBG_STARTUP, "debug reg is %08X", dmaIn[DMA_DEBUG]);

  clkReg  = initMapMem (fdMem, CLK_BASE,  CLK_LEN);
  if (clkReg == MAP_FAILED)
    SOFT_ERROR (PI_INIT_FAILED, "mmap clk failed (%m)");

  systReg  = initMapMem (fdMem, SYST_BASE,  SYST_LEN);
  if (systReg == MAP_FAILED)
    SOFT_ERROR (PI_INIT_FAILED, "mmap syst failed (%m)");

  spiReg  = initMapMem (fdMem, SPI_BASE,  SPI_LEN);
  if (spiReg == MAP_FAILED)
    SOFT_ERROR (PI_INIT_FAILED, "mmap spi failed (%m)");

  pwmReg  = initMapMem (fdMem, PWM_BASE,  PWM_LEN);
  if (pwmReg == MAP_FAILED)
    SOFT_ERROR (PI_INIT_FAILED, "mmap pwm failed (%m)");

  pcmReg  = initMapMem (fdMem, PCM_BASE,  PCM_LEN);
  if (pcmReg == MAP_FAILED)
    SOFT_ERROR (PI_INIT_FAILED, "mmap pcm failed (%m)");

  auxReg  = initMapMem (fdMem, AUX_BASE,  AUX_LEN);
  if (auxReg == MAP_FAILED)
    SOFT_ERROR (PI_INIT_FAILED, "mmap aux failed (%m)");

  padsReg  = initMapMem (fdMem, PADS_BASE,  PADS_LEN);
  if (padsReg == MAP_FAILED)
    SOFT_ERROR (PI_INIT_FAILED, "mmap pads failed (%m)");

  bscsReg  = initMapMem (fdMem, BSCS_BASE,  BSCS_LEN);
  if (bscsReg == MAP_FAILED)
    SOFT_ERROR (PI_INIT_FAILED, "mmap bscs failed (%m)");

  return 0;
  }
//}}}

//{{{
static int initZaps (int pmapFd, void* virtualBase, int  basePage, int  pages) {

  int status = 0;
  uintptr_t pageAdr = (uintptr_t) dmaVirt[basePage];
  uintptr_t index = ((uintptr_t)virtualBase / PAGE_SIZE) * 8;

  off_t offset = lseek (pmapFd, index, SEEK_SET);
  if (offset != index)
    SOFT_ERROR (PI_INIT_FAILED, "lseek pagemap failed (%m)");

  for (int n = 0; n < pages; n++) {
    unsigned long long pa;
    ssize_t t = read (pmapFd, &pa, sizeof(pa));
    if (t != sizeof(pa))
      SOFT_ERROR (PI_INIT_FAILED, "read pagemap failed (%m)");

    DBG (DBG_STARTUP, "pf%d=%016llX", n, pa);

    uint32_t physical = 0x3FFFFFFF & (PAGE_SIZE * (pa & 0xFFFFFFFF));
    if (physical) {
      //cast twice to suppress warning, I belive this is ok as these
      //are bus addresses, not virtual addresses. --plugwash
      dmaBus[basePage+n] = (dmaPage_t *)(uintptr_t) (physical | pi_dram_bus);

      dmaVirt[basePage+n] = mmap ((void*)pageAdr, PAGE_SIZE, PROT_READ|PROT_WRITE,
                                  MAP_SHARED|MAP_FIXED|MAP_LOCKED|MAP_NORESERVE, fdMem, physical);
      }
    else
      status = 1;

    pageAdr += PAGE_SIZE;
    }

  return status;
  }
//}}}
//{{{
static int initPagemapBlock (int block) {

  dmaPMapBlk[block] = mmap (0, (PAGES_PER_BLOCK*PAGE_SIZE),
                            PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS|MAP_NORESERVE|MAP_LOCKED, -1, 0);
  if (dmaPMapBlk[block] == MAP_FAILED)
    SOFT_ERROR(PI_INIT_FAILED, "mmap dma block %d failed (%m)", block);

  // force allocation of physical memory
  memset ((void*)dmaPMapBlk[block], 0xAA, (PAGES_PER_BLOCK*PAGE_SIZE));
  memset ((void*)dmaPMapBlk[block], 0xFF, (PAGES_PER_BLOCK*PAGE_SIZE));
  memset ((void*)dmaPMapBlk[block], 0, (PAGES_PER_BLOCK*PAGE_SIZE));
  unsigned pageNum = block * PAGES_PER_BLOCK;
  dmaVirt[pageNum] = mmap (0, (PAGES_PER_BLOCK*PAGE_SIZE),
                           PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS|MAP_NORESERVE|MAP_LOCKED, -1, 0);
  if (dmaVirt[pageNum] == MAP_FAILED)
    SOFT_ERROR (PI_INIT_FAILED, "mmap dma block %d failed (%m)", block);

  munmap (dmaVirt[pageNum], PAGES_PER_BLOCK*PAGE_SIZE);

  int trys = 0;
  int ok = 0;
  while ((trys < 10) && !ok) {
    if (initZaps (fdPmap, dmaPMapBlk[block], pageNum, PAGES_PER_BLOCK) == 0)
      ok = 1;
    else
      myGpioDelay(50000);
    ++trys;
    }

  if (!ok)
    SOFT_ERROR (PI_INIT_FAILED, "initZaps failed");

  return 0;
  }
//}}}
//{{{
static int initMboxBlock (int block) {

  int ok = mbDMAAlloc (&dmaMboxBlk[block], PAGES_PER_BLOCK * PAGE_SIZE, pi_mem_flag);
  if (!ok)
    SOFT_ERROR (PI_INIT_FAILED, "init mbox zaps failed");

  unsigned page = block * PAGES_PER_BLOCK;
  uintptr_t virtualAdr = (uintptr_t) dmaMboxBlk[block].virtual_addr;
  uintptr_t busAdr = dmaMboxBlk[block].bus_addr;

  for (int n = 0; n < PAGES_PER_BLOCK; n++) {
    dmaVirt[page+n] = (dmaPage_t*) virtualAdr;
    dmaBus[page+n] = (dmaPage_t*) busAdr;
    virtualAdr += PAGE_SIZE;
    busAdr += PAGE_SIZE;
    }

  return 0;
  }
//}}}
//{{{
static int initAllocDMAMem() {
// Calculate the number of blocks needed for buffers.  The number
// of blocks must be a multiple of the 20ms servo cycle.

  int servoCycles = gpioCfg.bufferMilliseconds / 20;
  if (gpioCfg.bufferMilliseconds % 20)
    servoCycles++;

  bufferCycles = (SUPERCYCLE * servoCycles) / gpioCfg.clockMicros;

  int superCycles = bufferCycles / SUPERCYCLE;
  if (bufferCycles % SUPERCYCLE)
    superCycles++;

  bufferCycles = SUPERCYCLE * superCycles;
  bufferBlocks = bufferCycles / CYCLES_PER_BLOCK;

  // allocate memory for pointers to virtual and bus memory pages */
  dmaVirt = mmap (0, PAGES_PER_BLOCK*(bufferBlocks+PI_WAVE_BLOCKS)*sizeof(dmaPage_t *),
                  PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_LOCKED, -1, 0);

  if (dmaVirt == MAP_FAILED)
    SOFT_ERROR(PI_INIT_FAILED, "mmap dma virtual failed (%m)");

  dmaBus = mmap (0, PAGES_PER_BLOCK*(bufferBlocks+PI_WAVE_BLOCKS)*sizeof(dmaPage_t *),
                 PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_LOCKED, -1, 0);

  if (dmaBus == MAP_FAILED)
    SOFT_ERROR(PI_INIT_FAILED, "mmap dma bus failed (%m)");

  dmaIVirt = (dmaIPage_t**) dmaVirt;
  dmaIBus = (dmaIPage_t**) dmaBus;
  dmaOVirt = (dmaOPage_t**)(dmaVirt + (PAGES_PER_BLOCK*bufferBlocks));
  dmaOBus = (dmaOPage_t**)(dmaBus  + (PAGES_PER_BLOCK*bufferBlocks));

  if ((gpioCfg.memAllocMode == PI_MEM_ALLOC_PAGEMAP) ||
      ((gpioCfg.memAllocMode == PI_MEM_ALLOC_AUTO) &&
       (gpioCfg.bufferMilliseconds > PI_DEFAULT_BUFFER_MILLIS))) {
    //{{{  pagemap allocation of DMA memory

    dmaPMapBlk = mmap (0, (bufferBlocks+PI_WAVE_BLOCKS)*sizeof(dmaPage_t *),
                       PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_LOCKED, -1, 0);

    if (dmaPMapBlk == MAP_FAILED)
      SOFT_ERROR (PI_INIT_FAILED, "pagemap mmap block failed (%m)");

    fdPmap = open ("/proc/self/pagemap", O_RDONLY);
    if (fdPmap < 0)
      SOFT_ERROR (PI_INIT_FAILED, "pagemap open failed(%m)");

    for (int i = 0; i < (bufferBlocks+PI_WAVE_BLOCKS); i++) {
      int status = initPagemapBlock(i);
      if (status < 0) {
        close(fdPmap);
        return status;
        }
      }

    close (fdPmap);
    }
    //}}}
  else {
    //{{{  mailbox allocation of DMA memory
    dmaMboxBlk = mmap (0, (bufferBlocks+PI_WAVE_BLOCKS)*sizeof(DMAMem_t),
                       PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_LOCKED, -1, 0);

    if (dmaMboxBlk == MAP_FAILED)
      SOFT_ERROR(PI_INIT_FAILED, "mmap mbox block failed (%m)");

    fdMbox = mbOpen();
     if (fdMbox < 0)
      SOFT_ERROR(PI_INIT_FAILED, "mbox open failed(%m)");

    for (int i = 0; i < (bufferBlocks+PI_WAVE_BLOCKS); i++) {
      int status = initMboxBlock(i);
      if (status < 0) {
        mbClose(fdMbox);
        return status;
        }
      }

    mbClose (fdMbox);
    DBG (DBG_STARTUP, "dmaMboxBlk=%08"PRIXPTR" dmaIn=%08"PRIXPTR, (uintptr_t)dmaMboxBlk, (uintptr_t)dmaIn);
    }
    //}}}

  return 0;
  }
//}}}

//{{{
static void initPWM (unsigned bits) {

  DBG (DBG_STARTUP, "bits=%d", bits);

  // reset PWM */
  pwmReg[PWM_CTL] = 0;
  myGpioDelay(10);
  pwmReg[PWM_STA] = -1;
  myGpioDelay(10);

  //set number of bits to transmit */
  pwmReg[PWM_RNG1] = bits;
  myGpioDelay(10);
  dmaIVirt[0]->periphData = 1;

  // enable PWM DMA, raise panic and dreq thresholds to 15 */
  pwmReg[PWM_DMAC] = PWM_DMAC_ENAB | PWM_DMAC_PANIC(15) | PWM_DMAC_DREQ(15);
  myGpioDelay(10);

  //clear PWM fifo */
  pwmReg[PWM_CTL] = PWM_CTL_CLRF1;
  myGpioDelay(10);

  // enable PWM channel 1 and use fifo */
  pwmReg[PWM_CTL] = PWM_CTL_USEF1 | PWM_CTL_MODE1 | PWM_CTL_PWEN1;
  }
//}}}
//{{{
static void initPCM (unsigned bits) {

  // disable PCM so we can modify the regs
  pcmReg[PCM_CS] = 0;

  myGpioDelay (1000);

  pcmReg[PCM_FIFO]   = 0;
  pcmReg[PCM_MODE]   = 0;
  pcmReg[PCM_RXC]    = 0;
  pcmReg[PCM_TXC]    = 0;
  pcmReg[PCM_DREQ]   = 0;
  pcmReg[PCM_INTEN]  = 0;
  pcmReg[PCM_INTSTC] = 0;
  pcmReg[PCM_GRAY]   = 0;
  myGpioDelay(1000);

  pcmReg[PCM_MODE] = PCM_MODE_FLEN(bits-1); /* # bits in frame */

  // enable channel 1 with # bits width */
  pcmReg[PCM_TXC] = PCM_TXC_CH1EN | PCM_TXC_CH1WID(bits-8);
  pcmReg[PCM_CS] |= PCM_CS_STBY; /* clear standby */
  myGpioDelay(1000);

  pcmReg[PCM_CS] |= PCM_CS_TXCLR; /* clear TX FIFO */
  pcmReg[PCM_CS] |= PCM_CS_DMAEN; /* enable DREQ */
  pcmReg[PCM_DREQ] = PCM_DREQ_TX_PANIC(16) | PCM_DREQ_TX_REQ_L(30);
  pcmReg[PCM_INTSTC] = 0b1111; /* clear status bits */

  // enable PCM
  pcmReg[PCM_CS] |= PCM_CS_EN;

  // enable tx
  pcmReg[PCM_CS] |= PCM_CS_TXON;
  dmaIVirt[0]->periphData = 0x0F;
  }
//}}}
//{{{
static void initHWClk (int clkCtl, int clkDiv, int clkSrc, int divI, int divF, int MASH) {

  // kill the clock if busy, anything else isn't reliable */
  if (clkReg[clkCtl] & CLK_CTL_BUSY) {
    do {
      clkReg[clkCtl] = BCM_PASSWD | CLK_CTL_KILL;
      } while (clkReg[clkCtl] & CLK_CTL_BUSY);
    }

  clkReg[clkDiv] = (BCM_PASSWD | CLK_DIV_DIVI(divI) | CLK_DIV_DIVF(divF));
  usleep (10);

  clkReg[clkCtl] = (BCM_PASSWD | CLK_CTL_MASH(MASH) | CLK_CTL_SRC(clkSrc));
  usleep (10);

  clkReg[clkCtl] |= (BCM_PASSWD | CLK_CTL_ENAB);
  }
//}}}
//{{{
static void initClock (int mainClock) {

  const unsigned BITS = 10;

  unsigned clkCtl, clkDiv, clkSrc, clkDivI, clkDivF, clkMash, clkBits;
  char* per;

  unsigned micros;
  if (mainClock)
    micros = gpioCfg.clockMicros;
  else
    micros = PI_WF_MICROS;

  int clockPWM;
  clockPWM = mainClock ^ (gpioCfg.clockPeriph == PI_CLOCK_PCM);
  if (clockPWM) {
    clkCtl = CLK_PWMCTL;
    clkDiv = CLK_PWMDIV;
    per = "PWM";
    }
  else {
    clkCtl = CLK_PCMCTL;
    clkDiv = CLK_PCMDIV;
    per = "PCM";
    }

  clkSrc  = CLK_CTL_SRC_PLLD;
  clkDivI = clk_plld_freq / (10000000 / micros); /* 10 MHz - 1 MHz */
  clkBits = BITS;        /* 10/BITS MHz - 1/BITS MHz */
  clkDivF = 0;
  clkMash = 0;

  DBG (DBG_STARTUP, "%s PLLD divi=%d divf=%d mash=%d bits=%d", per, clkDivI, clkDivF, clkMash, clkBits);

  initHWClk (clkCtl, clkDiv, clkSrc, clkDivI, clkDivF, clkMash);

  if (clockPWM)
    initPWM (BITS);
  else
    initPCM (BITS);

  myGpioDelay (2000);
  }
//}}}

//{{{
static void initKillDMA (volatile uint32_t* dmaAddr) {

  dmaAddr[DMA_CS] = DMA_CHANNEL_ABORT;
  dmaAddr[DMA_CS] = 0;
  dmaAddr[DMA_CS] = DMA_CHANNEL_RESET;
  dmaAddr[DMA_CONBLK_AD] = 0;
  }
//}}}
//{{{
static void initDMAgo (volatile uint32_t* dmaAddr, uint32_t cbAddr) {

  initKillDMA(dmaAddr);
  dmaAddr[DMA_CS] = DMA_INTERRUPT_STATUS | DMA_END_FLAG;
  dmaAddr[DMA_CONBLK_AD] = cbAddr;

  // clear READ/FIFO/READ_LAST_NOT_SET error bits */
  dmaAddr[DMA_DEBUG] = DMA_DEBUG_READ_ERR | DMA_DEBUG_FIFO_ERR | DMA_DEBUG_RD_LST_NOT_SET_ERR;
  dmaAddr[DMA_CS] = DMA_WAIT_ON_WRITES | DMA_PANIC_PRIORITY(8) | DMA_PRIORITY(8) | DMA_ACTIVE;
  }
//}}}

//{{{
static void initReleaseResources() {

  for (int i = 0; i <= PI_MAX_TIMER; i++) {
    if (gpioTimer[i].running) {
      //{{{  destroy thread
      pthread_cancel(gpioTimer[i].pthId);
      pthread_join(gpioTimer[i].pthId, NULL);
      gpioTimer[i].running = 0;
      }
      //}}}
    }

  //{{{  release mmap'd memory
  if (auxReg  != MAP_FAILED) munmap((void*)auxReg,  AUX_LEN);
  if (bscsReg != MAP_FAILED) munmap((void*)bscsReg, BSCS_LEN);
  if (clkReg  != MAP_FAILED) munmap((void*)clkReg,  CLK_LEN);
  if (dmaReg  != MAP_FAILED) munmap((void*)dmaReg,  DMA_LEN);
  if (gpioReg != MAP_FAILED) munmap((void*)gpioReg, GPIO_LEN);
  if (pcmReg  != MAP_FAILED) munmap((void*)pcmReg,  PCM_LEN);
  if (pwmReg  != MAP_FAILED) munmap((void*)pwmReg,  PWM_LEN);
  if (systReg != MAP_FAILED) munmap((void*)systReg, SYST_LEN);
  if (spiReg  != MAP_FAILED) munmap((void*)spiReg,  SPI_LEN);
  auxReg  = MAP_FAILED;
  bscsReg = MAP_FAILED;
  clkReg  = MAP_FAILED;
  dmaReg  = MAP_FAILED;
  gpioReg = MAP_FAILED;
  pcmReg  = MAP_FAILED;
  pwmReg  = MAP_FAILED;
  systReg = MAP_FAILED;
  spiReg  = MAP_FAILED;
  //}}}

  if (dmaBus != MAP_FAILED)
    munmap (dmaBus, PAGES_PER_BLOCK*(bufferBlocks+PI_WAVE_BLOCKS)*sizeof(dmaPage_t *));
  dmaBus = MAP_FAILED;

  if (dmaVirt != MAP_FAILED) {
    for (int i = 0; i < PAGES_PER_BLOCK*(bufferBlocks+PI_WAVE_BLOCKS); i++)
      munmap(dmaVirt[i], PAGE_SIZE);
    munmap (dmaVirt, PAGES_PER_BLOCK*(bufferBlocks+PI_WAVE_BLOCKS)*sizeof(dmaPage_t *));
    }
  dmaVirt = MAP_FAILED;

  if (dmaPMapBlk != MAP_FAILED) {
    for (int i = 0; i < (bufferBlocks+PI_WAVE_BLOCKS); i++)
      munmap(dmaPMapBlk[i], PAGES_PER_BLOCK*PAGE_SIZE);

    munmap(dmaPMapBlk, (bufferBlocks+PI_WAVE_BLOCKS)*sizeof(dmaPage_t *));
    }
  dmaPMapBlk = MAP_FAILED;

  if (dmaMboxBlk != MAP_FAILED) {
    fdMbox = mbOpen();
    for (int i = 0; i < (bufferBlocks + PI_WAVE_BLOCKS); i++)
      mbDMAFree (&dmaMboxBlk[bufferBlocks + PI_WAVE_BLOCKS-i-1]);
    mbClose (fdMbox);
    munmap (dmaMboxBlk, (bufferBlocks + PI_WAVE_BLOCKS) * sizeof(DMAMem_t));
    }
  dmaMboxBlk = MAP_FAILED;

  if (fdMem != -1) {
    close(fdMem);
    fdMem = -1;
    }

  if (fdPmap != -1) {
    close(fdPmap);
    fdPmap = -1;
    }

  if (fdMbox != -1) {
    close(fdMbox);
    fdMbox = -1;
    }
  }
//}}}

//{{{
static int initInitialise() {

  PWMClockInited = 0;
  clock_gettime (CLOCK_REALTIME, &libStarted);
  unsigned rev = gpioHardwareRevision();

  initClearGlobals();
  if (initCheckPermitted() < 0)
    return PI_INIT_FAILED;

  if (!gpioMaskSet) {
     //{{{  get gpioMask
     if (rev ==  0)
       gpioMask = PI_DEFAULT_UPDATE_MASK_UNKNOWN;
     else if (rev <   4)
       gpioMask = PI_DEFAULT_UPDATE_MASK_B1;
     else if (rev <  16)
       gpioMask = PI_DEFAULT_UPDATE_MASK_A_B2;
     else if (rev == 17)
       gpioMask = PI_DEFAULT_UPDATE_MASK_COMPUTE;
     else if (rev  < 20)
       gpioMask = PI_DEFAULT_UPDATE_MASK_APLUS_BPLUS;
     else if (rev == 20)
       gpioMask = PI_DEFAULT_UPDATE_MASK_COMPUTE;
     else if (rev == 21)
       gpioMask = PI_DEFAULT_UPDATE_MASK_APLUS_BPLUS;
     else {
        //{{{  description
        /* model
        0=A 1=B
        2=A+ 3=B+
        4=Pi2B
        5=Alpha
        6=Compute Module
        7=Unknown
        8=Pi3B
        9=Zero
        12=Zero W
        13=Pi3B+
        14=Pi3A+
        17=Pi4B
        */
        //}}}
        unsigned model = (rev >> 4) & 0xFF;
        if (model <  2)
          gpioMask = PI_DEFAULT_UPDATE_MASK_A_B2;
        else if (model <  4)
          gpioMask = PI_DEFAULT_UPDATE_MASK_APLUS_BPLUS;
        else if (model == 4)
         gpioMask = PI_DEFAULT_UPDATE_MASK_PI2B;
        else if (model == 6 || model == 10 || model == 16)
          gpioMask = PI_DEFAULT_UPDATE_MASK_COMPUTE;
        else if (model == 8 || model == 13 || model == 14)
          gpioMask = PI_DEFAULT_UPDATE_MASK_PI3B;
        else if (model == 9 || model == 12)
          gpioMask = PI_DEFAULT_UPDATE_MASK_ZERO;
        else if (model ==17)
          gpioMask = PI_DEFAULT_UPDATE_MASK_PI4B;
        else
          gpioMask = PI_DEFAULT_UPDATE_MASK_UNKNOWN;
       }

     gpioMaskSet = 1;
     }
     //}}}

  if (!(gpioCfg.internals & PI_CFG_NOSIGHANDLER))
    sigSetHandler();

  if (initPeripherals() < 0)
    return PI_INIT_FAILED;

  if (initAllocDMAMem() < 0)
    return PI_INIT_FAILED;

  if (fdMem != -1) {
    //{{{  close /dev/mem
    close (fdMem);
    fdMem = -1;
    }
    //}}}

  struct sched_param param;
  param.sched_priority = sched_get_priority_max (SCHED_FIFO);
  if (gpioCfg.internals & PI_CFG_RT_PRIORITY)
    sched_setscheduler (0, SCHED_FIFO, &param);

  initClock (1);
  atexit (gpioTerminate);

  pthread_attr_t pthAttr;
  if (pthread_attr_init (&pthAttr))
    SOFT_ERROR(PI_INIT_FAILED, "pthread_attr_init failed (%m)");
  if (pthread_attr_setstacksize (&pthAttr, STACK_SIZE))
    SOFT_ERROR(PI_INIT_FAILED, "pthread_attr_setstacksize failed (%m)");

  myGpioDelay (1000);
  dmaInitCbs();
  flushMemory();

  // cast twice to suppress compiler warning, I belive this cast
  // is ok because dmaIBus contains bus addresses, not virtual addresses.
  initDMAgo ((uint32_t*)dmaIn, (uint32_t)(uintptr_t)dmaIBus[0]);

  return PIGPIO_VERSION;
  }
//}}}
//}}}
unsigned gpioVersion() { return PIGPIO_VERSION; }
//{{{
unsigned gpioHardwareRevision() {
//{{{  description
// 2 2  2  2 2 2  1 1 1 1  1 1 1 1  1 1 0 0 0 0 0 0  0 0 0 0
// 5 4  3  2 1 0  9 8 7 6  5 4 3 2  1 0 9 8 7 6 5 4  3 2 1 0
// W W  S  M M M  B B B B  P P P P  T T T T T T T T  R R R R
// W  warranty void if either bit is set
// S  0=old (bits 0-22 are revision number) 1=new (following fields apply)
// M  0=256 1=512 2=1024 3=2GB 4=4GB
// B  0=Sony 1=Egoman 2=Embest 3=Sony Japan 4=Embest 5=Stadium
// P  0=2835, 1=2836, 2=2837 3=2711
// T  0=A 1=B 2=A+ 3=B+ 4=Pi2B 5=Alpha 6=CM1 8=Pi3B 9=Zero a=CM3 c=Zero W d=3B+ e=3A+ 10=CM3+ 11=4B
// R  PCB board revision
//}}}

  unsigned rev = 0;

  FILE* filp = fopen ("/proc/cpuinfo", "r");
  if (filp != NULL) {
    char buf[512];
    while (fgets (buf, sizeof(buf), filp) != NULL) {
      if (!strncasecmp ("revision\t:", buf, 10)) {
        char term;
        if (sscanf (buf+10, "%x%c", &rev, &term) == 2) {
          if (term != '\n')
            rev = 0;
          }
        }
      }
    fclose (filp);
    }

  // (some) arm64 operating systems get revision number here
  if (rev == 0) {
    filp = fopen ("/proc/device-tree/system/linux,revision", "r");

    if (filp != NULL) {
      uint32_t tmp;
      if (fread (&tmp,1 , 4, filp) == 4) {
        // for some reason the value returned by reading this /proc entry seems to be big endian, convert it.
        rev = ntohl (tmp);
        rev &= 0xFFFFFF; // mask out warranty bit
        }
       }
    fclose (filp);
    }

  piCores = 0;
  pi_ispi = 0;
  rev &= 0xFFFFFF; // mask out warranty bit

  // Decode revision code
  if ((rev & 0x800000) == 0) {
    // old rev code
    if (rev < 0x0016) {
      // all BCM2835
      pi_ispi = 1;
      piCores = 1;
      pi_peri_phys = 0x20000000;
      pi_dram_bus = 0x40000000;
      pi_mem_flag = 0x0C;
      }
    else {
      DBG (DBG_ALWAYS, "unknown revision=%x", rev);
      rev = 0;
      }
    }

  else {
    // new rev code
    switch ((rev >> 12) & 0xF) { // just interested in BCM model
      //{{{
      case 0x0: // BCM2835
         pi_ispi = 1;
         piCores = 1;
         pi_peri_phys = 0x20000000;
         pi_dram_bus  = 0x40000000;
         pi_mem_flag  = 0x0C;
         break;
      //}}}
      case 0x1:      // BCM2836
      //{{{
      case 0x2: // BCM2837
         pi_ispi = 1;
         piCores = 4;
         pi_peri_phys = 0x3F000000;
         pi_dram_bus  = 0xC0000000;
         pi_mem_flag  = 0x04;
         break;
      //}}}
      //{{{
      case 0x3: // BCM2711
         pi_ispi = 1;
         piCores = 4;

         pi_peri_phys = 0xFE000000;
         pi_dram_bus  = 0xC0000000;
         pi_mem_flag  = 0x04;
         pi_is_2711   = 1;

         clk_osc_freq = CLK_OSC_FREQ_2711;
         clk_plld_freq = CLK_PLLD_FREQ_2711;

         hw_pwm_max_freq = PI_HW_PWM_MAX_FREQ_2711;
         hw_clk_min_freq = PI_HW_CLK_MIN_FREQ_2711;
         hw_clk_max_freq = PI_HW_CLK_MAX_FREQ_2711;
         break;
      //}}}
      //{{{
      default:
        DBG (DBG_ALWAYS, "unknown rev code (%x)", rev);
        rev=0;
        pi_ispi = 0;
        break;
      //}}}
      }
    }

  return rev;
  }
//}}}
//{{{
int gpioInitialise() {

  int status = initInitialise();
  if (status < 0)
    initReleaseResources();

  return status;
  }
//}}}
//{{{
void gpioTerminate() {

  gpioMaskSet = 0;

  // reset DMA
  if (dmaReg != MAP_FAILED) {
    initKillDMA (dmaIn);
    initKillDMA (dmaOut);
    }

  initReleaseResources();
  fflush (NULL);
  }
//}}}

//{{{  gpio
//{{{
static void myGpioSetMode (unsigned gpio, unsigned mode) {

  int reg =  gpio / 10;
  int shift = (gpio % 10) * 3;
  gpioReg[reg] = (gpioReg[reg] & ~(7 << shift)) | (mode << shift);
  }
//}}}

//{{{
static int myGpioRead (unsigned gpio) {

  if ((*(gpioReg + GPLEV0 + BANK) & BIT) != 0)
    return PI_ON;
  else
    return PI_OFF;
  }
//}}}

//{{{
static void myGpioWrite (unsigned gpio, unsigned level) {

  if (level == PI_OFF)
    *(gpioReg + GPCLR0 + BANK) = BIT;
  else
    *(gpioReg + GPSET0 + BANK) = BIT;
  }
//}}}
//{{{
static void mySetGpioOff (unsigned gpio, int pos) {

  int page, slot;
  myOffPageSlot (pos, &page, &slot);

  dmaIVirt[page]->gpioOff[slot] |= (1<<gpio);
  }
//}}}
//{{{
static void myClearGpioOff (unsigned gpio, int pos) {

  int page, slot;
  myOffPageSlot (pos, &page, &slot);
  dmaIVirt[page]->gpioOff[slot] &= ~(1<<gpio);
  }
//}}}
//{{{
static void mySetGpioOn (unsigned gpio, int pos) {

  int page = pos / ON_PER_IPAGE;
  int slot = pos % ON_PER_IPAGE;

  dmaIVirt[page]->gpioOn[slot] |= (1<<gpio);
  }
//}}}
//{{{
static void myClearGpioOn (unsigned gpio, int pos) {

  int page = pos / ON_PER_IPAGE;
  int slot = pos % ON_PER_IPAGE;
  dmaIVirt[page]->gpioOn[slot] &= ~(1<<gpio);
  }
//}}}

//{{{
static void myGpioSetPwm (unsigned gpio, int oldVal, int newVal) {

  int switchGpioOff = 0;

  int realRange = pwmRealRange[gpioInfo[gpio].freqIdx];
  int cycles = pwmCycles   [gpioInfo[gpio].freqIdx];
  int newOff = (newVal * realRange) / gpioInfo[gpio].range;
  int oldOff = (oldVal * realRange) / gpioInfo[gpio].range;
  int deferOff = gpioInfo[gpio].deferOff;
  int deferRng = gpioInfo[gpio].deferRng;

  if (gpioInfo[gpio].deferOff) {
    for (int i = 0; i < SUPERLEVEL; i += deferRng)
      myClearGpioOff (gpio, i + deferOff);
    gpioInfo[gpio].deferOff = 0;
    }

  if (newOff != oldOff) {
    if (newOff && oldOff)  {
      // PWM CHANGE
      if (newOff != realRange)
        for (int i = 0; i < SUPERLEVEL; i += realRange)
          mySetGpioOff (gpio, i+newOff);

      if (newOff > oldOff) {
        for (int i = 0; i < SUPERLEVEL; i += realRange)
          myClearGpioOff (gpio, i+oldOff);
        }
      else {
        gpioInfo[gpio].deferOff = oldOff;
        gpioInfo[gpio].deferRng = realRange;
        }
      }
    else if (newOff) {
      // PWM START
      if (newOff != realRange)
        for (int i = 0; i < SUPERLEVEL; i += realRange)
          mySetGpioOff (gpio, i+newOff);

      // schedule new gpio on
      for (int i = 0; i < SUPERCYCLE; i += cycles)
        mySetGpioOn (gpio, i);
      }
    else  {
      // PWM STOP deschedule gpio on
      for (int i = 0; i < SUPERCYCLE; i += cycles)
        myClearGpioOn (gpio, i);

      for (int i = 0; i < SUPERLEVEL; i += realRange)
        myClearGpioOff (gpio, i + oldOff);

     switchGpioOff = 1;
     }

    if (switchGpioOff) {
      *(gpioReg + GPCLR0) = (1 << gpio);
      *(gpioReg + GPCLR0) = (1 << gpio);
      }
    }
  }
//}}}
//{{{
static void myGpioSetServo (unsigned gpio, int oldVal, int newVal) {

   int newOff, oldOff, realRange, cycles, i;
   int deferOff, deferRng;

   DBG (DBG_INTERNAL, "myGpioSetServo %d from %d to %d", gpio, oldVal, newVal);

   realRange = pwmRealRange[clkCfg[gpioCfg.clockMicros].servoIdx];
   cycles    = pwmCycles   [clkCfg[gpioCfg.clockMicros].servoIdx];

   newOff = (newVal * realRange)/20000;
   oldOff = (oldVal * realRange)/20000;

   deferOff = gpioInfo[gpio].deferOff;
   deferRng = gpioInfo[gpio].deferRng;

   if (gpioInfo[gpio].deferOff)
   {
      for (i=0; i<SUPERLEVEL; i+=deferRng)
      {
         myClearGpioOff(gpio, i+deferOff);
      }
      gpioInfo[gpio].deferOff = 0;
   }

   if (newOff != oldOff)
   {
      if (newOff && oldOff)                       /* SERVO CHANGE */
      {
         for (i=0; i<SUPERLEVEL; i+=realRange)
            mySetGpioOff(gpio, i+newOff);

         if (newOff > oldOff)
         {
            for (i=0; i<SUPERLEVEL; i+=realRange)
               myClearGpioOff(gpio, i+oldOff);
         }
         else
         {
            gpioInfo[gpio].deferOff = oldOff;
            gpioInfo[gpio].deferRng = realRange;
         }
      }
      else if (newOff)                            /* SERVO START */
      {
         for (i=0; i<SUPERLEVEL; i+=realRange)
            mySetGpioOff(gpio, i+newOff);

         /* schedule new gpio on */

         for (i=0; i<SUPERCYCLE; i+=cycles) mySetGpioOn(gpio, i);
      }
      else                                        /* SERVO STOP */
      {
         /* deschedule gpio on */

         for (i=0; i<SUPERCYCLE; i+=cycles)
            myClearGpioOn(gpio, i);

         /* if in pulse then delay for the last cycle to complete */

         if (myGpioRead(gpio)) myGpioDelay(PI_MAX_SERVO_PULSEWIDTH);

         /* deschedule gpio off */

         for (i=0; i<SUPERLEVEL; i+=realRange)
            myClearGpioOff(gpio, i+oldOff);
      }
   }
}
//}}}
//{{{
static void switchFunctionOff (unsigned gpio) {

  switch (gpioInfo[gpio].is) {
    case GPIO_SERVO:
      // switch servo off
      myGpioSetServo(gpio, gpioInfo[gpio].width, 0);
      gpioInfo[gpio].width = 0;
      break;

    case GPIO_PWM:
      // switch pwm off
      myGpioSetPwm(gpio, gpioInfo[gpio].width, 0);
      gpioInfo[gpio].width = 0;
      break;

    case GPIO_HW_CLK:
      // No longer disable clock hardware, doing that was a bug
      gpioInfo[gpio].width = 0;
      break;

    case GPIO_HW_PWM:
      // No longer disable PWM hardware, doing that was a bug
      gpioInfo[gpio].width = 0;
      break;
    }
  }
//}}}

//{{{
int gpioSetMode (unsigned gpio, unsigned mode) {

  if (gpio > PI_MAX_GPIO)
    SOFT_ERROR(PI_BAD_GPIO, "bad gpio (%d)", gpio);

  if (mode > PI_ALT3)
    SOFT_ERROR(PI_BAD_MODE, "gpio %d, bad mode (%d)", gpio, mode);

  int reg   =  gpio/10;
  int shift = (gpio%10) * 3;

  int old_mode = (gpioReg[reg] >> shift) & 7;
  if (mode != old_mode) {
    switchFunctionOff(gpio);
    gpioInfo[gpio].is = GPIO_UNDEFINED;
    }

  gpioReg[reg] = (gpioReg[reg] & ~(7<<shift)) | (mode<<shift);

  return 0;
  }
//}}}
//{{{
int gpioGetMode (unsigned gpio) {

  if (gpio > PI_MAX_GPIO)
    SOFT_ERROR(PI_BAD_GPIO, "bad gpio (%d)", gpio);

  int reg =  gpio / 10;
  int shift = (gpio % 10) * 3;
  return (gpioReg[reg] >> shift) & 7;
  }
//}}}
//{{{
int gpioSetPullUpDown (unsigned gpio, unsigned pud) {

  int shift = (gpio & 0xf) << 1;

  if (gpio > PI_MAX_GPIO)
    SOFT_ERROR(PI_BAD_GPIO, "bad gpio (%d)", gpio);

  if (pud > PI_PUD_UP)
    SOFT_ERROR(PI_BAD_PUD, "gpio %d, bad pud (%d)", gpio, pud);

  if (pi_is_2711) {
    uint32_t pull;
    switch (pud) {
      case PI_PUD_OFF:  pull = 0; break;
      case PI_PUD_UP:   pull = 1; break;
      case PI_PUD_DOWN: pull = 2; break;
      }

    uint32_t bits = *(gpioReg + GPPUPPDN0 + (gpio>>4));
    bits &= ~(3 << shift);
    bits |= (pull << shift);
    *(gpioReg + GPPUPPDN0 + (gpio>>4)) = bits;
    }

  else {
    *(gpioReg + GPPUD) = pud;
    myGpioDelay (1);

    *(gpioReg + GPPUDCLK0 + BANK) = BIT;
    myGpioDelay (1);

    *(gpioReg + GPPUD) = 0;
    *(gpioReg + GPPUDCLK0 + BANK) = 0;
    }

  return 0;
  }
//}}}

//{{{
int gpioRead (unsigned gpio) {

  if (gpio > PI_MAX_GPIO)
    SOFT_ERROR(PI_BAD_GPIO, "bad gpio (%d)", gpio);

  if ((*(gpioReg + GPLEV0 + BANK) & BIT) != 0)
    return PI_ON;
  else
    return PI_OFF;
  }
//}}}
uint32_t gpioRead_Bits_0_31() { return (*(gpioReg + GPLEV0)); }
uint32_t gpioRead_Bits_32_53() { return (*(gpioReg + GPLEV1)); }

//{{{
int gpioWrite (unsigned gpio, unsigned level) {

  if (gpio > PI_MAX_GPIO)
    SOFT_ERROR(PI_BAD_GPIO, "bad gpio (%d)", gpio);

  if (level > PI_ON)
    SOFT_ERROR(PI_BAD_LEVEL, "gpio %d, bad level (%d)", gpio, level);

  if (gpio <= PI_MAX_GPIO) {
    if (gpioInfo[gpio].is != GPIO_WRITE) {
      // stop a glitch between setting mode then level
      if (level == PI_OFF)
        *(gpioReg + GPCLR0 + BANK) = BIT;
      else
        *(gpioReg + GPSET0 + BANK) = BIT;

      switchFunctionOff (gpio);
      gpioInfo[gpio].is = GPIO_WRITE;
      }
    }

  myGpioSetMode (gpio, PI_OUTPUT);

  if (level == PI_OFF)
    *(gpioReg + GPCLR0 + BANK) = BIT;
  else
    *(gpioReg + GPSET0 + BANK) = BIT;

  return 0;
  }
//}}}
//{{{
int gpioWrite_Bits_0_31_Clear (uint32_t bits) {

  *(gpioReg + GPCLR0) = bits;
  return 0;
  }
//}}}
//{{{
int gpioWrite_Bits_32_53_Clear (uint32_t bits) {

  *(gpioReg + GPCLR1) = bits;
  return 0;
  }
//}}}
//{{{
int gpioWrite_Bits_0_31_Set (uint32_t bits) {

  *(gpioReg + GPSET0) = bits;
  return 0;
  }
//}}}
//{{{
int gpioWrite_Bits_32_53_Set (uint32_t bits) {

  *(gpioReg + GPSET1) = bits;
  return 0;
  }
//}}}
void fastGpioWrite_Bits_0_31_Clear (uint32_t bits) { *(gpioReg + GPCLR0) = bits; }
void fastGpioWrite_Bits_0_31_Set (uint32_t bits) { *(gpioReg + GPSET0) = bits; }

// pwm
//{{{
int gpioPWM (unsigned gpio, unsigned val) {

  if (gpio > PI_MAX_USER_GPIO)
    SOFT_ERROR(PI_BAD_USER_GPIO, "bad gpio (%d)", gpio);

  if (val > gpioInfo[gpio].range)
    SOFT_ERROR(PI_BAD_DUTYCYCLE, "gpio %d, bad dutycycle (%d)", gpio, val);

   if (gpioInfo[gpio].is != GPIO_PWM) {
     switchFunctionOff(gpio);
     gpioInfo[gpio].is = GPIO_PWM;
     if (!val)
       myGpioWrite(gpio, 0);
    }

  myGpioSetMode (gpio, PI_OUTPUT);
  myGpioSetPwm (gpio, gpioInfo[gpio].width, val);

  gpioInfo[gpio].width = val;
  return 0;
  }
//}}}
//{{{
int gpioGetPWMdutycycle (unsigned gpio)
{
   unsigned pwm;

   if (gpio > PI_MAX_USER_GPIO)
      SOFT_ERROR(PI_BAD_USER_GPIO, "bad gpio (%d)", gpio);

   switch (gpioInfo[gpio].is)
   {
      case GPIO_PWM:
         return gpioInfo[gpio].width;

      case GPIO_HW_PWM:
         pwm = (PWMDef[gpio] >> 4) & 3;
         return hw_pwm_duty[pwm];

      case GPIO_HW_CLK:
         return PI_HW_PWM_RANGE/2;

      default:
         SOFT_ERROR(PI_NOT_PWM_GPIO, "not a PWM gpio (%d)", gpio);
   }
}
//}}}

//{{{
int gpioSetPWMrange (unsigned gpio, unsigned range) {

  if (gpio > PI_MAX_USER_GPIO)
    SOFT_ERROR(PI_BAD_USER_GPIO, "bad gpio (%d)", gpio);

  if ((range < PI_MIN_DUTYCYCLE_RANGE)  || (range > PI_MAX_DUTYCYCLE_RANGE))
    SOFT_ERROR(PI_BAD_DUTYRANGE, "gpio %d, bad range (%d)", gpio, range);

   int oldWidth = gpioInfo[gpio].width;
   if (oldWidth) {
     if (gpioInfo[gpio].is == GPIO_PWM) {
       int newWidth = (range * oldWidth) / gpioInfo[gpio].range;

       myGpioSetPwm (gpio, oldWidth, 0);
       gpioInfo[gpio].range = range;
       gpioInfo[gpio].width = newWidth;
       myGpioSetPwm(gpio, 0, newWidth);
      }
     }

   gpioInfo[gpio].range = range;

   /* return the actual range for the current gpio frequency */
   return pwmRealRange[gpioInfo[gpio].freqIdx];
  }
//}}}
//{{{
int gpioGetPWMrange (unsigned gpio) {

  if (gpio > PI_MAX_USER_GPIO)
    SOFT_ERROR(PI_BAD_USER_GPIO, "bad gpio (%d)", gpio);

  switch (gpioInfo[gpio].is) {
    case GPIO_HW_PWM:
    case GPIO_HW_CLK:
      return PI_HW_PWM_RANGE;

    default:
      return gpioInfo[gpio].range;
    }
  }
//}}}
//{{{
int gpioGetPWMrealRange (unsigned gpio)
{
   unsigned pwm;

   if (gpio > PI_MAX_USER_GPIO)
      SOFT_ERROR(PI_BAD_USER_GPIO, "bad gpio (%d)", gpio);

   switch (gpioInfo[gpio].is)
   {
      case GPIO_HW_PWM:
         pwm = (PWMDef[gpio] >> 4) & 3;
         return hw_pwm_real_range[pwm];

      case GPIO_HW_CLK:
         return PI_HW_PWM_RANGE;

      default:
         return pwmRealRange[gpioInfo[gpio].freqIdx];
   }
}
//}}}

//{{{
int gpioSetPWMfrequency (unsigned gpio, unsigned frequency)
{
   int i, width;
   unsigned diff, best, idx;

   if (gpio > PI_MAX_USER_GPIO)
      SOFT_ERROR(PI_BAD_USER_GPIO, "bad gpio (%d)", gpio);

   if      (frequency > pwmFreq[0])           idx = 0;
   else if (frequency < pwmFreq[PWM_FREQS-1]) idx = PWM_FREQS-1;
   else
   {
      best = 100000; /* impossibly high frequency difference */
      idx = 0;

      for (i=0; i<PWM_FREQS; i++)
      {
         if (frequency > pwmFreq[i]) diff = frequency - pwmFreq[i];
         else                        diff = pwmFreq[i] - frequency;

         if (diff < best)
         {
            best = diff;
            idx = i;
         }
      }
   }

   width = gpioInfo[gpio].width;

   if (width)
   {
      if (gpioInfo[gpio].is == GPIO_PWM)
      {
         myGpioSetPwm(gpio, width, 0);
         gpioInfo[gpio].freqIdx = idx;
         myGpioSetPwm(gpio, 0, width);
      }
   }

   gpioInfo[gpio].freqIdx = idx;

   return pwmFreq[idx];
}
//}}}
//{{{
int gpioGetPWMfrequency (unsigned gpio)
{
   unsigned pwm, clock;

   if (gpio > PI_MAX_USER_GPIO)
      SOFT_ERROR(PI_BAD_USER_GPIO, "bad gpio (%d)", gpio);

   switch (gpioInfo[gpio].is)
   {
      case GPIO_HW_PWM:
         pwm = (PWMDef[gpio] >> 4) & 3;
         return hw_pwm_freq[pwm];

      case GPIO_HW_CLK:
         clock = (clkDef[gpio] >> 4) & 3;
         return hw_clk_freq[clock];

      default:
         return pwmFreq[gpioInfo[gpio].freqIdx];
   }
}
//}}}

//{{{
int gpioServo (unsigned gpio, unsigned val)
{
   if (gpio > PI_MAX_USER_GPIO)
      SOFT_ERROR(PI_BAD_USER_GPIO, "bad gpio (%d)", gpio);

   if ((val!=PI_SERVO_OFF) && (val<PI_MIN_SERVO_PULSEWIDTH))
      SOFT_ERROR(PI_BAD_PULSEWIDTH,
         "gpio %d, bad pulsewidth (%d)", gpio, val);

   if (val>PI_MAX_SERVO_PULSEWIDTH)
      SOFT_ERROR(PI_BAD_PULSEWIDTH,
         "gpio %d, bad pulsewidth (%d)", gpio, val);

   if (gpioInfo[gpio].is != GPIO_SERVO)
   {
      switchFunctionOff(gpio);

      gpioInfo[gpio].is = GPIO_SERVO;

      if (!val) myGpioWrite(gpio, 0);
   }

   myGpioSetMode(gpio, PI_OUTPUT);

   myGpioSetServo(gpio, gpioInfo[gpio].width, val);

   gpioInfo[gpio].width=val;

   return 0;
}
//}}}
//{{{
int gpioGetServoPulsewidth (unsigned gpio)
{
   if (gpio > PI_MAX_USER_GPIO)
      SOFT_ERROR(PI_BAD_USER_GPIO, "bad gpio (%d)", gpio);

   if (gpioInfo[gpio].is != GPIO_SERVO)
      SOFT_ERROR(PI_NOT_SERVO_GPIO, "not a servo gpio (%d)", gpio);

   return gpioInfo[gpio].width;
}
//}}}

//{{{
int gpioSetPad (unsigned pad, unsigned padStrength)
{
   if (pad > PI_MAX_PAD)
      SOFT_ERROR(PI_BAD_PAD, "bad pad number (%d)", pad);

   if ((padStrength < PI_MIN_PAD_STRENGTH) ||
       (padStrength > PI_MAX_PAD_STRENGTH))
      SOFT_ERROR(PI_BAD_STRENGTH, "bad pad drive strength (%d)", pad);

   /* 1-16 -> 0-7 */

   padStrength += 1;
   padStrength /= 2;
   padStrength -= 1;

   padsReg[11+pad] = BCM_PASSWD | 0x18 | (padStrength & 7) ;

   return 0;
}
//}}}
//{{{
int gpioGetPad (unsigned pad)
{
   int strength;
   if (pad > PI_MAX_PAD)
      SOFT_ERROR(PI_BAD_PAD, "bad pad (%d)", pad);

   strength = padsReg[11+pad] & 7;

   strength *= 2;
   strength += 2;

   return strength;
}
//}}}
//}}}
//{{{  spi
// internal hw
//{{{
static int spiAnyOpen (uint32_t flags) {

  int aux = PI_SPI_FLAGS_GET_AUX_SPI (flags);

  for (int i = 0; i<PI_SPI_SLOTS; i++)
    if ((spiInfo[i].state == PI_SPI_OPENED) &&
        (PI_SPI_FLAGS_GET_AUX_SPI (spiInfo[i].flags) == aux))
      return 1;

  return 0;
  }
//}}}
//{{{
static void spiInit (uint32_t flags) {

  uint32_t resvd = PI_SPI_FLAGS_GET_RESVD (flags);
  uint32_t cspols = PI_SPI_FLAGS_GET_CSPOLS (flags);

  if (PI_SPI_FLAGS_GET_AUX_SPI(flags)) {
    // enable module and access to registers
    auxReg[AUX_ENABLES] |= AUXENB_SPI1;

    // save original state
    old_mode_ace0 = gpioGetMode(PI_ASPI_CE0);
    old_mode_ace1 = gpioGetMode(PI_ASPI_CE1);
    old_mode_ace2 = gpioGetMode(PI_ASPI_CE2);
    old_mode_asclk = gpioGetMode(PI_ASPI_SCLK);
    old_mode_amiso = gpioGetMode(PI_ASPI_MISO);
    old_mode_amosi = gpioGetMode(PI_ASPI_MOSI);

    old_spi_cntl0 = auxReg[AUX_SPI0_CNTL0_REG];
    old_spi_cntl1 = auxReg[AUX_SPI0_CNTL1_REG];

    // manually control auxiliary SPI chip selects
    if (!(resvd & 1)) {
      myGpioSetMode (PI_ASPI_CE0,  PI_OUTPUT);
      myGpioWrite (PI_ASPI_CE0, !(cspols&1));
      }

    if (!(resvd & 2)) {
      myGpioSetMode (PI_ASPI_CE1,  PI_OUTPUT);
      myGpioWrite (PI_ASPI_CE1, !(cspols&2));
      }

    if (!(resvd&4)) {
      myGpioSetMode (PI_ASPI_CE2,  PI_OUTPUT);
      myGpioWrite (PI_ASPI_CE2, !(cspols&4));
      }

    // set gpios to SPI mode
    myGpioSetMode (PI_ASPI_SCLK, PI_ALT4);
    myGpioSetMode (PI_ASPI_MISO, PI_ALT4);
    myGpioSetMode (PI_ASPI_MOSI, PI_ALT4);
    }
  else {
    // save original state
    old_mode_ce0 = gpioGetMode(PI_SPI_CE0);
    old_mode_ce1 = gpioGetMode(PI_SPI_CE1);
    old_mode_sclk = gpioGetMode(PI_SPI_SCLK);
    old_mode_miso = gpioGetMode(PI_SPI_MISO);
    old_mode_mosi = gpioGetMode(PI_SPI_MOSI);

    old_spi_cs = spiReg[SPI_CS];
    old_spi_clk = spiReg[SPI_CLK];

    // set gpios to SPI mode
    if (!(resvd & 1))
      myGpioSetMode (PI_SPI_CE0,  PI_ALT0);
    if (!(resvd & 2))
      myGpioSetMode (PI_SPI_CE1,  PI_ALT0);

    myGpioSetMode (PI_SPI_SCLK, PI_ALT0);
    myGpioSetMode (PI_SPI_MISO, PI_ALT0);
    myGpioSetMode (PI_SPI_MOSI, PI_ALT0);
    }
  }
//}}}

//{{{
static uint32_t _spiTXBits (char* buf, int pos, int bitlen, int msbf) {

  uint32_t bits = 0;
  if (buf) {
    if (bitlen <=  8)
      bits = *((( uint8_t*)buf)+pos);
    else if (bitlen <= 16)
      bits = *(((uint16_t*)buf)+pos);
    else
      bits = *(((uint32_t*)buf)+pos);

    if (msbf)
      bits <<= (32-bitlen);
    }

  return bits;
  }
//}}}
//{{{
static void _spiRXBits (char *buf, int pos, int bitlen, int msbf, uint32_t bits) {

  if (buf) {
    if (!msbf)
      bits >>= (32-bitlen);

    if (bitlen <=  8)
      *((( uint8_t*)buf)+pos) = bits;
    else if (bitlen <= 16)
      *(((uint16_t*)buf)+pos) = bits;
    else
      *(((uint32_t*)buf)+pos) = bits;
    }
  }
//}}}
//{{{
static void spiACS (int channel, int on) {

  int gpio;
  switch (channel) {
    case  0: gpio = PI_ASPI_CE0; break;
    case  1: gpio = PI_ASPI_CE1; break;
    default: gpio = PI_ASPI_CE2; break;
    }

  myGpioWrite (gpio, on);
  }
//}}}
//{{{
static void spiGoA (unsigned speed, uint32_t flags, char* txBuf, char* rxBuf, unsigned count) {

  char bit_ir[4] = {1, 0, 0, 1}; // read on rising edge
  char bit_or[4] = {0, 1, 1, 0}; // write on rising edge
  char bit_ic[4] = {0, 0, 1, 1}; // invert clock

  int channel = PI_SPI_FLAGS_GET_CHANNEL(flags);
  int mode = PI_SPI_FLAGS_GET_MODE (flags);
  int bitlen = PI_SPI_FLAGS_GET_BITLEN (flags);
  if (!bitlen)
    bitlen = 8;
  if (bitlen >  8)
    count /= 2;
  if (bitlen > 16)
    count /= 2;

  int txmsbf = !PI_SPI_FLAGS_GET_TX_LSB (flags);
  int rxmsbf = !PI_SPI_FLAGS_GET_RX_LSB (flags);
  int cs = PI_SPI_FLAGS_GET_CSPOLS(flags) & (1<<channel);

  uint32_t spiDefaults = AUXSPI_CNTL0_SPEED((125000000/speed)-1)|
                         AUXSPI_CNTL0_IN_RISING(bit_ir[mode])  |
                         AUXSPI_CNTL0_OUT_RISING(bit_or[mode]) |
                         AUXSPI_CNTL0_INVERT_CLK(bit_ic[mode]) |
                         AUXSPI_CNTL0_MSB_FIRST(txmsbf)        |
                         AUXSPI_CNTL0_SHIFT_LEN(bitlen);

  if (!count) {
    auxReg[AUX_SPI0_CNTL0_REG] = AUXSPI_CNTL0_ENABLE | AUXSPI_CNTL0_CLR_FIFOS;
    myGpioDelay (10);
    auxReg[AUX_SPI0_CNTL0_REG] = AUXSPI_CNTL0_ENABLE  | spiDefaults;
    auxReg[AUX_SPI0_CNTL1_REG] = AUXSPI_CNTL1_MSB_FIRST(rxmsbf);
    return;
    }

  auxReg[AUX_SPI0_CNTL0_REG] = AUXSPI_CNTL0_ENABLE  | spiDefaults;
  auxReg[AUX_SPI0_CNTL1_REG] = AUXSPI_CNTL1_MSB_FIRST(rxmsbf);
  spiACS(channel, cs);

  unsigned txCnt = 0;
  unsigned rxCnt = 0;
  while ((txCnt < count) || (rxCnt < count)) {
     uint32_t statusReg = auxReg[AUX_SPI0_STAT_REG];
     int rxEmpty = statusReg & AUXSPI_STAT_RX_EMPTY;
     int txFull = (((statusReg>>28)&15) > 2);
     if (rxCnt < count)
       if (!rxEmpty)
         _spiRXBits (rxBuf, rxCnt++, bitlen, rxmsbf, auxReg[AUX_SPI0_IO_REG]);

     if (txCnt < count) {
       if (!txFull) {
         if (txCnt != (count-1))
           auxReg[AUX_SPI0_TX_HOLD] = _spiTXBits(txBuf, txCnt++, bitlen, txmsbf);
         else
           auxReg[AUX_SPI0_IO_REG] = _spiTXBits(txBuf, txCnt++, bitlen, txmsbf);
         }
      }
    }

  while ((auxReg[AUX_SPI0_STAT_REG] & AUXSPI_STAT_BUSY)) {}
  spiACS (channel, !cs);
  }
//}}}
//{{{
static void spiGoS (unsigned speed, uint32_t flags, char* txBuf, char* rxBuf, unsigned count) {

  unsigned channel = PI_SPI_FLAGS_GET_CHANNEL (flags);
  unsigned mode   =  PI_SPI_FLAGS_GET_MODE (flags);
  unsigned cspols =  PI_SPI_FLAGS_GET_CSPOLS (flags);
  unsigned cspol  =  (cspols >> channel) & 1;
  unsigned flag3w =  PI_SPI_FLAGS_GET_3WIRE (flags);
  unsigned ren3w  =  PI_SPI_FLAGS_GET_3WREN (flags);

  uint32_t spiDefaults = SPI_CS_MODE(mode) | SPI_CS_CSPOLS(cspols) |
                         SPI_CS_CS(channel) | SPI_CS_CSPOL(cspol) | SPI_CS_CLEAR(3);

  // undocumented, stops inter-byte gap
  spiReg[SPI_DLEN] = 2;

  // stop
  spiReg[SPI_CS] = spiDefaults;

  if (!count)
    return;

  unsigned cnt4w;
  unsigned cnt3w;
  if (flag3w) {
    if (ren3w < count) {
      cnt4w = ren3w;
      cnt3w = count - ren3w;
      }
    else {
      cnt4w = count;
      cnt3w = 0;
      }
    }
  else {
    cnt4w = count;
    cnt3w = 0;
    }

  spiReg[SPI_CLK] = 250000000 / speed;
  spiReg[SPI_CS] = spiDefaults | SPI_CS_TA; /* start */

  unsigned cnt = cnt4w;
  unsigned txCnt = 0;
  unsigned rxCnt = 0;
  while ((txCnt < cnt) || (rxCnt < cnt)) {
    while ((rxCnt < cnt) && ((spiReg[SPI_CS] & SPI_CS_RXD))) {
      if (rxBuf)
        rxBuf[rxCnt] = spiReg[SPI_FIFO];
      else
        spi_dummy = spiReg[SPI_FIFO];
      rxCnt++;
      }

    while ((txCnt < cnt) && ((spiReg[SPI_CS] & SPI_CS_TXD))) {
      if (txBuf)
        spiReg[SPI_FIFO] = txBuf[txCnt];
      else
        spiReg[SPI_FIFO] = 0;
      txCnt++;
      }
    }

  while (!(spiReg[SPI_CS] & SPI_CS_DONE)) {}

  // switch to 3-wire bus
  cnt += cnt3w;
  spiReg[SPI_CS] |= SPI_CS_REN;
  while ((txCnt < cnt) || (rxCnt < cnt)) {
    while((rxCnt < cnt) && ((spiReg[SPI_CS] & SPI_CS_RXD))) {
      if (rxBuf)
        rxBuf[rxCnt] = spiReg[SPI_FIFO];
      else
        spi_dummy = spiReg[SPI_FIFO];
      rxCnt++;
      }

    while ((txCnt < cnt) && ((spiReg[SPI_CS] & SPI_CS_TXD))) {
      if (txBuf)
        spiReg[SPI_FIFO] = txBuf[txCnt];
      else
        spiReg[SPI_FIFO] = 0;
      txCnt++;
      }
    }

  while (!(spiReg[SPI_CS] & SPI_CS_DONE)) {}
  // stop
  spiReg[SPI_CS] = spiDefaults;
  }
//}}}
//{{{
static void spiGo (unsigned speed, uint32_t flags, char* txBuf, char* rxBuf, unsigned count) {

  if (PI_SPI_FLAGS_GET_AUX_SPI (flags))
    spiGoA (speed, flags, txBuf, rxBuf, count);
  else
    spiGoS (speed, flags, txBuf, rxBuf, count);
  }
//}}}

//{{{
static void spiTerminate (uint32_t flags) {

  int resvd = PI_SPI_FLAGS_GET_RESVD (flags);

  if (PI_SPI_FLAGS_GET_AUX_SPI (flags)) {
    // disable module and access to registers
    auxReg[AUX_ENABLES] &= (~AUXENB_SPI1);

    // restore original state
    if (!(resvd & 1)) myGpioSetMode (PI_ASPI_CE0, old_mode_ace0);
    if (!(resvd & 2)) myGpioSetMode (PI_ASPI_CE1, old_mode_ace1);
    if (!(resvd & 4)) myGpioSetMode (PI_ASPI_CE2, old_mode_ace2);

    myGpioSetMode (PI_ASPI_SCLK, old_mode_asclk);
    myGpioSetMode (PI_ASPI_MISO, old_mode_amiso);
    myGpioSetMode (PI_ASPI_MOSI, old_mode_amosi);

    auxReg[AUX_SPI0_CNTL0_REG] = old_spi_cntl0;
    auxReg[AUX_SPI0_CNTL1_REG] = old_spi_cntl1;
    }

  else {
    // restore original state
    if (!(resvd & 1))
      myGpioSetMode(PI_SPI_CE0, old_mode_ce0);
    if (!(resvd & 2))
      myGpioSetMode(PI_SPI_CE1, old_mode_ce1);

    myGpioSetMode (PI_SPI_SCLK, old_mode_sclk);
    myGpioSetMode (PI_SPI_MISO, old_mode_miso);
    myGpioSetMode (PI_SPI_MOSI, old_mode_mosi);

    spiReg[SPI_CS] = old_spi_cs;
    spiReg[SPI_CLK] = old_spi_clk;
    }
  }
//}}}

// external hw, main & aux
//{{{
int spiOpen (unsigned spiChan, unsigned baud, unsigned spiFlags) {

  static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  int i, slot;

  if (PI_SPI_FLAGS_GET_AUX_SPI(spiFlags)) {
    if (gpioHardwareRevision() < 16)
      SOFT_ERROR(PI_NO_AUX_SPI, "no auxiliary SPI on Pi A or B");

    i = PI_NUM_AUX_SPI_CHANNEL;
    }
  else
    i = PI_NUM_STD_SPI_CHANNEL;

  if (spiChan >= i)
    SOFT_ERROR(PI_BAD_SPI_CHANNEL, "bad spiChan (%d)", spiChan);

  if ((baud < PI_SPI_MIN_BAUD) || (baud > PI_SPI_MAX_BAUD))
    SOFT_ERROR(PI_BAD_SPI_SPEED, "bad baud (%d)", baud);

  if (spiFlags > (1<<22))
    SOFT_ERROR(PI_BAD_FLAGS, "bad spiFlags (0x%X)", spiFlags);

  if (!spiAnyOpen(spiFlags)) {
    // initialise on first open
    spiInit (spiFlags);
    spiGo (baud, spiFlags, NULL, NULL, 0);
    }

  slot = -1;

  pthread_mutex_lock(&mutex);

  for (i=0; i<PI_SPI_SLOTS; i++) {
    if (spiInfo[i].state == PI_SPI_CLOSED) {
      slot = i;
      spiInfo[slot].state = PI_SPI_RESERVED;
      break;
      }
    }

  pthread_mutex_unlock(&mutex);

  if (slot < 0)
    SOFT_ERROR(PI_NO_HANDLE, "no SPI handles");

  spiInfo[slot].speed = baud;
  spiInfo[slot].flags = spiFlags | PI_SPI_FLAGS_CHANNEL(spiChan);
  spiInfo[slot].state = PI_SPI_OPENED;

  return slot;
  }
//}}}
//{{{
int spiRead (unsigned handle, char* buf, unsigned count)
{
   if (handle >= PI_SPI_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (spiInfo[handle].state != PI_SPI_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (count > PI_MAX_SPI_DEVICE_COUNT)
      SOFT_ERROR(PI_BAD_SPI_COUNT, "bad count (%d)", count);

   spiGo (spiInfo[handle].speed, spiInfo[handle].flags, NULL, buf, count);

   return count;
}
//}}}
//{{{
int spiWrite (unsigned handle, char* buf, unsigned count) {

  if (PI_SPI_FLAGS_GET_AUX_SPI (spiInfo[handle].flags))
    spiGoA (spiInfo[handle].speed, spiInfo[handle].flags, buf, NULL, count);
  else
    spiGoS (spiInfo[handle].speed, spiInfo[handle].flags, buf, NULL, count);

  return count;
  }
//}}}
//{{{
void spiWriteMainFast (unsigned handle, const uint8_t* buf, unsigned count) {

  spiReg[SPI_CS] |= SPI_CS_TA;

  if (count == 1) {
    // single byte case
    spiReg[SPI_FIFO] = *buf++;
    while (!(spiReg[SPI_CS] & SPI_CS_RXD)) {}
    spiDummyRead = spiReg[SPI_FIFO];
    }

  else {
    // multiple bytes, byte swap words
    uint32_t txCount = 0;
    uint32_t rxCount = 0;
    while ((txCount < count) || (rxCount < count)) {
      while ((rxCount < count) && (spiReg[SPI_CS] & SPI_CS_RXD)) {
        spiDummyRead = spiReg[SPI_FIFO];
        rxCount++;
        }
      while ((txCount < count) && (spiReg[SPI_CS] & SPI_CS_TXD)) {
        if (txCount & 1) {
          spiReg[SPI_FIFO] = *buf;
          buf += 2;
          }
        else
          spiReg[SPI_FIFO] = *(buf+1);
        txCount++;
        }
      }
    }
  }
//}}}
//{{{
int spiXfer (unsigned handle, char* txBuf, char* rxBuf, unsigned count)
{
   if (handle >= PI_SPI_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (spiInfo[handle].state != PI_SPI_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (count > PI_MAX_SPI_DEVICE_COUNT)
      SOFT_ERROR(PI_BAD_SPI_COUNT, "bad count (%d)", count);

   spiGo(spiInfo[handle].speed, spiInfo[handle].flags, txBuf, rxBuf, count);

   return count;
}
//}}}
//{{{
int spiClose (unsigned handle) {

  if (handle >= PI_SPI_SLOTS)
    SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

  if (spiInfo[handle].state != PI_SPI_OPENED)
    SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

  spiInfo[handle].state = PI_SPI_CLOSED;

  if (!spiAnyOpen(spiInfo[handle].flags))
    spiTerminate (spiInfo[handle].flags); /* terminate on last close */

  return 0;
  }
//}}}

// internal bitbang
//{{{
static void set_CS (wfRx_t* w)
{
   myGpioWrite(w->S.CS, PI_SPI_FLAGS_GET_CSPOL(w->S.spiFlags));
}
//}}}
//{{{
static void clear_CS (wfRx_t* w)
{
   myGpioWrite(w->S.CS, !PI_SPI_FLAGS_GET_CSPOL(w->S.spiFlags));
}
//}}}
//{{{
static void set_SCLK (wfRx_t* w)
{
   myGpioWrite(w->S.SCLK, !PI_SPI_FLAGS_GET_CPOL(w->S.spiFlags));
}
//}}}
//{{{
static void clear_SCLK (wfRx_t* w)
{
   myGpioWrite(w->S.SCLK, PI_SPI_FLAGS_GET_CPOL(w->S.spiFlags));
}
//}}}
//{{{
static void SPI_delay (wfRx_t* w)
{
   myGpioDelay(w->S.delay);
}
//}}}
//{{{
static void bbSPIStart (wfRx_t* w)
{
   clear_SCLK(w);

   SPI_delay(w);

   set_CS(w);

   SPI_delay(w);
}
//}}}
//{{{
static void bbSPIStop (wfRx_t* w)
{
   SPI_delay(w);

   clear_CS(w);

   SPI_delay(w);

   clear_SCLK(w);
}
//}}}
//{{{
static uint8_t bbSPIXferByte (wfRx_t* w, char txByte)
{
   uint8_t bit, rxByte=0;

   if (PI_SPI_FLAGS_GET_CPHA(w->S.spiFlags))
   {
      /*
      CPHA = 1
      write on set clock
      read on clear clock
      */

      for (bit=0; bit<8; bit++)
      {
         set_SCLK(w);

         if (PI_SPI_FLAGS_GET_TX_LSB(w->S.spiFlags))
         {
            myGpioWrite(w->S.MOSI, txByte & 0x01);
            txByte >>= 1;
         }
         else
         {
            myGpioWrite(w->S.MOSI, txByte & 0x80);
            txByte <<= 1;
         }

         SPI_delay(w);

         clear_SCLK(w);

         if (PI_SPI_FLAGS_GET_RX_LSB(w->S.spiFlags))
         {
            rxByte = (rxByte >> 1) | myGpioRead(w->S.MISO) << 7;
         }
         else
         {
            rxByte = (rxByte << 1) | myGpioRead(w->S.MISO);
         }

         SPI_delay(w);
      }
   }
   else
   {
      /*
      CPHA = 0
      read on set clock
      write on clear clock
      */

      for (bit=0; bit<8; bit++)
      {
         if (PI_SPI_FLAGS_GET_TX_LSB(w->S.spiFlags))
         {
            myGpioWrite(w->S.MOSI, txByte & 0x01);
            txByte >>= 1;
         }
         else
         {
            myGpioWrite(w->S.MOSI, txByte & 0x80);
            txByte <<= 1;
         }

         SPI_delay(w);

         set_SCLK(w);

         if (PI_SPI_FLAGS_GET_RX_LSB(w->S.spiFlags))
         {
            rxByte = (rxByte >> 1) | myGpioRead(w->S.MISO) << 7;
         }
         else
         {
            rxByte = (rxByte << 1) | myGpioRead(w->S.MISO);
         }

         SPI_delay(w);

         clear_SCLK(w);
      }
   }

   return rxByte;
}
//}}}

// external bitBang
//{{{
int bbSPIOpen (unsigned CS, unsigned MISO, unsigned MOSI, unsigned SCLK, unsigned baud, unsigned spiFlags) {

  int valid;
  uint32_t bits;

  if (CS > PI_MAX_USER_GPIO)
    SOFT_ERROR(PI_BAD_USER_GPIO, "bad CS (%d)", CS);

  if (MISO > PI_MAX_USER_GPIO)
    SOFT_ERROR(PI_BAD_USER_GPIO, "bad MISO (%d)", MISO);

  if (MOSI > PI_MAX_USER_GPIO)
    SOFT_ERROR(PI_BAD_USER_GPIO, "bad MOSI (%d)", MOSI);

  if (SCLK > PI_MAX_USER_GPIO)
    SOFT_ERROR(PI_BAD_USER_GPIO, "bad SCLK (%d)", SCLK);

  if ((baud < PI_BB_SPI_MIN_BAUD) || (baud > PI_BB_SPI_MAX_BAUD))
    SOFT_ERROR(PI_BAD_SPI_BAUD, "CS %d, bad baud (%d)", CS, baud);

  if (wfRx[CS].mode != PI_WFRX_NONE)
    SOFT_ERROR(PI_GPIO_IN_USE, "CS %d is already being used, mode %d", CS, wfRx[CS].mode);

  valid = 0;

  /* check all GPIO unique */
  bits = (1<<CS) | (1<<MISO) | (1<<MOSI) | (1<<SCLK);

  if (__builtin_popcount(bits) == 4) {
    if ((wfRx[MISO].mode == PI_WFRX_NONE) &&
        (wfRx[MOSI].mode == PI_WFRX_NONE) &&
        (wfRx[SCLK].mode == PI_WFRX_NONE)) {
      valid = 1; /* first time GPIO used for SPI */
      }
    else {
      if ((wfRx[MISO].mode == PI_WFRX_SPI_MISO) &&
          (wfRx[MOSI].mode == PI_WFRX_SPI_MOSI) &&
          (wfRx[SCLK].mode == PI_WFRX_SPI_SCLK)) {
        valid = 2; /* new CS for existing SPI GPIO */
        }
      }
    }

  if (!valid) {
    SOFT_ERROR(PI_GPIO_IN_USE,
        "GPIO already being used (%d=%d %d=%d, %d=%d %d=%d)",
         CS,   wfRx[CS].mode,
         MISO, wfRx[MISO].mode,
         MOSI, wfRx[MOSI].mode,
         SCLK, wfRx[SCLK].mode);
    }

  wfRx[CS].mode = PI_WFRX_SPI_CS;
  wfRx[CS].baud = baud;

  wfRx[CS].S.CS = CS;
  wfRx[CS].S.SCLK = SCLK;

  wfRx[CS].S.CSMode = gpioGetMode(CS);
  wfRx[CS].S.delay = (500000 / baud) - 1;
  wfRx[CS].S.spiFlags = spiFlags;

  /* preset CS to off */
  if (PI_SPI_FLAGS_GET_CSPOL(spiFlags))
    gpioWrite(CS, 0); /* active high */
  else
    gpioWrite(CS, 1); /* active low */

  /* The SCLK entry is used to store full information */
  if (valid == 1) { /* first time GPIO for SPI */
    wfRx[SCLK].S.usage = 1;

    wfRx[SCLK].S.SCLKMode = gpioGetMode(SCLK);
    wfRx[SCLK].S.MISOMode = gpioGetMode(MISO);
    wfRx[SCLK].S.MOSIMode = gpioGetMode(MOSI);

    wfRx[SCLK].mode = PI_WFRX_SPI_SCLK;
    wfRx[MISO].mode = PI_WFRX_SPI_MISO;
    wfRx[MOSI].mode = PI_WFRX_SPI_MOSI;

    wfRx[SCLK].S.SCLK = SCLK;
    wfRx[SCLK].S.MISO = MISO;
    wfRx[SCLK].S.MOSI = MOSI;

    myGpioSetMode(MISO, PI_INPUT);
    myGpioSetMode(SCLK, PI_OUTPUT);
    gpioWrite(MOSI, 0); /* low output */
    }
  else
    wfRx[SCLK].S.usage++;

  return 0;
  }
//}}}
//{{{
int bbSPIXfer (unsigned CS, char* inBuf, char* outBuf, unsigned count) {

  if (CS > PI_MAX_USER_GPIO)
    SOFT_ERROR(PI_BAD_USER_GPIO, "bad gpio (%d)", CS);

  if (wfRx[CS].mode != PI_WFRX_SPI_CS)
    SOFT_ERROR(PI_NOT_SPI_GPIO, "no SPI on gpio (%d)", CS);

  if (!inBuf || !count)
    SOFT_ERROR(PI_BAD_POINTER, "input buffer can't be NULL");

  if (!outBuf && count)
    SOFT_ERROR(PI_BAD_POINTER, "output buffer can't be NULL");

  int SCLK = wfRx[CS].S.SCLK;
  wfRx[SCLK].S.CS = CS;
  wfRx[SCLK].baud = wfRx[CS].baud;
  wfRx[SCLK].S.delay = wfRx[CS].S.delay;
  wfRx[SCLK].S.spiFlags = wfRx[CS].S.spiFlags;

  wfRx_t* w = &wfRx[SCLK];

  wfRx_lock (SCLK);
  bbSPIStart (w);
  for (int pos = 0; pos < count; pos++)
    outBuf[pos] = bbSPIXferByte(w, inBuf[pos]);
  bbSPIStop(w);
  wfRx_unlock(SCLK);

  return count;
  }
//}}}
//{{{
int bbSPIClose (unsigned CS)
{
   int SCLK;
   if (CS > PI_MAX_USER_GPIO)
      SOFT_ERROR(PI_BAD_USER_GPIO, "bad gpio (%d)", CS);

   switch(wfRx[CS].mode)
   {
      case PI_WFRX_SPI_CS:

         myGpioSetMode(wfRx[CS].S.CS, wfRx[CS].S.CSMode);
         wfRx[CS].mode = PI_WFRX_NONE;

         SCLK = wfRx[CS].S.SCLK;

         if (--wfRx[SCLK].S.usage <= 0)
         {
            myGpioSetMode(wfRx[SCLK].S.MISO, wfRx[SCLK].S.MISOMode);
            myGpioSetMode(wfRx[SCLK].S.MOSI, wfRx[SCLK].S.MOSIMode);
            myGpioSetMode(wfRx[SCLK].S.SCLK, wfRx[SCLK].S.SCLKMode);

            wfRx[wfRx[SCLK].S.MISO].mode = PI_WFRX_NONE;
            wfRx[wfRx[SCLK].S.MOSI].mode = PI_WFRX_NONE;
            wfRx[wfRx[SCLK].S.SCLK].mode = PI_WFRX_NONE;
         }

         break;

      default:

         SOFT_ERROR(PI_NOT_SPI_GPIO, "no SPI on gpio (%d)", CS);

         break;

   }

   return 0;
}
//}}}
//}}}
//{{{  i2c
// external hw
//{{{
int i2cWriteQuick (unsigned handle, unsigned bit)
{
   int status;

   if (handle >= PI_I2C_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (i2cInfo[handle].state != PI_I2C_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if ((i2cInfo[handle].funcs & PI_I2C_FUNC_SMBUS_QUICK) == 0)
      SOFT_ERROR(PI_BAD_SMBUS_CMD, "SMBUS command not supported by driver");

   if (bit > 1)
      SOFT_ERROR(PI_BAD_PARAM, "bad bit (%d)", bit);

   status = my_smbus_access (i2cInfo[handle].fd, bit, 0, PI_I2C_SMBUS_QUICK, NULL);

   if (status < 0)
      return PI_I2C_WRITE_FAILED;

   return status;
}
//}}}

//{{{
int i2cReadByte (unsigned handle)
{
   union my_smbus_data data;
   int status;

   if (handle >= PI_I2C_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (i2cInfo[handle].state != PI_I2C_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if ((i2cInfo[handle].funcs & PI_I2C_FUNC_SMBUS_READ_BYTE) == 0)
      SOFT_ERROR(PI_BAD_SMBUS_CMD, "SMBUS command not supported by driver");

   status = my_smbus_access (i2cInfo[handle].fd, PI_I2C_SMBUS_READ, 0, PI_I2C_SMBUS_BYTE, &data);

   if (status < 0)
      return PI_I2C_READ_FAILED;

   return 0xFF & data.byte;
}
//}}}
//{{{
int i2cWriteByte (unsigned handle, unsigned bVal)
{
   int status;

   if (handle >= PI_I2C_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (i2cInfo[handle].state != PI_I2C_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if ((i2cInfo[handle].funcs & PI_I2C_FUNC_SMBUS_WRITE_BYTE) == 0)
      SOFT_ERROR(PI_BAD_SMBUS_CMD, "SMBUS command not supported by driver");

   if (bVal > 0xFF)
      SOFT_ERROR(PI_BAD_PARAM, "bad bVal (%d)", bVal);

   status = my_smbus_access (i2cInfo[handle].fd, PI_I2C_SMBUS_WRITE, bVal, PI_I2C_SMBUS_BYTE, NULL);

   if (status < 0)
      return PI_I2C_WRITE_FAILED;

   return status;
}
//}}}

//{{{
int i2cReadByteData (unsigned handle, unsigned reg)
{
   union my_smbus_data data;
   int status;

   if (handle >= PI_I2C_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (i2cInfo[handle].state != PI_I2C_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if ((i2cInfo[handle].funcs & PI_I2C_FUNC_SMBUS_READ_BYTE_DATA) == 0)
      SOFT_ERROR(PI_BAD_SMBUS_CMD, "SMBUS command not supported by driver");

   if (reg > 0xFF)
      SOFT_ERROR(PI_BAD_PARAM, "bad reg (%d)", reg);

   status = my_smbus_access(i2cInfo[handle].fd, PI_I2C_SMBUS_READ, reg, PI_I2C_SMBUS_BYTE_DATA, &data);

   if (status < 0)
      return PI_I2C_READ_FAILED;

   return 0xFF & data.byte;
}
//}}}
//{{{
int i2cWriteByteData (unsigned handle, unsigned reg, unsigned bVal)
{
   union my_smbus_data data;

   int status;

   if (handle >= PI_I2C_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (i2cInfo[handle].state != PI_I2C_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if ((i2cInfo[handle].funcs & PI_I2C_FUNC_SMBUS_WRITE_BYTE_DATA) == 0)
      SOFT_ERROR(PI_BAD_SMBUS_CMD, "SMBUS command not supported by driver");

   if (reg > 0xFF)
      SOFT_ERROR(PI_BAD_PARAM, "bad reg (%d)", reg);

   if (bVal > 0xFF)
      SOFT_ERROR(PI_BAD_PARAM, "bad bVal (%d)", bVal);

   data.byte = bVal;

   status = my_smbus_access( i2cInfo[handle].fd, PI_I2C_SMBUS_WRITE, reg, PI_I2C_SMBUS_BYTE_DATA, &data);

   if (status < 0)
      return PI_I2C_WRITE_FAILED;

   return status;
}
//}}}

//{{{
int i2cReadWordData (unsigned handle, unsigned reg)
{
   union my_smbus_data data;
   int status;

   if (handle >= PI_I2C_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (i2cInfo[handle].state != PI_I2C_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if ((i2cInfo[handle].funcs & PI_I2C_FUNC_SMBUS_READ_WORD_DATA) == 0)
      SOFT_ERROR(PI_BAD_SMBUS_CMD, "SMBUS command not supported by driver");

   if (reg > 0xFF)
      SOFT_ERROR(PI_BAD_PARAM, "bad reg (%d)", reg);

   status = (my_smbus_access( i2cInfo[handle].fd, PI_I2C_SMBUS_READ, reg, PI_I2C_SMBUS_WORD_DATA, &data));

   if (status < 0)
      return PI_I2C_READ_FAILED;

   return 0xFFFF & data.word;
}
//}}}
//{{{
int i2cWriteWordData (unsigned handle, unsigned reg, unsigned wVal)
{
   union my_smbus_data data;

   int status;

   if (handle >= PI_I2C_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (i2cInfo[handle].state != PI_I2C_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if ((i2cInfo[handle].funcs & PI_I2C_FUNC_SMBUS_WRITE_WORD_DATA) == 0)
      SOFT_ERROR(PI_BAD_SMBUS_CMD, "SMBUS command not supported by driver");

   if (reg > 0xFF)
      SOFT_ERROR(PI_BAD_PARAM, "bad reg (%d)", reg);

   if (wVal > 0xFFFF)
      SOFT_ERROR(PI_BAD_PARAM, "bad wVal (%d)", wVal);

   data.word = wVal;

   status = my_smbus_access( i2cInfo[handle].fd, PI_I2C_SMBUS_WRITE, reg, PI_I2C_SMBUS_WORD_DATA, &data);

   if (status < 0)
      return PI_I2C_WRITE_FAILED;

   return status;
}
//}}}

//{{{
int i2cProcessCall (unsigned handle, unsigned reg, unsigned wVal)
{
   union my_smbus_data data;
   int status;

   if (handle >= PI_I2C_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (i2cInfo[handle].state != PI_I2C_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if ((i2cInfo[handle].funcs & PI_I2C_FUNC_SMBUS_PROC_CALL) == 0)
      SOFT_ERROR(PI_BAD_SMBUS_CMD, "SMBUS command not supported by driver");

   if (reg > 0xFF)
      SOFT_ERROR(PI_BAD_PARAM, "bad reg (%d)", reg);

   if (wVal > 0xFFFF)
      SOFT_ERROR(PI_BAD_PARAM, "bad wVal (%d)", wVal);

   data.word = wVal;

   status = (my_smbus_access( i2cInfo[handle].fd, PI_I2C_SMBUS_WRITE, reg, PI_I2C_SMBUS_PROC_CALL, &data));

   if (status < 0)
      return PI_I2C_READ_FAILED;

   return 0xFFFF & data.word;
}
//}}}

//{{{
int i2cReadBlockData (unsigned handle, unsigned reg, char *buf)
{
   union my_smbus_data data;

   int i, status;

   if (handle >= PI_I2C_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (i2cInfo[handle].state != PI_I2C_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if ((i2cInfo[handle].funcs & PI_I2C_FUNC_SMBUS_READ_BLOCK_DATA) == 0)
      SOFT_ERROR(PI_BAD_SMBUS_CMD, "SMBUS command not supported by driver");

   if (reg > 0xFF)
      SOFT_ERROR(PI_BAD_PARAM, "bad reg (%d)", reg);

   status = (my_smbus_access( i2cInfo[handle].fd, PI_I2C_SMBUS_READ, reg, PI_I2C_SMBUS_BLOCK_DATA, &data));

   if (status < 0)
      return PI_I2C_READ_FAILED;
   else
   {
      if (data.block[0] <= PI_I2C_SMBUS_BLOCK_MAX)
      {
         for (i=0; i<data.block[0]; i++) buf[i] = data.block[i+1];
         return data.block[0];
      }
      else return PI_I2C_READ_FAILED;
   }
}
//}}}
//{{{
int i2cWriteBlockData (unsigned handle, unsigned reg, char *buf, unsigned count)
{
   union my_smbus_data data;

   int i, status;

   if (handle >= PI_I2C_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (i2cInfo[handle].state != PI_I2C_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if ((i2cInfo[handle].funcs & PI_I2C_FUNC_SMBUS_WRITE_BLOCK_DATA) == 0)
      SOFT_ERROR(PI_BAD_SMBUS_CMD, "SMBUS command not supported by driver");

   if (reg > 0xFF)
      SOFT_ERROR(PI_BAD_PARAM, "bad reg (%d)", reg);

   if ((count < 1) || (count > 32))
      SOFT_ERROR(PI_BAD_PARAM, "bad count (%d)", count);

   for (i=1; i<=count; i++) data.block[i] = buf[i-1];
   data.block[0] = count;

   status = my_smbus_access( i2cInfo[handle].fd, PI_I2C_SMBUS_WRITE, reg, PI_I2C_SMBUS_BLOCK_DATA, &data);

   if (status < 0)
      return PI_I2C_WRITE_FAILED;

   return status;
}
//}}}

//{{{
int i2cBlockProcessCall (unsigned handle, unsigned reg, char *buf, unsigned count)
{
   union my_smbus_data data;

   int i, status;

   if (handle >= PI_I2C_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (i2cInfo[handle].state != PI_I2C_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if ((i2cInfo[handle].funcs & PI_I2C_FUNC_SMBUS_PROC_CALL) == 0)
      SOFT_ERROR(PI_BAD_SMBUS_CMD, "SMBUS command not supported by driver");

   if (reg > 0xFF)
      SOFT_ERROR(PI_BAD_PARAM, "bad reg (%d)", reg);

   if ((count < 1) || (count > 32))
      SOFT_ERROR(PI_BAD_PARAM, "bad count (%d)", count);

   for (i=1; i<=count; i++) data.block[i] = buf[i-1];
   data.block[0] = count;

   status = (my_smbus_access( i2cInfo[handle].fd, PI_I2C_SMBUS_WRITE, reg, PI_I2C_SMBUS_BLOCK_PROC_CALL, &data));

   if (status < 0)
      return PI_I2C_READ_FAILED;
   else
   {
      if (data.block[0] <= PI_I2C_SMBUS_BLOCK_MAX)
      {
         for (i=0; i<data.block[0]; i++) buf[i] = data.block[i+1];
         return data.block[0];
      }
      else return PI_I2C_READ_FAILED;
   }
}
//}}}

//{{{
int i2cReadI2CBlockData (unsigned handle, unsigned reg, char *buf, unsigned count)
{
   union my_smbus_data data;

   int i, status;
   uint32_t size;

   if (handle >= PI_I2C_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (i2cInfo[handle].state != PI_I2C_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if ((i2cInfo[handle].funcs & PI_I2C_FUNC_SMBUS_READ_I2C_BLOCK) == 0)
      SOFT_ERROR(PI_BAD_SMBUS_CMD, "SMBUS command not supported by driver");

   if (reg > 0xFF)
      SOFT_ERROR(PI_BAD_PARAM, "bad reg (%d)", reg);

   if ((count < 1) || (count > 32))
      SOFT_ERROR(PI_BAD_PARAM, "bad count (%d)", count);

   if (count == 32)
      size = PI_I2C_SMBUS_I2C_BLOCK_BROKEN;
   else
      size = PI_I2C_SMBUS_I2C_BLOCK_DATA;

   data.block[0] = count;

   status = (my_smbus_access(i2cInfo[handle].fd, PI_I2C_SMBUS_READ, reg, size, &data));

   if (status < 0)
      return PI_I2C_READ_FAILED;
   else
   {
      if (data.block[0] <= PI_I2C_SMBUS_I2C_BLOCK_MAX)
      {
         for (i=0; i<data.block[0]; i++) buf[i] = data.block[i+1];
         return data.block[0];
      }
      else return PI_I2C_READ_FAILED;
   }
}
//}}}
//{{{
int i2cWriteI2CBlockData (unsigned handle, unsigned reg, char *buf, unsigned count)
{
   union my_smbus_data data;

   int i, status;

   if (handle >= PI_I2C_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (i2cInfo[handle].state != PI_I2C_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if ((i2cInfo[handle].funcs & PI_I2C_FUNC_SMBUS_WRITE_I2C_BLOCK) == 0)
      SOFT_ERROR(PI_BAD_SMBUS_CMD, "SMBUS command not supported by driver");

   if (reg > 0xFF)
      SOFT_ERROR(PI_BAD_PARAM, "bad reg (%d)", reg);

   if ((count < 1) || (count > 32))
      SOFT_ERROR(PI_BAD_PARAM, "bad count (%d)", count);

   for (i=1; i<=count; i++) data.block[i] = buf[i-1];

   data.block[0] = count;

   status = my_smbus_access( i2cInfo[handle].fd, PI_I2C_SMBUS_WRITE, reg, PI_I2C_SMBUS_I2C_BLOCK_BROKEN, &data);

   if (status < 0)
      return PI_I2C_WRITE_FAILED;

   return status;
}
//}}}

//{{{
int i2cWriteDevice (unsigned handle, char *buf, unsigned count)
{
   int bytes;

   if (handle >= PI_I2C_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (i2cInfo[handle].state != PI_I2C_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if ((count < 1) || (count > PI_MAX_I2C_DEVICE_COUNT))
      SOFT_ERROR(PI_BAD_PARAM, "bad count (%d)", count);

   bytes = write(i2cInfo[handle].fd, buf, count);

   if (bytes != count)
      return PI_I2C_WRITE_FAILED;

   return 0;
}
//}}}
//{{{
int i2cReadDevice (unsigned handle, char *buf, unsigned count)
{
   int bytes;

   if (handle >= PI_I2C_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (i2cInfo[handle].state != PI_I2C_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if ((count < 1) || (count > PI_MAX_I2C_DEVICE_COUNT))
      SOFT_ERROR(PI_BAD_PARAM, "bad count (%d)", count);

   bytes = read(i2cInfo[handle].fd, buf, count);

   if (bytes != count)
      return PI_I2C_READ_FAILED;

   return bytes;
}
//}}}

//{{{
int i2cOpen (unsigned i2cBus, unsigned i2cAddr, unsigned i2cFlags)
{
   static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
   char dev[32];
   int i, slot, fd;
   uint32_t funcs;

   if (i2cAddr > PI_MAX_I2C_ADDR)
      SOFT_ERROR(PI_BAD_I2C_ADDR, "bad I2C address (%d)", i2cAddr);

   if (i2cFlags)
      SOFT_ERROR(PI_BAD_FLAGS, "bad flags (0x%X)", i2cFlags);

   slot = -1;

   pthread_mutex_lock(&mutex);

   for (i=0; i<PI_I2C_SLOTS; i++)
   {
      if (i2cInfo[i].state == PI_I2C_CLOSED)
      {
         slot = i;
         i2cInfo[slot].state = PI_I2C_RESERVED;
         break;
      }
   }

   pthread_mutex_unlock(&mutex);

   if (slot < 0) SOFT_ERROR(PI_NO_HANDLE, "no I2C handles");

   sprintf(dev, "/dev/i2c-%d", i2cBus);

   if ((fd = open(dev, O_RDWR)) < 0)
   {
      /* try a modprobe */

      if (system("/sbin/modprobe i2c_dev") == -1) { /* ignore errors */}
      if (system("/sbin/modprobe i2c_bcm2835") == -1) { /* ignore errors */}

      myGpioDelay(100000);

      if ((fd = open(dev, O_RDWR)) < 0)
      {
         i2cInfo[slot].state = PI_I2C_CLOSED;
         return PI_BAD_I2C_BUS;
      }
   }

   if (ioctl(fd, PI_I2C_SLAVE, i2cAddr) < 0)
   {
      close(fd);
      i2cInfo[slot].state = PI_I2C_CLOSED;
      return PI_I2C_OPEN_FAILED;
   }

   if (ioctl(fd, PI_I2C_FUNCS, &funcs) < 0)
   {
      funcs = -1; /* assume all smbus commands allowed */
   }

   i2cInfo[slot].fd = fd;
   i2cInfo[slot].addr = i2cAddr;
   i2cInfo[slot].flags = i2cFlags;
   i2cInfo[slot].funcs = funcs;
   i2cInfo[i].state = PI_I2C_OPENED;

   return slot;
}
//}}}
//{{{
int i2cClose (unsigned handle)
{
   if (handle >= PI_I2C_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (i2cInfo[handle].state != PI_I2C_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (i2cInfo[handle].fd >= 0) close(i2cInfo[handle].fd);

   i2cInfo[handle].fd = -1;
   i2cInfo[handle].state = PI_I2C_CLOSED;

   return 0;
}
//}}}

//{{{
void i2cSwitchCombined (int setting)
{
   int fd;

   fd = open(PI_I2C_COMBINED, O_WRONLY);

   if (fd >= 0)
   {
      if (setting)
      {
         if (write(fd, "1\n", 2) == -1) { /* ignore errors */ }
      }
      else
      {
         if (write(fd, "0\n", 2) == -1) { /* ignore errors */ }
      }

      close(fd);
   }
}
//}}}
//{{{
int i2cSegments (unsigned handle, pi_i2c_msg_t *segs, unsigned numSegs)
{
   int retval;
   my_i2c_rdwr_ioctl_data_t rdwr;

   if (handle >= PI_I2C_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (i2cInfo[handle].state != PI_I2C_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (segs == NULL)
      SOFT_ERROR(PI_BAD_POINTER, "null segments");

   if (numSegs > PI_I2C_RDRW_IOCTL_MAX_MSGS)
      SOFT_ERROR(PI_TOO_MANY_SEGS, "too many segments (%d)", numSegs);

   rdwr.msgs = segs;
   rdwr.nmsgs = numSegs;

   retval = ioctl(i2cInfo[handle].fd, PI_I2C_RDWR, &rdwr);

   if (retval >= 0) return retval;
   else             return PI_BAD_I2C_SEG;
}
//}}}
//{{{
int i2cZip (unsigned handle, char *inBuf, unsigned inLen, char *outBuf, unsigned outLen)
{
   int numSegs, inPos, outPos, status, bytes, flags, addr;
   int esc, setesc;
   pi_i2c_msg_t segs[PI_I2C_RDRW_IOCTL_MAX_MSGS];

   if (handle >= PI_I2C_SLOTS)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (i2cInfo[handle].state != PI_I2C_OPENED)
      SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

   if (!inBuf || !inLen)
      SOFT_ERROR(PI_BAD_POINTER, "input buffer can't be NULL");

   if (!outBuf && outLen)
      SOFT_ERROR(PI_BAD_POINTER, "output buffer can't be NULL");

   numSegs = 0;

   inPos = 0;
   outPos = 0;
   status = 0;

   addr = i2cInfo[handle].addr;
   flags = 0;
   esc = 0;
   setesc = 0;

   while (!status && (inPos < inLen)) {
      DBG (DBG_INTERNAL, "status=%d inpos=%d inlen=%d cmd=%d addr=%d flags=%x",
         status, inPos, inLen, inBuf[inPos], addr, flags);

      switch (inBuf[inPos++])
      {
         case PI_I2C_END:
            status = 1;
            break;

         case PI_I2C_COMBINED_ON:
            /* Run prior transactions before setting combined flag */
            if (numSegs)
            {
               status = i2cSegments(handle, segs, numSegs);
               if (status >= 0) status = 0; /* continue */
               numSegs = 0;
            }
            i2cSwitchCombined(1);
            break;

         case PI_I2C_COMBINED_OFF:
            /* Run prior transactions before clearing combined flag */
            if (numSegs)
            {
               status = i2cSegments(handle, segs, numSegs);
               if (status >= 0) status = 0; /* continue */
               numSegs = 0;
            }
            i2cSwitchCombined(0);
            break;

         case PI_I2C_ADDR:
            addr = myI2CGetPar(inBuf, &inPos, inLen, &esc);
            if (addr < 0) status = PI_BAD_I2C_CMD;
            break;

         case PI_I2C_FLAGS:
            /* cheat to force two byte flags */
            esc = 1;
            flags = myI2CGetPar(inBuf, &inPos, inLen, &esc);
            if (flags < 0) status = PI_BAD_I2C_CMD;
            break;

         case PI_I2C_ESC:
            setesc = 1;
            break;

         case PI_I2C_READ:

            bytes = myI2CGetPar(inBuf, &inPos, inLen, &esc);

            if (bytes >= 0)
            {
               if ((bytes + outPos) < outLen)
               {
                  segs[numSegs].addr = addr;
                  segs[numSegs].flags = (flags|1);
                  segs[numSegs].len = bytes;
                  segs[numSegs].buf = (uint8_t *)(outBuf + outPos);
                  outPos += bytes;
                  numSegs++;
                  if (numSegs >= PI_I2C_RDRW_IOCTL_MAX_MSGS)
                  {
                     status = i2cSegments(handle, segs, numSegs);
                     if (status >= 0) status = 0; /* continue */
                     numSegs = 0;
                  }
               }
               else status = PI_BAD_I2C_RLEN;
            }
            else status = PI_BAD_I2C_RLEN;
            break;

         case PI_I2C_WRITE:

            bytes = myI2CGetPar(inBuf, &inPos, inLen, &esc);

            if (bytes >= 0)
            {
               if ((bytes + inPos) < inLen)
               {
                  segs[numSegs].addr = addr;
                  segs[numSegs].flags = (flags&0xfffe);
                  segs[numSegs].len = bytes;
                  segs[numSegs].buf = (uint8_t *)(inBuf + inPos);
                  inPos += bytes;
                  numSegs++;
                  if (numSegs >= PI_I2C_RDRW_IOCTL_MAX_MSGS)
                  {
                     status = i2cSegments(handle, segs, numSegs);
                     if (status >= 0) status = 0; /* continue */
                     numSegs = 0;
                  }
               }
               else status = PI_BAD_I2C_WLEN;
            }
            else status = PI_BAD_I2C_WLEN;
            break;

         default:
            status = PI_BAD_I2C_CMD;
      }

      if (setesc) esc = 1; else esc = 0;

      setesc = 0;
   }

   if (status >= 0)
   {
      if (numSegs) status = i2cSegments(handle, segs, numSegs);
   }

   if (status >= 0) status = outPos;

   return status;
}
//}}}

// internal bitBang
//{{{
static int read_SDA (wfRx_t* w)
{
   myGpioSetMode(w->I.SDA, PI_INPUT);
   return myGpioRead(w->I.SDA);
}
//}}}
//{{{
static void set_SDA (wfRx_t* w)
{
   myGpioSetMode(w->I.SDA, PI_INPUT);
}
//}}}
//{{{
static void clear_SDA (wfRx_t* w)
{
   myGpioSetMode(w->I.SDA, PI_OUTPUT);
   myGpioWrite(w->I.SDA, 0);
}
//}}}
//{{{
static void clear_SCL (wfRx_t* w)
{
   myGpioSetMode(w->I.SCL, PI_OUTPUT);
   myGpioWrite(w->I.SCL, 0);
}
//}}}

//{{{
static void I2C_delay (wfRx_t* w)
{
   myGpioDelay(w->I.delay);
}
//}}}
//{{{
static void I2C_clock_stretch (wfRx_t* w)
{
   uint32_t now, max_stretch=100000;

   myGpioSetMode(w->I.SCL, PI_INPUT);
   now = gpioTick();
   while ((myGpioRead(w->I.SCL) == 0) && ((gpioTick()-now) < max_stretch));
}
//}}}
//{{{
static void I2CStart (wfRx_t* w)
{
   if (w->I.started)
   {
      set_SDA(w);
      I2C_delay(w);
      I2C_clock_stretch(w);
      I2C_delay(w);
   }

   clear_SDA(w);
   I2C_delay(w);
   clear_SCL(w);
   I2C_delay(w);

   w->I.started = 1;
}
//}}}
//{{{
static void I2CStop( wfRx_t* w)
{
   clear_SDA(w);
   I2C_delay(w);
   I2C_clock_stretch(w);
   I2C_delay(w);
   set_SDA(w);
   I2C_delay(w);

   w->I.started = 0;
}
//}}}
//{{{
static void I2CPutBit (wfRx_t* w, int bit)
{
   if (bit) set_SDA(w);
   else     clear_SDA(w);

   I2C_delay(w);
   I2C_clock_stretch(w);
   I2C_delay(w);
   clear_SCL(w);
}
//}}}
//{{{
static int I2CGetBit (wfRx_t* w)
{
   int bit;

   set_SDA(w); /* let SDA float */
   I2C_delay(w);
   I2C_clock_stretch(w);
   bit = read_SDA(w);
   I2C_delay(w);
   clear_SCL(w);

   return bit;
}
//}}}
//{{{
static int I2CPutByte (wfRx_t* w, int byte)
{
   int bit, nack;

   for(bit=0; bit<8; bit++)
   {
      I2CPutBit(w, byte & 0x80);
      byte <<= 1;
   }

   nack = I2CGetBit(w);

   return nack;
}
//}}}
//{{{
static uint8_t I2CGetByte (wfRx_t* w, int nack)
{
   int bit, byte=0;

   for (bit=0; bit<8; bit++)
   {
      byte = (byte << 1) | I2CGetBit(w);
   }

   I2CPutBit(w, nack);

   return byte;
}
//}}}

// external bitBang
//{{{
int bbI2COpen (unsigned SDA, unsigned SCL, unsigned baud)
{
   if (SDA > PI_MAX_USER_GPIO)
      SOFT_ERROR(PI_BAD_USER_GPIO, "bad SDA (%d)", SDA);

   if (SCL > PI_MAX_USER_GPIO)
      SOFT_ERROR(PI_BAD_USER_GPIO, "bad SCL (%d)", SCL);

   if ((baud < PI_BB_I2C_MIN_BAUD) || (baud > PI_BB_I2C_MAX_BAUD))
      SOFT_ERROR(PI_BAD_I2C_BAUD,
         "SDA %d, bad baud rate (%d)", SDA, baud);

   if (wfRx[SDA].mode != PI_WFRX_NONE)
      SOFT_ERROR(PI_GPIO_IN_USE, "gpio %d is already being used", SDA);

   if ((wfRx[SCL].mode != PI_WFRX_NONE)  || (SCL == SDA))
      SOFT_ERROR(PI_GPIO_IN_USE, "gpio %d is already being used", SCL);

   wfRx[SDA].gpio = SDA;
   wfRx[SDA].mode = PI_WFRX_I2C_SDA;
   wfRx[SDA].baud = baud;

   wfRx[SDA].I.started = 0;
   wfRx[SDA].I.SDA = SDA;
   wfRx[SDA].I.SCL = SCL;
   wfRx[SDA].I.delay = 500000 / baud;
   wfRx[SDA].I.SDAMode = gpioGetMode(SDA);
   wfRx[SDA].I.SCLMode = gpioGetMode(SCL);

   wfRx[SCL].gpio = SCL;
   wfRx[SCL].mode = PI_WFRX_I2C_SCL;

   myGpioSetMode(SDA, PI_INPUT);
   myGpioSetMode(SCL, PI_INPUT);

   return 0;
}
//}}}
//{{{
int bbI2CClose (unsigned SDA)
{
   if (SDA > PI_MAX_USER_GPIO)
      SOFT_ERROR(PI_BAD_USER_GPIO, "bad gpio (%d)", SDA);

   switch(wfRx[SDA].mode)
   {
      case PI_WFRX_I2C_SDA:

         myGpioSetMode(wfRx[SDA].I.SDA, wfRx[SDA].I.SDAMode);
         myGpioSetMode(wfRx[SDA].I.SCL, wfRx[SDA].I.SCLMode);

         wfRx[wfRx[SDA].I.SDA].mode = PI_WFRX_NONE;
         wfRx[wfRx[SDA].I.SCL].mode = PI_WFRX_NONE;

         break;

      default:

         SOFT_ERROR(PI_NOT_I2C_GPIO, "no I2C on gpio (%d)", SDA);

         break;

   }

   return 0;
}
//}}}
//{{{
int bbI2CZip (unsigned SDA, char *inBuf, unsigned inLen, char *outBuf, unsigned outLen)
{
   int i, ack, inPos, outPos, status, bytes;
   int addr, flags, esc, setesc;
   wfRx_t *w;

   if (SDA > PI_MAX_USER_GPIO)
      SOFT_ERROR(PI_BAD_USER_GPIO, "bad gpio (%d)", SDA);

   if (wfRx[SDA].mode != PI_WFRX_I2C_SDA)
      SOFT_ERROR(PI_NOT_I2C_GPIO, "no I2C on gpio (%d)", SDA);

   if (!inBuf || !inLen)
      SOFT_ERROR(PI_BAD_POINTER, "input buffer can't be NULL");

   if (!outBuf && outLen)
      SOFT_ERROR(PI_BAD_POINTER, "output buffer can't be NULL");

   w = &wfRx[SDA];

   inPos = 0;
   outPos = 0;
   status = 0;

   addr = 0;
   flags = 0;
   esc = 0;
   setesc = 0;

   wfRx_lock(SDA);

   while (!status && (inPos < inLen)) {
      DBG (DBG_INTERNAL, "status=%d inpos=%d inlen=%d cmd=%d addr=%d flags=%x",
         status, inPos, inLen, inBuf[inPos], addr, flags);

      switch (inBuf[inPos++]) {
         case PI_I2C_END:
            status = 1;
            break;

         case PI_I2C_START:
            I2CStart(w);
            break;

         case PI_I2C_STOP:
            I2CStop(w);
            break;

         case PI_I2C_ADDR:
            addr = myI2CGetPar(inBuf, &inPos, inLen, &esc);
            if (addr < 0) status = PI_BAD_I2C_CMD;
            break;

         case PI_I2C_FLAGS:
            /* cheat to force two byte flags */
            esc = 1;
            flags = myI2CGetPar(inBuf, &inPos, inLen, &esc);
            if (flags < 0) status = PI_BAD_I2C_CMD;
            break;

         case PI_I2C_ESC:
            setesc = 1;
            break;

         case PI_I2C_READ:

            bytes = myI2CGetPar(inBuf, &inPos, inLen, &esc);

            if (bytes >= 0) ack = I2CPutByte(w, (addr<<1)|1);

            if (bytes > 0)
            {
               if (!ack)
               {
                  if ((bytes + outPos) <= outLen)
                  {
                     for (i=0; i<(bytes-1); i++)
                     {
                        outBuf[outPos++] = I2CGetByte(w, 0);
                     }
                     outBuf[outPos++] = I2CGetByte(w, 1);
                  }
                  else status = PI_BAD_I2C_RLEN;
               }
               else status = PI_I2C_READ_FAILED;
            }
            else status = PI_BAD_I2C_CMD;
            break;

         case PI_I2C_WRITE:

            bytes = myI2CGetPar(inBuf, &inPos, inLen, &esc);

            if (bytes >= 0) ack = I2CPutByte(w, addr<<1);

            if (bytes > 0)
            {
               if (!ack)
               {
                  if ((bytes + inPos) <= inLen)
                  {
                     for (i=0; i<(bytes-1); i++)
                     {
                        ack = I2CPutByte(w, inBuf[inPos++]);
                        if (ack) status = PI_I2C_WRITE_FAILED;
                     }
                     ack = I2CPutByte(w, inBuf[inPos++]);
                  }
                  else status = PI_BAD_I2C_WLEN;
               } else status = PI_I2C_WRITE_FAILED;
            }
            else status = PI_BAD_I2C_CMD;
            break;

         default:
            status = PI_BAD_I2C_CMD;
      }

      if (setesc) esc = 1; else esc = 0;

      setesc = 0;
   }

   wfRx_unlock(SDA);

   if (status >= 0) status = outPos;

   return status;
}
//}}}

//{{{
void bscInit (int mode)
{
   int sda, scl, miso, ce;

   bscsReg[BSC_CR]=0; /* clear device */
   bscsReg[BSC_RSR]=0; /* clear underrun and overrun errors */
   bscsReg[BSC_SLV]=0; /* clear I2C slave address */
   bscsReg[BSC_IMSC]=0xf; /* mask off all interrupts */
   bscsReg[BSC_ICR]=0x0f; /* clear all interrupts */

   if (pi_is_2711)
   {
      sda = BSC_SDA_MOSI_2711;
      scl = BSC_SCL_SCLK_2711;
      miso = BSC_MISO_2711;
      ce = BSC_CE_N_2711;
   }
   else
   {
      sda = BSC_SDA_MOSI;
      scl = BSC_SCL_SCLK;
      miso = BSC_MISO;
      ce = BSC_CE_N;
   }

   gpioSetMode(sda, PI_ALT3);
   gpioSetMode(scl, PI_ALT3);

   if (mode > 1) /* SPI uses all GPIO */
   {
      gpioSetMode(miso, PI_ALT3);
      gpioSetMode(ce, PI_ALT3);
   }
}
//}}}
//{{{
void bscTerm (int mode)
{
   int sda, scl, miso, ce;

   bscsReg[BSC_CR] = 0; /* clear device */
   bscsReg[BSC_RSR]=0; /* clear underrun and overrun errors */
   bscsReg[BSC_SLV]=0; /* clear I2C slave address */

   if (pi_is_2711)
   {
      sda = BSC_SDA_MOSI_2711;
      scl = BSC_SCL_SCLK_2711;
      miso = BSC_MISO_2711;
      ce = BSC_CE_N_2711;
   }
   else
   {
      sda = BSC_SDA_MOSI;
      scl = BSC_SCL_SCLK;
      miso = BSC_MISO;
      ce = BSC_CE_N;
   }

   gpioSetMode(sda, PI_INPUT);
   gpioSetMode(scl, PI_INPUT);

   if (mode > 1)
   {
      gpioSetMode(miso, PI_INPUT);
      gpioSetMode(ce, PI_INPUT);
   }
}
//}}}
//{{{
int bscXfer (bsc_xfer_t* xfer)
{
   static int bscMode = 0;

   int copied=0;
   int active, mode;

   if (xfer->control)
   {
      /*
         bscMode (0=None, 1=I2C, 2=SPI) tracks which GPIO have been
         set to BSC mode
      */
      if (xfer->control & 2) mode = 2; /* SPI */
      else                   mode = 1; /* assume I2C */

      if (mode > bscMode)
      {
         bscInit(mode);
         bscMode = mode;
      }
   }
   else
   {
      if (bscMode) bscTerm(bscMode);
      bscMode = 0;
      return 0; /* leave ignore set */
   }

   xfer->rxCnt = 0;

   bscsReg[BSC_SLV] = ((xfer->control)>>16) & 127;
   bscsReg[BSC_CR] = (xfer->control) & 0x3fff;
   bscsReg[BSC_RSR]=0; /* clear underrun and overrun errors */

   active = 1;

   while (active)
   {
      active = 0;

      while ((copied < xfer->txCnt) &&
             (!(bscsReg[BSC_FR] & BSC_FR_TXFF)))
      {
         bscsReg[BSC_DR] = xfer->txBuf[copied++];
         active = 1;
      }

      while ((xfer->rxCnt < BSC_FIFO_SIZE) &&
             (!(bscsReg[BSC_FR] & BSC_FR_RXFE)))
      {
         xfer->rxBuf[xfer->rxCnt++] = bscsReg[BSC_DR];
         active = 1;
      }

      if (!active)
      {
         active = bscsReg[BSC_FR] & (BSC_FR_RXBUSY | BSC_FR_TXBUSY);
      }

      if (active)
        myGpioSleep(0, 20);
   }

   bscFR = bscsReg[BSC_FR] & 0xffff;

   return (copied<<16) | bscFR;
}
//}}}
//}}}
//{{{  serial
// hw
//{{{
int serOpen (char* tty, unsigned serBaud, unsigned serFlags) {

  static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  struct termios new;
  int speed;
  int fd;
  int i, slot;

  if (strncmp ("/dev/tty", tty, 8) && strncmp ("/dev/serial", tty, 11))
    SOFT_ERROR(PI_BAD_SER_DEVICE, "bad device (%s)", tty);

  switch (serBaud) {
    case     50: speed =     B50; break;
    case     75: speed =     B75; break;
    case    110: speed =    B110; break;
    case    134: speed =    B134; break;
    case    150: speed =    B150; break;
    case    200: speed =    B200; break;
    case    300: speed =    B300; break;
    case    600: speed =    B600; break;
    case   1200: speed =   B1200; break;
    case   1800: speed =   B1800; break;
    case   2400: speed =   B2400; break;
    case   4800: speed =   B4800; break;
    case   9600: speed =   B9600; break;
    case  19200: speed =  B19200; break;
    case  38400: speed =  B38400; break;
    case  57600: speed =  B57600; break;
    case 115200: speed = B115200; break;
    case 230400: speed = B230400; break;
    default:
      SOFT_ERROR(PI_BAD_SER_SPEED, "bad speed (%d)", serBaud);
    }

  if (serFlags)
   SOFT_ERROR(PI_BAD_FLAGS, "bad flags (0x%X)", serFlags);

  slot = -1;
  pthread_mutex_lock (&mutex);
  for (i = 0; i < PI_SER_SLOTS; i++) {
    if (serInfo[i].state == PI_SER_CLOSED) {
      slot = i;
      serInfo[slot].state = PI_SER_RESERVED;
      break;
      }
    }
  pthread_mutex_unlock(&mutex);

  if (slot < 0)
    SOFT_ERROR(PI_NO_HANDLE, "no serial handles");

  if ((fd = open(tty, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK)) == -1) {
    serInfo[slot].state = PI_SER_CLOSED;
    return PI_SER_OPEN_FAILED;
    }

  tcgetattr (fd, &new);

  cfmakeraw (&new);

  cfsetispeed (&new, speed);
  cfsetospeed (&new, speed);

  new.c_cc [VMIN]  = 0;
  new.c_cc [VTIME] = 0;

  tcflush (fd, TCIFLUSH);
  tcsetattr (fd, TCSANOW, &new);
  //fcntl(fd, F_SETFL, O_RDWR);

  serInfo[slot].fd = fd;
  serInfo[slot].flags = serFlags;
  serInfo[slot].state = PI_SER_OPENED;

  return slot;
  }
//}}}
//{{{
int serClose (unsigned handle) {

  if (handle >= PI_SER_SLOTS)
    SOFT_ERROR (PI_BAD_HANDLE, "bad handle (%d)", handle);

  if (serInfo[handle].state != PI_SER_OPENED)
    SOFT_ERROR (PI_BAD_HANDLE, "bad handle (%d)", handle);

  if (serInfo[handle].fd >= 0)
    close (serInfo[handle].fd);

  serInfo[handle].fd = -1;
  serInfo[handle].state = PI_SER_CLOSED;

  return 0;
  }
//}}}

//{{{
int serWriteByte (unsigned handle, unsigned bVal) {

  if (handle >= PI_SER_SLOTS)
    SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

  if (serInfo[handle].state != PI_SER_OPENED)
    SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

  if (bVal > 0xFF)
    SOFT_ERROR(PI_BAD_PARAM, "bad parameter (%d)", bVal);

  char c = bVal;
  if (write (serInfo[handle].fd, &c, 1) != 1)
    return PI_SER_WRITE_FAILED;
  else
    return 0;
  }
//}}}
//{{{
int serReadByte (unsigned handle) {

  int r;
  char x;

  if (handle >= PI_SER_SLOTS)
    SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

  if (serInfo[handle].state != PI_SER_OPENED)
    SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

  r = read(serInfo[handle].fd, &x, 1);
  if (r == 1)
    return ((int)x) & 0xFF;
  else if (r == 0)
    return PI_SER_READ_NO_DATA;
  else if ((r == -1) && (errno == EAGAIN))
    return PI_SER_READ_NO_DATA;
  else
    return PI_SER_READ_FAILED;
  }
//}}}

//{{{
int serWrite (unsigned handle, char *buf, unsigned count) {

  int written = 0;
  int wrote = 0;

  if (handle >= PI_SER_SLOTS)
    SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

  if (serInfo[handle].state != PI_SER_OPENED)
   SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

  if (!count)
    SOFT_ERROR(PI_BAD_PARAM, "bad count (%d)", count);

  while ((written != count) && (wrote >= 0)) {
    wrote = write(serInfo[handle].fd, buf+written, count-written);
    if (wrote >= 0) {
      written += wrote;

      if (written != count)
        timeSleep (0.05);
      }
    }

  if (written != count)
     return PI_SER_WRITE_FAILED;
  else
     return 0;
  }
//}}}
//{{{
int serRead (unsigned handle, char *buf, unsigned count) {

  if (handle >= PI_SER_SLOTS)
    SOFT_ERROR (PI_BAD_HANDLE, "bad handle (%d)", handle);

  if (serInfo[handle].state != PI_SER_OPENED)
    SOFT_ERROR (PI_BAD_HANDLE, "bad handle (%d)", handle);

  if (!count)
    SOFT_ERROR (PI_BAD_PARAM, "bad count (%d)", count);

  int r = read (serInfo[handle].fd, buf, count);

  if (r == -1) {
    if (errno == EAGAIN)
      return PI_SER_READ_NO_DATA;
    else
      return PI_SER_READ_FAILED;
    }
  else {
    if (r < count) buf[r] = 0;
      return r;
    }
  }
//}}}

//{{{
int serDataAvailable (unsigned handle) {

  if (handle >= PI_SER_SLOTS)
    SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

  if (serInfo[handle].state != PI_SER_OPENED)
    SOFT_ERROR(PI_BAD_HANDLE, "bad handle (%d)", handle);

  int result;
  if (ioctl (serInfo[handle].fd, FIONREAD, &result) == -1)
    return 0;

  return result;
  }
//}}}

// bitbang
//{{{
int gpioSerialReadOpen (unsigned gpio, unsigned baud, unsigned data_bits) {

  if (gpio > PI_MAX_USER_GPIO)
    SOFT_ERROR(PI_BAD_USER_GPIO, "bad gpio (%d)", gpio);

  if ((baud < PI_BB_SER_MIN_BAUD) || (baud > PI_BB_SER_MAX_BAUD))
    SOFT_ERROR(PI_BAD_WAVE_BAUD, "gpio %d, bad baud rate (%d)", gpio, baud);

  if ((data_bits < PI_MIN_WAVE_DATABITS) ||
      (data_bits > PI_MAX_WAVE_DATABITS))
    SOFT_ERROR(PI_BAD_DATABITS, "gpio %d, bad data bits (%d)", gpio, data_bits);

  if (wfRx[gpio].mode != PI_WFRX_NONE)
    SOFT_ERROR(PI_GPIO_IN_USE, "gpio %d is already being used", gpio);

  int bitTime = (1000 * MILLION) / baud; /* nanos */
  int timeout  = ((data_bits+2) * bitTime)/MILLION; /* millis */
  if (timeout < 1)
    timeout = 1;

  wfRx[gpio].gpio = gpio;
  wfRx[gpio].mode = PI_WFRX_SERIAL;
  wfRx[gpio].baud = baud;

  wfRx[gpio].s.buf      = malloc(SRX_BUF_SIZE);
  wfRx[gpio].s.bufSize  = SRX_BUF_SIZE;
  wfRx[gpio].s.timeout  = timeout;
  wfRx[gpio].s.fullBit  = bitTime;         /* nanos */
  wfRx[gpio].s.halfBit  = (bitTime/2)+500; /* nanos (500 for rounding) */
  wfRx[gpio].s.readPos  = 0;
  wfRx[gpio].s.writePos = 0;
  wfRx[gpio].s.bit      = -1;
  wfRx[gpio].s.dataBits = data_bits;
  wfRx[gpio].s.invert   = PI_BB_SER_NORMAL;

  if (data_bits <  9)
    wfRx[gpio].s.bytes = 1;
  else if (data_bits < 17)
    wfRx[gpio].s.bytes = 2;
  else
    wfRx[gpio].s.bytes = 4;

  return 0;
}
//}}}
//{{{
int gpioSerialReadInvert (unsigned gpio, unsigned invert) {

  if (gpio > PI_MAX_USER_GPIO)
    SOFT_ERROR(PI_BAD_USER_GPIO, "bad gpio (%d)", gpio);

  if (wfRx[gpio].mode != PI_WFRX_SERIAL)
    SOFT_ERROR(PI_NOT_SERIAL_GPIO, "no serial read on gpio (%d)", gpio);

  if ((invert < PI_BB_SER_NORMAL) ||
      (invert > PI_BB_SER_INVERT))
    SOFT_ERROR(PI_BAD_SER_INVERT, "bad invert level for gpio %d (%d)", gpio, invert);

  wfRx[gpio].s.invert = invert;

  return 0;
  }
//}}}
//{{{
int gpioSerialRead (unsigned gpio, void *buf, size_t bufSize) {

  unsigned bytes=0, wpos;
  volatile wfRx_t *w;

  if (gpio > PI_MAX_USER_GPIO)
    SOFT_ERROR (PI_BAD_USER_GPIO, "bad gpio (%d)", gpio);

  if (bufSize == 0)
    SOFT_ERROR (PI_BAD_SERIAL_COUNT, "buffer size can't be zero");

  if (wfRx[gpio].mode != PI_WFRX_SERIAL)
    SOFT_ERROR (PI_NOT_SERIAL_GPIO, "no serial read on gpio (%d)", gpio);

  w = &wfRx[gpio];
  if (w->s.readPos != w->s.writePos) {
    wpos = w->s.writePos;

    if (wpos > w->s.readPos)
      bytes = wpos - w->s.readPos;
    else
      bytes = w->s.bufSize - w->s.readPos;

    if (bytes > bufSize)
      bytes = bufSize;

    // copy in multiples of the data size in bytes */
    bytes = (bytes / w->s.bytes) * w->s.bytes;
    if (buf)
      memcpy (buf, w->s.buf+w->s.readPos, bytes);

    w->s.readPos += bytes;

    if (w->s.readPos >= w->s.bufSize)
      w->s.readPos = 0;
    }

  return bytes;
  }
//}}}
//{{{
int gpioSerialReadClose (unsigned gpio) {

  if (gpio > PI_MAX_USER_GPIO)
    SOFT_ERROR (PI_BAD_USER_GPIO, "bad gpio (%d)", gpio);

  switch(wfRx[gpio].mode) {
    case PI_WFRX_NONE:
      SOFT_ERROR (PI_NOT_SERIAL_GPIO, "no serial read on gpio (%d)", gpio);
      break;

    case PI_WFRX_SERIAL:
      free(wfRx[gpio].s.buf);
      gpioSetWatchdog (gpio, 0); /* switch off timeouts */
      wfRx[gpio].mode = PI_WFRX_NONE;
      break;
    }

  return 0;
  }
//}}}
//}}}
