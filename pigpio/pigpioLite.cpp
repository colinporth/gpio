// pigpio.cpp - lightweight pigpio
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

// dma
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
#include <limits.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/sysmacros.h>

#include <arpa/inet.h>

#include "pigpioLite.h"

#include "../../shared/utils/cLog.h"
//}}}
//{{{  defines
#define BANK (gpio >> 5)
#define BIT (1 << (gpio & 0x1F))

#define LOG_ERROR(format, arg...)     \
  {                                    \
  cLog::log (LOGINFO, format, ## arg); \
  return -1;                           \
  }
//{{{
#define TIMER_ADD(a, b, result) \
  do {  \
    (result)->tv_sec =  (a)->tv_sec  + (b)->tv_sec;  \
    (result)->tv_nsec = (a)->tv_nsec + (b)->tv_nsec; \
    if ((result)->tv_nsec >= 1000000000) {           \
      ++(result)->tv_sec;                            \
      (result)->tv_nsec -= 1000000000;               \
      }                                              \
    } while (0)
//}}}
//{{{
#define TIMER_SUB(a, b, result) \
  do {                                               \
    (result)->tv_sec =  (a)->tv_sec  - (b)->tv_sec;  \
    (result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; \
    if ((result)->tv_nsec < 0) {                     \
      --(result)->tv_sec;                            \
      (result)->tv_nsec += 1000000000;               \
      }                                              \
   } while (0)
//}}}

//{{{  base addresses
#define PI_PERI_BUS 0x7E000000

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

// lens
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

// BCM2711 has different pulls
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

// standard SPI gpios (ALT0)
#define PI_SPI_CE0   8
#define PI_SPI_CE1   7
#define PI_SPI_SCLK 11
#define PI_SPI_MISO  9
#define PI_SPI_MOSI 10

// auxiliary SPI gpios (ALT4)
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

//{{{  mailbox
#define MB_DEV_MAJOR 100

#define MB_IOCTL _IOWR(MB_DEV_MAJOR, 0, char *)

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

//{{{  wave
#define PI_WAVE_BLOCKS     4
#define PI_WAVE_MAX_PULSES (PI_WAVE_BLOCKS * 3000)
#define PI_WAVE_MAX_CHARS  (PI_WAVE_BLOCKS *  300)

#define PI_BB_I2C_MIN_BAUD     50
#define PI_BB_I2C_MAX_BAUD 500000

#define PI_BB_SPI_MIN_BAUD     50
#define PI_BB_SPI_MAX_BAUD 250000

#define PI_BB_SER_MIN_BAUD     50
#define PI_BB_SER_MAX_BAUD 250000

#define PI_BB_SER_NORMAL 0
#define PI_BB_SER_INVERT 1

#define PI_WAVE_MIN_BAUD      50
#define PI_WAVE_MAX_BAUD 1000000

#define PI_SPI_MIN_BAUD     32000
#define PI_SPI_MAX_BAUD 125000000

#define PI_MIN_WAVE_DATABITS 1
#define PI_MAX_WAVE_DATABITS 32

#define PI_MIN_WAVE_HALFSTOPBITS 2
#define PI_MAX_WAVE_HALFSTOPBITS 8

#define PI_WAVE_MAX_MICROS (30 * 60 * 1000000) /* half an hour */

#define PI_MAX_WAVES 250

#define PI_MAX_WAVE_CYCLES 65535
#define PI_MAX_WAVE_DELAY  65535

#define PI_WAVE_COUNT_PAGES 10

// wave tx mode
#define PI_WAVE_MODE_ONE_SHOT      0
#define PI_WAVE_MODE_REPEAT        1
#define PI_WAVE_MODE_ONE_SHOT_SYNC 2
#define PI_WAVE_MODE_REPEAT_SYNC   3

// special wave at return values
#define PI_WAVE_NOT_FOUND  9998 /* Transmitted wave not found. */
#define PI_NO_TX_WAVE      9999 /* No wave being transmitted. */
//}}}
//{{{  error Codes
#define PI_INIT_FAILED       -1 // gpioInitialise failed
#define PI_BAD_USER_GPIO     -2 // GPIO not 0-31
#define PI_BAD_GPIO          -3 // GPIO not 0-53
#define PI_BAD_MODE          -4 // mode not 0-7
#define PI_BAD_LEVEL         -5 // level not 0-1
#define PI_BAD_PUD           -6 // pud not 0-2
#define PI_BAD_PULSEWIDTH    -7 // pulsewidth not 0 or 500-2500
#define PI_BAD_DUTYCYCLE     -8 // dutycycle outside set range
#define PI_BAD_TIMER         -9 // timer not 0-9
#define PI_BAD_MS           -10 // ms not 10-60000
#define PI_BAD_TIMETYPE     -11 // timetype not 0-1
#define PI_BAD_SECONDS      -12 // seconds < 0
#define PI_BAD_MICROS       -13 // micros not 0-999999
#define PI_TIMER_FAILED     -14 // gpioSetTimerFunc failed
#define PI_BAD_WDOG_TIMEOUT -15 // timeout not 0-60000
#define PI_NO_ALERT_FUNC    -16 // DEPRECATED
#define PI_BAD_CLK_PERIPH   -17 // clock peripheral not 0-1
#define PI_BAD_CLK_SOURCE   -18 // DEPRECATED
#define PI_BAD_CLK_MICROS   -19 // clock micros not 1, 2, 4, 5, 8, or 10
#define PI_BAD_BUF_MILLIS   -20 // buf millis not 100-10000
#define PI_BAD_DUTYRANGE    -21 // dutycycle range not 25-40000
#define PI_BAD_DUTY_RANGE   -21 // DEPRECATED (use PI_BAD_DUTYRANGE)
#define PI_BAD_SIGNUM       -22 // signum not 0-63
#define PI_BAD_PATHNAME     -23 // can't open pathname
#define PI_NO_HANDLE        -24 // no handle available
#define PI_BAD_HANDLE       -25 // unknown handle
#define PI_BAD_IF_FLAGS     -26 // ifFlags > 4
#define PI_BAD_CHANNEL      -27 // DMA channel not 0-15
#define PI_BAD_PRIM_CHANNEL -27 // DMA primary channel not 0-15
#define PI_BAD_SOCKET_PORT  -28 // socket port not 1024-32000
#define PI_BAD_FIFO_COMMAND -29 // unrecognized fifo command
#define PI_BAD_SECO_CHANNEL -30 // DMA secondary channel not 0-15
#define PI_NOT_INITIALISED  -31 // function called before gpioInitialise
#define PI_INITIALISED      -32 // function called after gpioInitialise
#define PI_BAD_WAVE_MODE    -33 // waveform mode not 0-3
#define PI_BAD_CFG_INTERNAL -34 // bad parameter in gpioCfgInternals call
#define PI_BAD_WAVE_BAUD    -35 // baud rate not 50-250K(RX)/50-1M(TX)
#define PI_TOO_MANY_PULSES  -36 // waveform has too many pulses
#define PI_TOO_MANY_CHARS   -37 // waveform has too many chars
#define PI_NOT_SERIAL_GPIO  -38 // no bit bang serial read on GPIO
#define PI_BAD_SERIAL_STRUC -39 // bad (null) serial structure parameter
#define PI_BAD_SERIAL_BUF   -40 // bad (null) serial buf parameter
#define PI_NOT_PERMITTED    -41 // GPIO operation not permitted
#define PI_SOME_PERMITTED   -42 // one or more GPIO not permitted
#define PI_BAD_WVSC_COMMND  -43 // bad WVSC subcommand
#define PI_BAD_WVSM_COMMND  -44 // bad WVSM subcommand
#define PI_BAD_WVSP_COMMND  -45 // bad WVSP subcommand
#define PI_BAD_PULSELEN     -46 // trigger pulse length not 1-100
#define PI_BAD_SCRIPT       -47 // invalid script
#define PI_BAD_SCRIPT_ID    -48 // unknown script id
#define PI_BAD_SER_OFFSET   -49 // add serial data offset > 30 minutes
#define PI_GPIO_IN_USE      -50 // GPIO already in use
#define PI_BAD_SERIAL_COUNT -51 // must read at least a byte at a time
#define PI_BAD_PARAM_NUM    -52 // script parameter id not 0-9
#define PI_DUP_TAG          -53 // script has duplicate tag
#define PI_TOO_MANY_TAGS    -54 // script has too many tags
#define PI_BAD_SCRIPT_CMD   -55 // illegal script command
#define PI_BAD_VAR_NUM      -56 // script variable id not 0-149
#define PI_NO_SCRIPT_ROOM   -57 // no more room for scripts
#define PI_NO_MEMORY        -58 // can't allocate temporary memory
#define PI_SOCK_READ_FAILED -59 // socket read failed
#define PI_SOCK_WRIT_FAILED -60 // socket write failed
#define PI_TOO_MANY_PARAM   -61 // too many script parameters (> 10)
#define PI_NOT_HALTED       -62 // DEPRECATED
#define PI_SCRIPT_NOT_READY -62 // script initialising
#define PI_BAD_TAG          -63 // script has unresolved tag
#define PI_BAD_MICS_DELAY   -64 // bad MICS delay (too large)
#define PI_BAD_MILS_DELAY   -65 // bad MILS delay (too large)
#define PI_BAD_WAVE_ID      -66 // non existent wave id
#define PI_TOO_MANY_CBS     -67 // No more CBs for waveform
#define PI_TOO_MANY_OOL     -68 // No more OOL for waveform
#define PI_EMPTY_WAVEFORM   -69 // attempt to create an empty waveform
#define PI_NO_WAVEFORM_ID   -70 // no more waveforms
#define PI_I2C_OPEN_FAILED  -71 // can't open I2C device
#define PI_SER_OPEN_FAILED  -72 // can't open serial device
#define PI_SPI_OPEN_FAILED  -73 // can't open SPI device
#define PI_BAD_I2C_BUS      -74 // bad I2C bus
#define PI_BAD_I2C_ADDR     -75 // bad I2C address
#define PI_BAD_SPI_CHANNEL  -76 // bad SPI channel
#define PI_BAD_FLAGS        -77 // bad i2c/spi/ser open flags
#define PI_BAD_SPI_SPEED    -78 // bad SPI speed
#define PI_BAD_SER_DEVICE   -79 // bad serial device name
#define PI_BAD_SER_SPEED    -80 // bad serial baud rate
#define PI_BAD_PARAM        -81 // bad i2c/spi/ser parameter
#define PI_I2C_WRITE_FAILED -82 // i2c write failed
#define PI_I2C_READ_FAILED  -83 // i2c read failed
#define PI_BAD_SPI_COUNT    -84 // bad SPI count
#define PI_SER_WRITE_FAILED -85 // ser write failed
#define PI_SER_READ_FAILED  -86 // ser read failed
#define PI_SER_READ_NO_DATA -87 // ser read no data available
#define PI_UNKNOWN_COMMAND  -88 // unknown command
#define PI_SPI_XFER_FAILED  -89 // spi xfer/read/write failed
#define PI_BAD_POINTER      -90 // bad (NULL) pointer
#define PI_NO_AUX_SPI       -91 // no auxiliary SPI on Pi A or B
#define PI_NOT_PWM_GPIO     -92 // GPIO is not in use for PWM
#define PI_NOT_SERVO_GPIO   -93 // GPIO is not in use for servo pulses
#define PI_NOT_HCLK_GPIO    -94 // GPIO has no hardware clock
#define PI_NOT_HPWM_GPIO    -95 // GPIO has no hardware PWM
#define PI_BAD_HPWM_FREQ    -96 // invalid hardware PWM frequency
#define PI_BAD_HPWM_DUTY    -97 // hardware PWM dutycycle not 0-1M
#define PI_BAD_HCLK_FREQ    -98 // invalid hardware clock frequency
#define PI_BAD_HCLK_PASS    -99 // need password to use hardware clock 1
#define PI_HPWM_ILLEGAL    -100 // illegal, PWM in use for main clock
#define PI_BAD_DATABITS    -101 // serial data bits not 1-32
#define PI_BAD_STOPBITS    -102 // serial (half) stop bits not 2-8
#define PI_MSG_TOOBIG      -103 // socket/pipe message too big
#define PI_BAD_MALLOC_MODE -104 // bad memory allocation mode
#define PI_TOO_MANY_SEGS   -105 // too many I2C transaction segments
#define PI_BAD_I2C_SEG     -106 // an I2C transaction segment failed
#define PI_BAD_SMBUS_CMD   -107 // SMBus command not supported by driver
#define PI_NOT_I2C_GPIO    -108 // no bit bang I2C in progress on GPIO
#define PI_BAD_I2C_WLEN    -109 // bad I2C write length
#define PI_BAD_I2C_RLEN    -110 // bad I2C read length
#define PI_BAD_I2C_CMD     -111 // bad I2C command
#define PI_BAD_I2C_BAUD    -112 // bad I2C baud rate, not 50-500k
#define PI_CHAIN_LOOP_CNT  -113 // bad chain loop count
#define PI_BAD_CHAIN_LOOP  -114 // empty chain loop
#define PI_CHAIN_COUNTER   -115 // too many chain counters
#define PI_BAD_CHAIN_CMD   -116 // bad chain command
#define PI_BAD_CHAIN_DELAY -117 // bad chain delay micros
#define PI_CHAIN_NESTING   -118 // chain counters nested too deeply
#define PI_CHAIN_TOO_BIG   -119 // chain is too long
#define PI_DEPRECATED      -120 // deprecated function removed
#define PI_BAD_SER_INVERT  -121 // bit bang serial invert not 0 or 1
#define PI_BAD_EDGE        -122 // bad ISR edge value, not 0-2
#define PI_BAD_ISR_INIT    -123 // bad ISR initialisation
#define PI_BAD_FOREVER     -124 // loop forever must be last command
#define PI_BAD_FILTER      -125 // bad filter parameter
#define PI_BAD_PAD         -126 // bad pad number
#define PI_BAD_STRENGTH    -127 // bad pad drive strength
#define PI_FIL_OPEN_FAILED -128 // file open failed
#define PI_BAD_FILE_MODE   -129 // bad file mode
#define PI_BAD_FILE_FLAG   -130 // bad file flag
#define PI_BAD_FILE_READ   -131 // bad file read
#define PI_BAD_FILE_WRITE  -132 // bad file write
#define PI_FILE_NOT_ROPEN  -133 // file not open for read
#define PI_FILE_NOT_WOPEN  -134 // file not open for write
#define PI_BAD_FILE_SEEK   -135 // bad file seek
#define PI_NO_FILE_MATCH   -136 // no files match pattern
#define PI_NO_FILE_ACCESS  -137 // no permission to access file
#define PI_FILE_IS_A_DIR   -138 // file is a directory
#define PI_BAD_SHELL_STATUS -139 // bad shell return status
#define PI_BAD_SCRIPT_NAME -140 // bad script name
#define PI_BAD_SPI_BAUD    -141 // bad SPI baud rate, not 50-500k
#define PI_NOT_SPI_GPIO    -142 // no bit bang SPI in progress on GPIO
#define PI_BAD_EVENT_ID    -143 // bad event id
#define PI_CMD_INTERRUPTED -144 // Used by Python
#define PI_NOT_ON_BCM2711  -145 // not available on BCM2711
#define PI_ONLY_ON_BCM2711 -146 // only available on BCM2711

#define PI_PIGIF_ERR_0    -2000
#define PI_PIGIF_ERR_99   -2099

#define PI_CUSTOM_ERR_0   -3000
#define PI_CUSTOM_ERR_999 -3999

//}}}
//}}}
//{{{  typedef
typedef void (*callbk_t) ();
//{{{
struct gpioInfo_t {
  uint8_t  is;
  uint8_t  pad;
  uint16_t width;
  uint16_t range; /* dutycycles specified by 0 .. range */
  uint16_t freqIdx;
  uint16_t deferOff;
  uint16_t deferRng;
  };
//}}}
//{{{
struct gpioTimer_t {
  callbk_t func;
  uint32_t ex;
  void* userdata;
  uint32_t id;
  uint32_t running;
  uint32_t millis;
  pthread_t pthId;
  };
//}}}

//{{{
struct rawCbs_t {
// linux/arch/arm/mach-bcm2708/include/mach/dma.h */
  uint32_t info;
  uint32_t src;
  uint32_t dst;
  uint32_t length;
  uint32_t stride;
  uint32_t next;
  uint32_t pad[2];
  };
//}}}
//{{{
struct dmaPage_t {
  rawCbs_t cb[128];
  };
//}}}
//{{{
struct dmaIPage_t {
  rawCbs_t cb[CBS_PER_IPAGE];
  uint32_t level[LVS_PER_IPAGE];
  uint32_t gpioOff[OFF_PER_IPAGE];
  uint32_t tick[TCK_PER_IPAGE];
  uint32_t gpioOn[ON_PER_IPAGE];
  uint32_t periphData;
  uint32_t pad[PAD_PER_IPAGE];
  } ;
//}}}
//{{{
struct dmaOPage_t {
  rawCbs_t cb     [CBS_PER_OPAGE];
  uint32_t OOL    [OOL_PER_OPAGE];
  uint32_t periphData;
  };
//}}}

//{{{
struct clkCfg_t {
  uint16_t valid;
  uint16_t servoIdx;
  };
//}}}
//{{{
struct i2cInfo_t {
  uint16_t state;
  int16_t  fd;
  uint32_t addr;
  uint32_t flags;
  uint32_t funcs;
  };
//}}}
//{{{
struct serInfo_t {
  uint16_t state;
  int16_t  fd;
  uint32_t flags;
  };
//}}}
//{{{
struct spiInfo_t {
  uint16_t state;
  uint32_t speed;
  uint32_t flags;
  };
//}}}
//{{{
struct gpioCfg_t {
  uint32_t bufferMilliseconds;
  uint32_t clockMicros;
  uint32_t clockPeriph;
  uint32_t DMAprimaryChannel;
  uint32_t DMAsecondaryChannel;
  uint32_t ifFlags;
  uint32_t memAllocMode;
  uint32_t alertFreq;
  uint32_t internals;
  };
//}}}

//{{{
struct wfStats_t {
  uint32_t micros;
  uint32_t highMicros;
  uint32_t maxMicros;
  uint32_t pulses;
  uint32_t highPulses;
  uint32_t maxPulses;
  uint32_t cbs;
  uint32_t highCbs;
  uint32_t maxCbs;
  };
//}}}
//{{{
struct wfRxSerial_t {
  char*    buf;
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
  };
//}}}
//{{{
struct wfRxI2C_t {
  int SDA;
  int SCL;
  int delay;
  int SDAMode;
  int SCLMode;
  int started;
  };
//}}}
//{{{
struct wfRxSPI_t {
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
  };
//}}}
//{{{
struct wfRx_t {
  int mode;
  int gpio;
  uint32_t baud;

  pthread_mutex_t mutex;

  union {
    wfRxSerial_t s;
    wfRxI2C_t    I;
    wfRxSPI_t    S;
    };
  };
//}}}

//{{{
union my_smbus_data {
  uint8_t  byte;
  uint16_t word;
  uint8_t  block[PI_I2C_SMBUS_BLOCK_MAX + 2];
  };
//}}}
//{{{
struct my_smbus_ioctl_data {
  uint8_t read_write;
  uint8_t command;
  uint32_t size;
  union my_smbus_data* data;
  };
//}}}
//{{{
struct my_i2c_rdwr_ioctl_data_t {
  pi_i2c_msg_t* msgs; // pointers to pi_i2c_msgs
  uint32_t     nmsgs; // number of pi_i2c_msgs
  };
//}}}

//{{{
struct clkInf_t {
  uint32_t div;
  uint32_t frac;
  uint32_t clock;
  };
//}}}
//{{{
struct DMAMem_t {
  uint32_t   handle;       // mbAllocateMemory()
  uintptr_t  bus_addr;     // mbLockMemory()
  uintptr_t* virtual_addr; // mbMapMem()
  uint32_t   size;         // in bytes
  };
//}}}
//}}}
//{{{  static var
static volatile uint32_t piCores = 0;
static volatile uint32_t pi_peri_phys = 0x20000000;
static volatile uint32_t pi_dram_bus = 0x40000000;
static volatile uint32_t pi_mem_flag = 0x0C;
static volatile uint32_t pi_ispi = 0;
static volatile uint32_t pi_is_2711 = 0;

static volatile uint32_t clk_osc_freq = CLK_OSC_FREQ;
static volatile uint32_t clk_plld_freq = CLK_PLLD_FREQ;

static volatile uint32_t hw_pwm_max_freq = PI_HW_PWM_MAX_FREQ;
static volatile uint32_t hw_clk_min_freq = PI_HW_CLK_MIN_FREQ;
static volatile uint32_t hw_clk_max_freq = PI_HW_CLK_MAX_FREQ;

static struct timespec libStarted;
static int PWMClockInited = 0;
static int gpioMaskSet = 0;

static uint64_t gpioMask;
static int wfc[3] = {0, 0, 0};
static int wfcur = 0;
static wfStats_t wfStats = {
  0, 0, PI_WAVE_MAX_MICROS,
  0, 0, PI_WAVE_MAX_PULSES,
  0, 0, (DMAO_PAGES * CBS_PER_OPAGE)
  };
static wfRx_t wfRx[PI_MAX_USER_GPIO+1];

static gpioInfo_t gpioInfo[PI_MAX_GPIO+1];
static i2cInfo_t i2cInfo[PI_I2C_SLOTS];
static serInfo_t serInfo[PI_SER_SLOTS];
static spiInfo_t spiInfo[PI_SPI_SLOTS];
static gpioTimer_t gpioTimer[PI_MAX_TIMER+1];
static int pwmFreq[PWM_FREQS];

static uint32_t spiDummyRead;

//{{{  static var - reset after gpioTerminated
// resources which must be released on gpioTerminate */
static int fdMem = -1;
static int fdPmap = -1;
static int fdMbox = -1;

static DMAMem_t* dmaMboxBlk = nullptr;
static uintptr_t** dmaPMapBlk = nullptr;
static dmaPage_t** dmaVirt = nullptr;
static dmaPage_t** dmaBus = nullptr;
static dmaIPage_t** dmaIVirt = nullptr;
static dmaIPage_t** dmaIBus = nullptr;
static dmaOPage_t** dmaOVirt = nullptr;
static dmaOPage_t** dmaOBus = nullptr;

static volatile uint32_t* auxReg  = nullptr;
static volatile uint32_t* bscsReg = nullptr;
static volatile uint32_t* clkReg  = nullptr;
static volatile uint32_t* dmaReg  = nullptr;
static volatile uint32_t* padsReg = nullptr;
static volatile uint32_t* pcmReg  = nullptr;
static volatile uint32_t* pwmReg  = nullptr;
static volatile uint32_t* spiReg  = nullptr;
static volatile uint32_t* systReg = nullptr;

static volatile uint32_t* gpioReg = nullptr;
static volatile uint32_t* gpioClr0Reg = nullptr;
static volatile uint32_t* gpioSet0Reg = nullptr;

static volatile uint32_t* dmaIn   = nullptr;
static volatile uint32_t* dmaOut  = nullptr;

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
  0, /* alertFreq */
  0, /* internals */
  };
//}}}
//{{{  static var - no initialisation
static uint32_t bufferBlocks; // number of blocks in buffer
static uint32_t bufferCycles; // number of cycles

static uint32_t spiDummy;

static uint32_t old_mode_ce0;
static uint32_t old_mode_ce1;
static uint32_t old_mode_sclk;
static uint32_t old_mode_miso;
static uint32_t old_mode_mosi;

static uint32_t old_spi_cs;
static uint32_t old_spi_clk;

static uint32_t old_mode_ace0;
static uint32_t old_mode_ace1;
static uint32_t old_mode_ace2;
static uint32_t old_mode_asclk;
static uint32_t old_mode_amiso;
static uint32_t old_mode_amosi;

static uint32_t old_spi_cntl0;
static uint32_t old_spi_cntl1;

static uint32_t bscFR;
//}}}
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
static const clkCfg_t clkCfg[] = {
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

//{{{
static void sigHandler (int signum) {

  cLog::log (LOGINFO, "Unhandled signal %d, terminating\n", signum);
  gpioTerminate();
  exit (-1);
  }
//}}}
//{{{
static void sigSetHandler() {

  #define PI_MIN_SIGNUM 0
  #define PI_MAX_SIGNUM 63

  struct sigaction newSigAction;
  for (int i = PI_MIN_SIGNUM; i <= PI_MAX_SIGNUM; i++) {
    memset (&newSigAction, 0, sizeof(newSigAction));
    newSigAction.sa_handler = sigHandler;
    sigaction (i, &newSigAction, NULL);
    }
  }
//}}}

//{{{  dma helpers
//{{{
static rawCbs_t* dmaCB2adr (int pos) {

  int page = pos / CBS_PER_IPAGE;
  int slot = pos % CBS_PER_IPAGE;
  return &dmaIVirt[page]->cb[slot];
  }
//}}}
//{{{
static uint32_t dmaPwmDataAdr (int pos) {
//cast twice to suppress compiler warning, I belive this cast is ok
//because dmaIbus contains bus addresses, not user addresses. --plugwash

  return (uint32_t)(uintptr_t) &dmaIBus[pos]->periphData;
  }
//}}}
//{{{
static uint32_t dmaGpioOnAdr (int pos) {

  int page = pos / ON_PER_IPAGE;
  int slot = pos % ON_PER_IPAGE;

  //cast twice to suppress compiler warning, I belive this cast is ok
  //because dmaIbus contains bus addresses, not user addresses. --plugwash
  return (uint32_t)(uintptr_t) &dmaIBus[page]->gpioOn[slot];
  }
//}}}

//{{{
static void myOffPageSlot (int pos, int* page, int* slot) {
  *page = pos / OFF_PER_IPAGE;
  *slot = pos % OFF_PER_IPAGE;
  }
//}}}
//{{{
static uint32_t dmaGpioOffAdr (int pos) {

  int page, slot;
  myOffPageSlot (pos, &page, &slot);

  //cast twice to suppress compiler warning, I belive this cast is ok
  //because dmaIbus contains bus addresses, not user addresses. --plugwash
  return (uint32_t)(uintptr_t) &dmaIBus[page]->gpioOff[slot];
  }
//}}}
//{{{
static void myTckPageSlot (int pos, int* page, int* slot) {
  *page = pos / TCK_PER_IPAGE;
  *slot = pos % TCK_PER_IPAGE;
  }
//}}}
//{{{
static uint32_t dmaTickAdr (int pos) {

  int page, slot;
  myTckPageSlot (pos, &page, &slot);

  //cast twice to suppress compiler warning, I belive this cast is ok
  //because dmaIbus contains bus addresses, not user addresses. --plugwash
  return (uint32_t)(uintptr_t) &dmaIBus[page]->tick[slot];
  }
//}}}
//{{{
static void myLvsPageSlot (int pos, int* page, int* slot) {
  *page = pos/LVS_PER_IPAGE;
  *slot = pos%LVS_PER_IPAGE;
  }
//}}}
//{{{
static uint32_t dmaReadLevelsAdr (int pos) {

  int page, slot;
  myLvsPageSlot (pos, &page, &slot);

  //cast twice to suppress compiler warning, I belive this cast is ok
  //because dmaIbus contains bus addresses, not user addresses. --plugwash
  return (uint32_t)(uintptr_t) &dmaIBus[page]->level[slot];
  }
//}}}
//{{{
static uint32_t dmaCbAdr (int pos) {

  int page = (pos / CBS_PER_IPAGE);
  int slot = (pos % CBS_PER_IPAGE);

  //cast twice to suppress compiler warning, I belive this cast is ok
  //because dmaIbus contains bus addresses, not user addresses. --plugwash
  return (uint32_t)(uintptr_t) &dmaIBus[page]->cb[slot];
  }
//}}}

//{{{
static void dmaGpioOnCb (int b, int pos) {

  rawCbs_t* p = dmaCB2adr (b);

  p->info   = NORMAL_DMA;
  p->src    = dmaGpioOnAdr (pos);
  p->dst    = ((GPIO_BASE + (GPSET0*4)) & 0x00ffffff) | PI_PERI_BUS;
  p->length = 4;
  p->next   = dmaCbAdr (b+1);
  }
//}}}
//{{{
static void dmaTickCb (int b, int pos) {

  rawCbs_t* p = dmaCB2adr (b);

  p->info   = NORMAL_DMA;
  p->src    = ((SYST_BASE + (SYST_CLO*4)) & 0x00ffffff) | PI_PERI_BUS;
  p->dst    = dmaTickAdr (pos);
  p->length = 4;
  p->next   = dmaCbAdr (b+1);
  }
//}}}
//{{{
static void dmaGpioOffCb (int b, int pos) {

  rawCbs_t* p = dmaCB2adr (b);

  p->info   = NORMAL_DMA;
  p->src    = dmaGpioOffAdr (pos);
  p->dst    = ((GPIO_BASE + (GPCLR0*4)) & 0x00ffffff) | PI_PERI_BUS;
  p->length = 4;
  p->next   = dmaCbAdr (b+1);
  }
//}}}
//{{{
static void dmaReadLevelsCb (int b, int pos) {

  rawCbs_t* p = dmaCB2adr (b);

  p->info   = NORMAL_DMA;
  p->src    = ((GPIO_BASE + (GPLEV0*4)) & 0x00ffffff) | PI_PERI_BUS;
  p->dst    = dmaReadLevelsAdr (pos);
  p->length = 4;
  p->next   = dmaCbAdr (b+1);
  }
//}}}
//{{{
static void dmaDelayCb (int b) {

  rawCbs_t* p = dmaCB2adr (b);

  if (gpioCfg.clockPeriph == PI_CLOCK_PCM) {
    p->info = NORMAL_DMA | TIMED_DMA(2);
    p->dst  = PCM_TIMER;
    }
  else {
    p->info = NORMAL_DMA | TIMED_DMA(5);
    p->dst  = PWM_TIMER;
    }

  p->src    = dmaPwmDataAdr (b % DMAI_PAGES);
  p->length = 4;
  p->next   = dmaCbAdr (b+1);
  }
//}}}
//{{{
static void dmaInitCbs() {
// set up the DMA control blocks

  int b = -1;
  int level = 0;

  for (int cycle = 0; cycle < (int)bufferCycles; cycle++) {
    b++;
    dmaGpioOnCb (b, cycle%SUPERCYCLE); /* gpio on slot */
    b++;
    dmaTickCb (b, cycle);              /* tick slot */
    for (int pulse = 0; pulse < PULSE_PER_CYCLE; pulse++) {
      b++;
      dmaReadLevelsCb (b, level);               /* read levels slot */
      b++;
      dmaDelayCb (b);                           /* delay slot */
      b++;
      dmaGpioOffCb (b, (level%SUPERLEVEL)+1);   /* gpio off slot */
      ++level;
      }
    }

  // point last cb back to first for continuous loop
  rawCbs_t* p = dmaCB2adr(b);
  p->next = dmaCbAdr(0);
  }
//}}}
//}}}
//{{{  mailbox helpers
//{{{
static int mbCreate (char* dev) {
// https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface

  unlink (dev);
  return mknod (dev, S_IFCHR | 0600, makedev (MB_DEV_MAJOR, 0));
  }
//}}}
//{{{
static int mbOpen() {

  int fd = open ("/dev/vcio", 0);
  if (fd < 0) {
    const char* mb2 = "/dev/pigpio-mb";
    mbCreate ((char*)mb2);
    fd = open (mb2, 0);
    }

  return fd;
  }
//}}}
static int mbProperty (int fd, void* buf) { return ioctl (fd, MB_IOCTL, buf); }
static void mbClose (int fd) { close(fd); }

//{{{
static uint32_t mbAllocateMemory (int fd, uint32_t size, uint32_t align, uint32_t flags) {

  int i = 1;
  uint32_t p[32];
  p[i++] = MB_PROCESS_REQUEST;
  p[i++] = MB_ALLOCATE_MEMORY_TAG;
  p[i++] = 12;
  p[i++] = 12;
  p[i++] = size;
  p[i++] = align;
  p[i++] = flags;
  p[i++] = MB_END_TAG;
  p[0] = i * sizeof (*p);

  mbProperty (fd, p);

  return p[5];
  }
//}}}
//{{{
static uint32_t mbLockMemory (int fd, uint32_t handle) {

  int i = 1;
  uint32_t p[32];
  p[i++] = MB_PROCESS_REQUEST;
  p[i++] = MB_LOCK_MEMORY_TAG;
  p[i++] = 4;
  p[i++] = 4;
  p[i++] = handle;
  p[i++] = MB_END_TAG;
  p[0] = i * sizeof (*p);
  mbProperty (fd, p);

  return p[5];
  }
//}}}
//{{{
static uint32_t mbUnlockMemory (int fd, uint32_t handle) {

  int i = 1;
  uint32_t p[32];
  p[i++] = MB_PROCESS_REQUEST;
  p[i++] = MB_UNLOCK_MEMORY_TAG;
  p[i++] = 4;
  p[i++] = 4;
  p[i++] = handle;
  p[i++] = MB_END_TAG;
  p[0] = i * sizeof (*p);
  mbProperty (fd, p);

  return p[5];
  }
//}}}
//{{{
static uint32_t mbReleaseMemory (int fd, uint32_t handle) {

  int i = 1;
  uint32_t p[32];
  p[i++] = MB_PROCESS_REQUEST;
  p[i++] = MB_RELEASE_MEMORY_TAG;
  p[i++] = 4;
  p[i++] = 4;
  p[i++] = handle;
  p[i++] = MB_END_TAG;
  p[0] = i * sizeof (*p);
  mbProperty (fd, p);

  return p[5];
  }
//}}}

//{{{
static void* mbMapMem (uint32_t base, uint32_t size) {
  return mmap (0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fdMem, base);
  }
//}}}
static int mbUnmapMem (void* addr, uint32_t size) { return munmap (addr, size); }
//{{{
static void mbDMAFree (DMAMem_t* DMAMem) {

  if (DMAMem->handle) {
    mbUnmapMem (DMAMem->virtual_addr, DMAMem->size);
    mbUnlockMemory (fdMbox, DMAMem->handle);
    mbReleaseMemory (fdMbox, DMAMem->handle);
    DMAMem->handle = 0;
    }
  }
//}}}
//{{{
static int mbDMAAlloc (DMAMem_t* DMAMem, uint32_t size, uint32_t pi_mem_flag) {

  DMAMem->size = size;
  DMAMem->handle = mbAllocateMemory (fdMbox, size, PAGE_SIZE, pi_mem_flag);

  if (DMAMem->handle) {
    DMAMem->bus_addr = mbLockMemory (fdMbox, DMAMem->handle);
    DMAMem->virtual_addr = (uintptr_t*)mbMapMem (BUS_TO_PHYS (DMAMem->bus_addr), size);
    return 1;
    }

  return 0;
  }
//}}}
//}}}
//{{{  init helpers
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
    while ((systReg[SYST_CLO] - start) <= micros) {}
    }
  else
    myGpioSleep (micros / 1000000, micros % 1000000);

  return (systReg[SYST_CLO] - start);
  }
//}}}

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

  for (int i = 0; i <= PI_MAX_TIMER; i++) {
    gpioTimer[i].running = 0;
    gpioTimer[i].func = NULL;
    }

  // calculate the usable PWM frequencies */
  for (int i = 0; i < PWM_FREQS; i++)
    pwmFreq[i]= (1000000.0 / ((float)PULSE_PER_CYCLE * gpioCfg.clockMicros * pwmCycles[i])) + 0.5;

  fdMem = -1;

  gpioReg = nullptr;
  gpioClr0Reg = nullptr;
  gpioSet0Reg = nullptr;

  dmaMboxBlk = nullptr;
  dmaPMapBlk = nullptr;
  dmaVirt = nullptr;
  dmaBus  = nullptr;
  auxReg  = nullptr;
  clkReg  = nullptr;
  dmaReg  = nullptr;
  pcmReg  = nullptr;
  pwmReg  = nullptr;
  systReg = nullptr;
  spiReg  = nullptr;
  }
//}}}
//{{{
static int initCheckPermitted() {

  if (!pi_ispi) {
    cLog::log (LOGINFO,
         "\n" \
         "+---------------------------------------------------------+\n" \
         "|Sorry, this system does not appear to be a raspberry pi. |\n" \
         "|aborting.                                                |\n" \
         "+---------------------------------------------------------+\n\n");
    return -1;
    }

  if ((fdMem = open("/dev/mem", O_RDWR | O_SYNC) ) < 0) {
    cLog::log (LOGINFO,
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
  return (uint32_t*) mmap (0, len, PROT_READ | PROT_WRITE|PROT_EXEC, MAP_SHARED | MAP_LOCKED, fd, addr);
  }
//}}}
//{{{
static int initPeripherals() {

  gpioReg = initMapMem (fdMem, GPIO_BASE, GPIO_LEN);
  if (gpioReg == nullptr)
    LOG_ERROR ("mmap gpio failed (%m)");
  gpioClr0Reg = gpioReg + GPCLR0;
  gpioSet0Reg = gpioReg + GPSET0;

  dmaReg = initMapMem (fdMem, DMA_BASE, DMA_LEN);
  if (dmaReg == nullptr)
    LOG_ERROR ("mmap dma failed (%m)");

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

  clkReg = initMapMem (fdMem, CLK_BASE,  CLK_LEN);
  if (clkReg == nullptr)
    LOG_ERROR ("mmap clk failed (%m)");

  systReg = initMapMem (fdMem, SYST_BASE,  SYST_LEN);
  if (systReg == nullptr)
    LOG_ERROR ("mmap syst failed (%m)");

  spiReg = initMapMem (fdMem, SPI_BASE,  SPI_LEN);
  if (spiReg == nullptr)
    LOG_ERROR ("mmap spi failed (%m)");

  pwmReg = initMapMem (fdMem, PWM_BASE,  PWM_LEN);
  if (pwmReg == nullptr)
    LOG_ERROR ("mmap pwm failed (%m)");

  pcmReg = initMapMem (fdMem, PCM_BASE,  PCM_LEN);
  if (pcmReg == nullptr)
    LOG_ERROR ("mmap pcm failed (%m)");

  auxReg = initMapMem (fdMem, AUX_BASE,  AUX_LEN);
  if (auxReg == nullptr)
    LOG_ERROR ("mmap aux failed (%m)");

  padsReg = initMapMem (fdMem, PADS_BASE,  PADS_LEN);
  if (padsReg == nullptr)
    LOG_ERROR ("mmap pads failed (%m)");

  bscsReg = initMapMem (fdMem, BSCS_BASE,  BSCS_LEN);
  if (bscsReg == nullptr)
    LOG_ERROR ("mmap bscs failed (%m)");

  return 0;
  }
//}}}

//{{{
static int initZaps (int pmapFd, void* virtualBase, int  basePage, int  pages) {

  int status = 0;
  uintptr_t pageAdr = (uintptr_t) dmaVirt[basePage];
  off_t index = ((off_t)virtualBase / PAGE_SIZE) * 8;
  off_t offset = lseek (pmapFd, index, SEEK_SET);
  if (offset != index)
    LOG_ERROR ("lseek pagemap failed (%m)");

  for (int n = 0; n < pages; n++) {
    uint64_t pa;
    ssize_t t = read (pmapFd, &pa, sizeof(pa));
    if (t != sizeof(pa))
      LOG_ERROR ("read pagemap failed (%m)");

    uint32_t physical = 0x3FFFFFFF & (PAGE_SIZE * (pa & 0xFFFFFFFF));
    if (physical) {
      // cast twice to suppress warning, I belive this is ok as these
      // are bus addresses, not virtual addresses. --plugwash
      dmaBus[basePage+n] = (dmaPage_t*)(uintptr_t)(physical | pi_dram_bus);

      dmaVirt[basePage+n] = (dmaPage_t*)mmap ((void*)pageAdr, PAGE_SIZE, PROT_READ | PROT_WRITE,
                                              MAP_SHARED | MAP_FIXED|MAP_LOCKED | MAP_NORESERVE,
                                              fdMem, physical);
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

  dmaPMapBlk[block] = (long unsigned int*)mmap (0, (PAGES_PER_BLOCK * PAGE_SIZE),
                                                    PROT_READ | PROT_WRITE,
                                                    MAP_SHARED | MAP_ANONYMOUS|MAP_NORESERVE|MAP_LOCKED, -1, 0);
  if (dmaPMapBlk[block] == nullptr)
    LOG_ERROR ("mmap dma block %d failed (%m)", block);

  // force allocation of physical memory
  memset ((void*)dmaPMapBlk[block], 0xAA, (PAGES_PER_BLOCK*PAGE_SIZE));
  memset ((void*)dmaPMapBlk[block], 0xFF, (PAGES_PER_BLOCK*PAGE_SIZE));
  memset ((void*)dmaPMapBlk[block], 0, (PAGES_PER_BLOCK*PAGE_SIZE));
  uint32_t pageNum = block * PAGES_PER_BLOCK;
  dmaVirt[pageNum] = (dmaPage_t*)mmap (0, (PAGES_PER_BLOCK * PAGE_SIZE),
                                       PROT_READ | PROT_WRITE,
                                       MAP_SHARED | MAP_ANONYMOUS | MAP_NORESERVE | MAP_LOCKED, -1, 0);
  if (dmaVirt[pageNum] == nullptr)
    LOG_ERROR ("mmap dma block %d failed (%m)", block);

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
    LOG_ERROR ("initZaps failed");

  return 0;
  }
//}}}
//{{{
static int initMboxBlock (int block) {

  int ok = mbDMAAlloc (&dmaMboxBlk[block], PAGES_PER_BLOCK * PAGE_SIZE, pi_mem_flag);
  if (!ok)
    LOG_ERROR ("init mbox zaps failed");

  uint32_t page = block * PAGES_PER_BLOCK;
  uintptr_t virtualAdr = (uintptr_t)dmaMboxBlk[block].virtual_addr;
  uintptr_t busAdr = dmaMboxBlk[block].bus_addr;

  for (int n = 0; n < PAGES_PER_BLOCK; n++) {
    dmaVirt[page+n] = (dmaPage_t*)virtualAdr;
    dmaBus[page+n] = (dmaPage_t*)busAdr;
    virtualAdr += PAGE_SIZE;
    busAdr += PAGE_SIZE;
    }

  return 0;
  }
//}}}
//{{{
static int initAllocDMAMem() {
// Calculate the number of blocks needed for buffers
// The number of blocks must be a multiple of the 20ms servo cycle

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
  dmaVirt = (dmaPage_t**) mmap (0, PAGES_PER_BLOCK * (bufferBlocks + PI_WAVE_BLOCKS) * sizeof(dmaPage_t *),
                                PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED, -1, 0);

  if (dmaVirt == nullptr)
    LOG_ERROR ("mmap dma virtual failed (%m)");

  dmaBus = (dmaPage_t**)mmap (0, PAGES_PER_BLOCK*(bufferBlocks+PI_WAVE_BLOCKS)*sizeof(dmaPage_t *),
                              PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED, -1, 0);

  if (dmaBus == nullptr)
    LOG_ERROR ("mmap dma bus failed (%m)");

  dmaIVirt = (dmaIPage_t**) dmaVirt;
  dmaIBus = (dmaIPage_t**) dmaBus;
  dmaOVirt = (dmaOPage_t**)(dmaVirt + (PAGES_PER_BLOCK*bufferBlocks));
  dmaOBus = (dmaOPage_t**)(dmaBus  + (PAGES_PER_BLOCK*bufferBlocks));

  if ((gpioCfg.memAllocMode == PI_MEM_ALLOC_PAGEMAP) ||
      ((gpioCfg.memAllocMode == PI_MEM_ALLOC_AUTO) && (gpioCfg.bufferMilliseconds > PI_DEFAULT_BUFFER_MILLIS))) {
    //{{{  pagemap allocation of DMA memory
    dmaPMapBlk = (uintptr_t**)mmap (0, (bufferBlocks + PI_WAVE_BLOCKS) * sizeof(dmaPage_t*),
                                    PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED, -1, 0);

    if (dmaPMapBlk == nullptr)
      LOG_ERROR ("pagemap mmap block failed (%m)");

    fdPmap = open ("/proc/self/pagemap", O_RDONLY);
    if (fdPmap < 0)
      LOG_ERROR ("pagemap open failed(%m)");

    for (int i = 0; i < ((int)bufferBlocks+PI_WAVE_BLOCKS); i++) {
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
    dmaMboxBlk = (DMAMem_t*)mmap (0, (bufferBlocks+PI_WAVE_BLOCKS)*sizeof(DMAMem_t),
                                 PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED, -1, 0);

    if (dmaMboxBlk == nullptr)
      LOG_ERROR ("mmap mbox block failed (%m)");

    fdMbox = mbOpen();
    if (fdMbox < 0)
      LOG_ERROR ("mbox open failed(%m)");

    for (int i = 0; i < ((int)bufferBlocks+PI_WAVE_BLOCKS); i++) {
      int status = initMboxBlock (i);
      if (status < 0) {
        mbClose(fdMbox);
        return status;
        }
      }

    mbClose (fdMbox);
    }
    //}}}

  return 0;
  }
//}}}

//{{{
static void initPWM (uint32_t bits) {

  // reset PWM
  pwmReg[PWM_CTL] = 0;
  myGpioDelay (10);
  pwmReg[PWM_STA] = -1;
  myGpioDelay (10);

  // set number of bits to transmit
  pwmReg[PWM_RNG1] = bits;
  myGpioDelay (10);
  dmaIVirt[0]->periphData = 1;

  // enable PWM DMA, raise panic and dreq thresholds to 15
  pwmReg[PWM_DMAC] = PWM_DMAC_ENAB | PWM_DMAC_PANIC(15) | PWM_DMAC_DREQ(15);
  myGpioDelay (10);

  // clear PWM fifo
  pwmReg[PWM_CTL] = PWM_CTL_CLRF1;
  myGpioDelay (10);

  // enable PWM channel 1 and use fifo
  pwmReg[PWM_CTL] = PWM_CTL_USEF1 | PWM_CTL_MODE1 | PWM_CTL_PWEN1;
  }
//}}}
//{{{
static void initPCM (uint32_t bits) {

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

  pcmReg[PCM_MODE] = PCM_MODE_FLEN (bits-1); /* # bits in frame */

  // enable channel 1 with # bits width */
  pcmReg[PCM_TXC] = PCM_TXC_CH1EN | PCM_TXC_CH1WID (bits-8);
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

  #define BCM_PASSWD  (0x5A << 24)

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

  const uint32_t BITS = 10;

  uint32_t micros;
  if (mainClock)
    micros = gpioCfg.clockMicros;
  else
    micros = PI_WF_MICROS;

  uint32_t clkCtl;
  uint32_t clkDiv;
  int clockPWM = mainClock ^ (gpioCfg.clockPeriph == PI_CLOCK_PCM);
  if (clockPWM) {
    clkCtl = CLK_PWMCTL;
    clkDiv = CLK_PWMDIV;
    }
  else {
    clkCtl = CLK_PCMCTL;
    clkDiv = CLK_PCMDIV;
    }

  uint32_t clkSrc  = CLK_CTL_SRC_PLLD;
  uint32_t clkDivI = clk_plld_freq / (10000000 / micros); /* 10 MHz - 1 MHz */
  uint32_t clkBits = BITS;        /* 10/BITS MHz - 1/BITS MHz */
  uint32_t clkDivF = 0;
  uint32_t clkMash = 0;

  initHWClk (clkCtl, clkDiv, clkSrc, clkDivI, clkDivF, clkMash);

  if (clockPWM)
    initPWM (clkBits);
  else
    initPCM (clkBits);

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

  initKillDMA (dmaAddr);
  dmaAddr[DMA_CS] = DMA_INTERRUPT_STATUS | DMA_END_FLAG;
  dmaAddr[DMA_CONBLK_AD] = cbAddr;

  // clear READ/FIFO/READ_LAST_NOT_SET error bits */
  dmaAddr[DMA_DEBUG] = DMA_DEBUG_READ_ERR | DMA_DEBUG_FIFO_ERR | DMA_DEBUG_RD_LST_NOT_SET_ERR;
  dmaAddr[DMA_CS] = DMA_WAIT_ON_WRITES | DMA_PANIC_PRIORITY(8) | DMA_PRIORITY(8) | DMA_ACTIVE;
  }
//}}}

//{{{
static void flushMemory() {

  #define FLUSH_PAGES 1024
  static int val = 0;

  void* dummy = mmap (0, (FLUSH_PAGES * PAGE_SIZE),
                      PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_SHARED | MAP_ANONYMOUS | MAP_NORESERVE | MAP_LOCKED,
                      -1, 0);

  if (dummy == MAP_FAILED)
    cLog::log (LOGINFO,  "mmap dummy failed (%m)");
  else {
    memset (dummy, val++, FLUSH_PAGES * PAGE_SIZE);
    memset (dummy, val++, FLUSH_PAGES * PAGE_SIZE);
    munmap (dummy, FLUSH_PAGES * PAGE_SIZE);
    }
  }
//}}}
//{{{
static void initReleaseResources() {

  for (int i = 0; i <= PI_MAX_TIMER; i++) {
    if (gpioTimer[i].running) {
      // destroy thread
      pthread_cancel (gpioTimer[i].pthId);
      pthread_join (gpioTimer[i].pthId, NULL);
      gpioTimer[i].running = 0;
      }
    }

  //{{{  release mmap'd memory
  if (auxReg)
    munmap ((void*)auxReg,  AUX_LEN);
  auxReg  = nullptr;

  if (bscsReg)
    munmap ((void*)bscsReg, BSCS_LEN);
  bscsReg = nullptr;

  if (clkReg)
    munmap ((void*)clkReg,  CLK_LEN);
  clkReg  = nullptr;

  if (dmaReg)
    munmap ((void*)dmaReg,  DMA_LEN);
  dmaReg  = nullptr;

  if (gpioReg)
    munmap ((void*)gpioReg, GPIO_LEN);
  gpioReg = nullptr;
  gpioClr0Reg = nullptr;
  gpioSet0Reg = nullptr;

  if (pcmReg)
    munmap ((void*)pcmReg,  PCM_LEN);
  pcmReg  = nullptr;

  if (pwmReg)
    munmap ((void*)pwmReg,  PWM_LEN);
  pwmReg  = nullptr;

  if (systReg)
    munmap ((void*)systReg, SYST_LEN);
  systReg = nullptr;

  if (spiReg)
    munmap ((void*)spiReg,  SPI_LEN);
  spiReg  = nullptr;
  //}}}

  if (dmaBus)
    munmap (dmaBus, PAGES_PER_BLOCK * (bufferBlocks + PI_WAVE_BLOCKS) * sizeof(dmaPage_t *));
  dmaBus = nullptr;

  if (dmaVirt) {
    for (int i = 0; i < PAGES_PER_BLOCK * ((int)bufferBlocks + PI_WAVE_BLOCKS); i++)
      munmap(dmaVirt[i], PAGE_SIZE);
    munmap (dmaVirt, PAGES_PER_BLOCK * (bufferBlocks + PI_WAVE_BLOCKS) * sizeof(dmaPage_t *));
    }
  dmaVirt = nullptr;

  if (dmaPMapBlk) {
    for (int i = 0; i < ((int)bufferBlocks + PI_WAVE_BLOCKS); i++)
      munmap(dmaPMapBlk[i], PAGES_PER_BLOCK * PAGE_SIZE);
    munmap(dmaPMapBlk, (bufferBlocks + PI_WAVE_BLOCKS) * sizeof(dmaPage_t *));
    }
  dmaPMapBlk = nullptr;

  if (dmaMboxBlk) {
    fdMbox = mbOpen();
    for (int i = 0; i < ((int)bufferBlocks + PI_WAVE_BLOCKS); i++)
      mbDMAFree (&dmaMboxBlk[bufferBlocks + PI_WAVE_BLOCKS - i - 1]);
    mbClose (fdMbox);
    munmap (dmaMboxBlk, (bufferBlocks + PI_WAVE_BLOCKS) * sizeof(DMAMem_t));
    }
  dmaMboxBlk = nullptr;

  if (fdMem != -1) {
    close (fdMem);
    fdMem = -1;
    }

  if (fdPmap != -1) {
    close (fdPmap);
    fdPmap = -1;
    }

  if (fdMbox != -1) {
    close (fdMbox);
    fdMbox = -1;
    }
  }
//}}}

//{{{
static int initInitialise() {

  PWMClockInited = 0;
  clock_gettime (CLOCK_REALTIME, &libStarted);
  uint32_t rev = gpioHardwareRevision();

  initClearGlobals();
  if (initCheckPermitted() < 0)
    return PI_INIT_FAILED;

  if (!gpioMaskSet) {
     //{{{  get gpioMask
     if (rev == 0)
       gpioMask = PI_DEFAULT_UPDATE_MASK_UNKNOWN;
     else if (rev < 4)
       gpioMask = PI_DEFAULT_UPDATE_MASK_B1;
     else if (rev < 16)
       gpioMask = PI_DEFAULT_UPDATE_MASK_A_B2;
     else if (rev == 17)
       gpioMask = PI_DEFAULT_UPDATE_MASK_COMPUTE;
     else if (rev < 20)
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
        uint32_t model = (rev >> 4) & 0xFF;
        if (model < 2)
          gpioMask = PI_DEFAULT_UPDATE_MASK_A_B2;
        else if (model < 4)
          gpioMask = PI_DEFAULT_UPDATE_MASK_APLUS_BPLUS;
        else if (model == 4)
         gpioMask = PI_DEFAULT_UPDATE_MASK_PI2B;
        else if (model == 6 || model == 10 || model == 16)
          gpioMask = PI_DEFAULT_UPDATE_MASK_COMPUTE;
        else if (model == 8 || model == 13 || model == 14)
          gpioMask = PI_DEFAULT_UPDATE_MASK_PI3B;
        else if (model == 9 || model == 12)
          gpioMask = PI_DEFAULT_UPDATE_MASK_ZERO;
        else if (model == 17)
          gpioMask = PI_DEFAULT_UPDATE_MASK_PI4B;
        else
          gpioMask = PI_DEFAULT_UPDATE_MASK_UNKNOWN;
       }

     gpioMaskSet = 1;
     }
     //}}}

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
    LOG_ERROR ("pthread_attr_init failed (%m)");
  if (pthread_attr_setstacksize (&pthAttr, STACK_SIZE))
    LOG_ERROR ("pthread_attr_setstacksize failed (%m)");

  myGpioDelay (1000);
  dmaInitCbs();
  flushMemory();

  // cast twice to suppress compiler warning,
  // I belive this cast is ok because dmaIBus contains bus addresses, not virtual addresses.
  initDMAgo ((uint32_t*)dmaIn, (uint32_t)(uintptr_t)dmaIBus[0]);

  return PIGPIO_VERSION;
  }
//}}}
//}}}

//{{{  time
//{{{
double timeTime() {

  struct timeval tv;
  gettimeofday (&tv, 0);
  return (double)tv.tv_sec + ((double)tv.tv_usec / 1E6);
  }
//}}}
//{{{
void timeSleep (double seconds) {

  struct timespec ts, rem;

  if (seconds > 0.0) {
    ts.tv_sec = seconds;
    ts.tv_nsec = (seconds - (double)ts.tv_sec) * 1E9;

    while (clock_nanosleep(CLOCK_REALTIME, 0, &ts, &rem)) {
      // copy remaining time to ts
      ts.tv_sec = rem.tv_sec;
      ts.tv_nsec = rem.tv_nsec;
      }
    }
  }
//}}}

//{{{
int gpioTime (uint32_t timetype, int* seconds, int* micros) {

  struct timespec ts;

  if (timetype == PI_TIME_ABSOLUTE) {
    clock_gettime (CLOCK_REALTIME, &ts);
    *seconds = ts.tv_sec;
    *micros = ts.tv_nsec/1000;
    }
  else {
    clock_gettime (CLOCK_REALTIME, &ts);
    TIMER_SUB (&ts, &libStarted, &ts);
    *seconds = ts.tv_sec;
    *micros = ts.tv_nsec/1000;
    }

   return 0;
  }
//}}}
//{{{
int gpioSleep (uint32_t timetype, int seconds, int micros) {

  struct timespec ts;
  ts.tv_sec = seconds;
  ts.tv_nsec = micros * 1000;

  struct timespec rem;
  if (timetype == PI_TIME_ABSOLUTE) {
    while (clock_nanosleep (CLOCK_REALTIME, TIMER_ABSTIME, &ts, &rem)) {}
    }
  else {
    while (clock_nanosleep (CLOCK_REALTIME, 0, &ts, &rem)) {
      // copy remaining time to ts
      ts.tv_sec = rem.tv_sec;
      ts.tv_nsec = rem.tv_nsec;
      }
    }

  return 0;
 }
//}}}

//{{{
uint32_t gpioDelay (uint32_t micros) {

  uint32_t start = systReg[SYST_CLO];

  if (micros <= PI_MAX_BUSY_DELAY)
    while ((systReg[SYST_CLO] - start) <= micros) {}
  else
    gpioSleep (PI_TIME_RELATIVE, (micros / 1000000), (micros % 1000000));

  return systReg[SYST_CLO] - start;
  }
//}}}
//{{{
uint32_t gpioTick() {
  return systReg[SYST_CLO];
  }
//}}}
//}}}

//{{{
uint32_t gpioHardwareRevision() {
//{{{  description
// 2 2  2  2 2 2  1 1 1 1  1 1 1 1  1 1 0 0 0 0 0 0  0 0 0 0
// 5 4  3  2 1 0  9 8 7 6  5 4 3 2  1 0 9 8 7 6 5 4  3 2 1 0
// W W  S  M M M  B B B B  P P P P  T T T T T T T T  R R R R
//
// W warranty void if either bit is set
//
// S 0 = old - bits 0-22 are revision number
//   1 = new - following fields apply
//
// M 0 = 256 1 = 512 2 = 1024 3 = 2GB 4 = 4GB
//
// B 0 = Sony 1 = Egoman 2 = Embest 3 = Sony Japan 4 = Embest 5 = Stadium
//
// P 0 = 2835
//   1 = 2836
//   2 = 2837
//   3 = 2711
//
// T 0 = A
//   1 = B
//   2 = A+
//   3 = B+
//   4 = Pi2B
//   5 = Alpha
//   6 = CM1
//   8 = Pi3B
//   9 = Zero a = CM3
//   c = Zero W
//   d = 3B+ e=3A+
//  10 = CM3+
//  11 = 4B
//
// R  PCB board revision
//}}}

  uint32_t rev = 0;

  FILE* file = fopen ("/proc/cpuinfo", "r");
  if (file != NULL) {
    char buf[512];
    while (fgets (buf, sizeof(buf), file) != NULL) {
      if (!strncasecmp ("revision\t:", buf, 10)) {
        char term;
        if (sscanf (buf+10, "%x%c", &rev, &term) == 2) {
          if (term != '\n')
            rev = 0;
          }
        }
      }
    fclose (file);
    }

  // (some) arm64 operating systems get revision number here
  if (rev == 0) {
    file = fopen ("/proc/device-tree/system/linux,revision", "r");
    if (file) {
      uint32_t tmp;
      if (fread (&tmp, 1, 4, file) == 4) {
        // for some reason the value returned by reading this /proc entry seems to be big endian, convert it.
        rev = ntohl (tmp);
        rev &= 0xFFFFFF; // mask out warranty bit
        }
      }
    fclose (file);
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
    else
      rev = 0;
    }

  else {
    // new rev code
    switch ((rev >> 12) & 0xF) { // just interested in BCM model
      //{{{
      case 0x0: // BCM2835
        pi_ispi = 1;
        piCores = 1;

        pi_peri_phys = 0x20000000;
        pi_dram_bus = 0x40000000;
        pi_mem_flag = 0x0C;

        break;
      //}}}
      case 0x1:      // BCM2836
      //{{{
      case 0x2: // BCM2837
        pi_ispi = 1;
        piCores = 4;

        pi_peri_phys = 0x3F000000;
        pi_dram_bus = 0xC0000000;
        pi_mem_flag = 0x04;

        break;
      //}}}
      //{{{
      case 0x3: // BCM2711
        pi_ispi = 1;
        piCores = 4;

        pi_peri_phys = 0xFE000000;
        pi_dram_bus = 0xC0000000;
        pi_mem_flag = 0x04;
        pi_is_2711 = 1;

        clk_osc_freq = CLK_OSC_FREQ_2711;
        clk_plld_freq = CLK_PLLD_FREQ_2711;

        hw_pwm_max_freq = PI_HW_PWM_MAX_FREQ_2711;
        hw_clk_min_freq = PI_HW_CLK_MIN_FREQ_2711;
        hw_clk_max_freq = PI_HW_CLK_MAX_FREQ_2711;
        break;
      //}}}
      //{{{
      default:
        cLog::log (LOGINFO, "unknown rev code %x", rev);
        rev = 0;
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
  if (dmaReg) {
    initKillDMA (dmaIn);
    initKillDMA (dmaOut);
    }

  initReleaseResources();
  fflush (NULL);
  }
//}}}

//{{{  gpio helpers
//{{{
static void myGpioSetMode (uint32_t gpio, uint32_t mode) {

  int reg =  gpio / 10;
  int shift = (gpio % 10) * 3;
  gpioReg[reg] = (gpioReg[reg] & ~(7 << shift)) | (mode << shift);
  }
//}}}
//{{{
static int myGpioRead (uint32_t gpio) {

  if ((*(gpioReg + GPLEV0 + BANK) & BIT) != 0)
    return PI_ON;
  else
    return PI_OFF;
  }
//}}}
//{{{
static void myGpioWrite (uint32_t gpio, uint32_t level) {

  if (level == PI_OFF)
    *(gpioReg + GPCLR0 + BANK) = BIT;
  else
    *(gpioReg + GPSET0 + BANK) = BIT;
  }
//}}}
//{{{
static void mySetGpioOff (uint32_t gpio, int pos) {

  int page, slot;
  myOffPageSlot (pos, &page, &slot);
  dmaIVirt[page]->gpioOff[slot] |= (1<<gpio);
  }
//}}}
//{{{
static void myClearGpioOff (uint32_t gpio, int pos) {

  int page, slot;
  myOffPageSlot (pos, &page, &slot);
  dmaIVirt[page]->gpioOff[slot] &= ~(1<<gpio);
  }
//}}}
//{{{
static void mySetGpioOn (uint32_t gpio, int pos) {

  int page = pos / ON_PER_IPAGE;
  int slot = pos % ON_PER_IPAGE;
  dmaIVirt[page]->gpioOn[slot] |= (1<<gpio);
  }
//}}}
//{{{
static void myClearGpioOn (uint32_t gpio, int pos) {

  int page = pos / ON_PER_IPAGE;
  int slot = pos % ON_PER_IPAGE;
  dmaIVirt[page]->gpioOn[slot] &= ~(1<<gpio);
  }
//}}}
//{{{
static void myGpioSetPwm (uint32_t gpio, int oldVal, int newVal) {

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
static void myGpioSetServo (uint32_t gpio, int oldVal, int newVal) {

  int newOff, oldOff, realRange, cycles, i;
  int deferOff, deferRng;

  realRange = pwmRealRange[clkCfg[gpioCfg.clockMicros].servoIdx];
  cycles    = pwmCycles   [clkCfg[gpioCfg.clockMicros].servoIdx];

  newOff = (newVal * realRange)/20000;
  oldOff = (oldVal * realRange)/20000;

  deferOff = gpioInfo[gpio].deferOff;
  deferRng = gpioInfo[gpio].deferRng;

  if (gpioInfo[gpio].deferOff) {
    for (i=0; i<SUPERLEVEL; i+=deferRng)
      myClearGpioOff (gpio, i+deferOff);
     gpioInfo[gpio].deferOff = 0;
    }

  if (newOff != oldOff) {
    if (newOff && oldOff)  {
      /* SERVO CHANGE */
      for (i = 0; i < SUPERLEVEL; i+=realRange)
        mySetGpioOff (gpio, i+newOff);

      if (newOff > oldOff) {
        for (i = 0; i < SUPERLEVEL; i+=realRange)
          myClearGpioOff (gpio, i+oldOff);
        }
      else {
        gpioInfo[gpio].deferOff = oldOff;
        gpioInfo[gpio].deferRng = realRange;
        }
      }
    else if (newOff) {
      /* SERVO START */
      for (i = 0; i < SUPERLEVEL; i+=realRange)
        mySetGpioOff (gpio, i+newOff);

      /* schedule new gpio on */
      for (i = 0; i < SUPERCYCLE; i+=cycles)
        mySetGpioOn(gpio, i);
      }
    else  {
      /* SERVO STOP */
      /* deschedule gpio on */
      for (i = 0; i < SUPERCYCLE; i+=cycles)
        myClearGpioOn (gpio, i);

      /* if in pulse then delay for the last cycle to complete */
      if (myGpioRead (gpio))
        myGpioDelay (PI_MAX_SERVO_PULSEWIDTH);

      /* deschedule gpio off */
      for (i = 0; i < SUPERLEVEL; i+=realRange)
        myClearGpioOff (gpio, i+oldOff);
      }
    }
  }
//}}}
//{{{
static void switchFunctionOff (uint32_t gpio) {

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
//}}}
//{{{
int gpioGetMode (uint32_t gpio) {

  int reg =  gpio / 10;
  int shift = (gpio % 10) * 3;
  return (gpioReg[reg] >> shift) & 7;
  }
//}}}
//{{{
void gpioSetMode (uint32_t gpio, uint32_t mode) {

  int reg =  gpio / 10;
  int shift = (gpio % 10) * 3;

  uint32_t old_mode = (gpioReg[reg] >> shift) & 7;
  if (mode != old_mode) {
    switchFunctionOff (gpio);
    gpioInfo[gpio].is = GPIO_UNDEFINED;
    }

  gpioReg[reg] = (gpioReg[reg] & ~(7<<shift)) | (mode<<shift);
  }
//}}}
//{{{
void gpioSetPullUpDown (uint32_t gpio, uint32_t pud) {

  int shift = (gpio & 0xf) << 1;

  if (pi_is_2711) {
    uint32_t pull = 0;
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
  }
//}}}
//{{{
int gpioGetPad (uint32_t pad) {

  int strength = padsReg[11+pad] & 7;
  strength *= 2;
  strength += 2;

  return strength;
  }
//}}}
//{{{
void gpioSetPad (uint32_t pad, uint32_t padStrength) {

  // 1-16 -> 0-7
  padStrength += 1;
  padStrength /= 2;
  padStrength -= 1;

  padsReg[11+pad] = BCM_PASSWD | 0x18 | (padStrength & 7) ;
  }
//}}}

//{{{
int gpioRead (uint32_t gpio) {

  if ((*(gpioReg + GPLEV0 + BANK) & BIT) != 0)
    return PI_ON;
  else
    return PI_OFF;
  }
//}}}
uint32_t gpioRead_Bits_0_31() { return (*(gpioReg + GPLEV0)); }

//{{{
void gpioWrite (uint32_t gpio, uint32_t level) {

  if (gpioInfo[gpio].is != GPIO_WRITE) {
    // stop a glitch between setting mode then level
    if (level == PI_OFF)
      *(gpioReg + GPCLR0 + BANK) = BIT;
    else
      *(gpioReg + GPSET0 + BANK) = BIT;

    switchFunctionOff (gpio);
    gpioInfo[gpio].is = GPIO_WRITE;
    }

  myGpioSetMode (gpio, PI_OUTPUT);

  if (level == PI_OFF)
    *(gpioReg + GPCLR0 + BANK) = BIT;
  else
    *(gpioReg + GPSET0 + BANK) = BIT;
  }
//}}}
void gpioWrite_Bits_0_31_Clear (uint32_t bits) { *gpioClr0Reg = bits; }
void gpioWrite_Bits_0_31_Set (uint32_t bits) { *gpioSet0Reg = bits; }

//{{{  gpio pwm
// pwm
//{{{
void gpioPWM (uint32_t gpio, uint32_t val) {

  if (gpioInfo[gpio].is != GPIO_PWM) {
    switchFunctionOff(gpio);
    gpioInfo[gpio].is = GPIO_PWM;
    if (!val)
      myGpioWrite(gpio, 0);
    }

  myGpioSetMode (gpio, PI_OUTPUT);
  myGpioSetPwm (gpio, gpioInfo[gpio].width, val);

  gpioInfo[gpio].width = val;
  }
//}}}
//{{{
int gpioGetPWMdutycycle (uint32_t gpio) {

  uint32_t pwm;

  switch (gpioInfo[gpio].is) {
    case GPIO_PWM:
      return gpioInfo[gpio].width;

    case GPIO_HW_PWM:
      pwm = (PWMDef[gpio] >> 4) & 3;
      return hw_pwm_duty[pwm];

    case GPIO_HW_CLK:
      return PI_HW_PWM_RANGE/2;

    default:
      LOG_ERROR ("not a PWM gpio (%d)", gpio);
    }
  }
//}}}

//{{{
void gpioSetPWMrange (uint32_t gpio, uint32_t range) {

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
  }
//}}}
//{{{
int gpioGetPWMrange (uint32_t gpio) {

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
int gpioGetPWMrealRange (uint32_t gpio) {

  uint32_t pwm;
  switch (gpioInfo[gpio].is) {
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
void gpioSetPWMfrequency (uint32_t gpio, uint32_t frequency) {

  int i, width;
  uint32_t diff, best, idx;

  if ((int)frequency > pwmFreq[0])
    idx = 0;
  else if ((int)frequency < pwmFreq[PWM_FREQS-1])
    idx = PWM_FREQS-1;
  else {
    best = 100000; /* impossibly high frequency difference */
    idx = 0;

    for (i=0; i<PWM_FREQS; i++) {
      if ((int)frequency > pwmFreq[i])
        diff = frequency - pwmFreq[i];
      else
        diff = pwmFreq[i] - frequency;

      if (diff < best) {
        best = diff;
        idx = i;
        }
      }
   }

  width = gpioInfo[gpio].width;
  if (width) {
    if (gpioInfo[gpio].is == GPIO_PWM) {
      myGpioSetPwm(gpio, width, 0);
      gpioInfo[gpio].freqIdx = idx;
      myGpioSetPwm(gpio, 0, width);
      }
    }

  gpioInfo[gpio].freqIdx = idx;
  }
//}}}
//{{{
int gpioGetPWMfrequency (uint32_t gpio) {

  uint32_t pwm, clock;
  switch (gpioInfo[gpio].is) {
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
void gpioServo (uint32_t gpio, uint32_t val) {

  if (gpioInfo[gpio].is != GPIO_SERVO) {
    switchFunctionOff (gpio);
    gpioInfo[gpio].is = GPIO_SERVO;
    if (!val)
      myGpioWrite (gpio, 0);
    }

  myGpioSetMode (gpio, PI_OUTPUT);
  myGpioSetServo (gpio, gpioInfo[gpio].width, val);
  gpioInfo[gpio].width = val;
  }
//}}}
int gpioGetServoPulsewidth (uint32_t gpio) { return gpioInfo[gpio].width; }
//}}}

//{{{  spi
// hw helpers
//{{{
static int spiAnyOpen (uint32_t flags) {

  uint32_t aux = PI_SPI_FLAGS_GET_AUX_SPI (flags);

  for (int i = 0; i < PI_SPI_SLOTS; i++)
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

  if (PI_SPI_FLAGS_GET_AUX_SPI (flags)) {
    // enable module and access to registers
    auxReg[AUX_ENABLES] |= AUXENB_SPI1;

    // save original state
    old_mode_ace0 = gpioGetMode (PI_ASPI_CE0);
    old_mode_ace1 = gpioGetMode (PI_ASPI_CE1);
    old_mode_ace2 = gpioGetMode (PI_ASPI_CE2);
    old_mode_asclk = gpioGetMode (PI_ASPI_SCLK);
    old_mode_amiso = gpioGetMode (PI_ASPI_MISO);
    old_mode_amosi = gpioGetMode (PI_ASPI_MOSI);
    old_spi_cntl0 = auxReg[AUX_SPI0_CNTL0_REG];
    old_spi_cntl1 = auxReg[AUX_SPI0_CNTL1_REG];

    // manually control auxiliary SPI chip selects
    if (!(resvd & 1)) {
      myGpioSetMode (PI_ASPI_CE0, PI_OUTPUT);
      myGpioWrite (PI_ASPI_CE0, !(cspols & 1));
      }

    if (!(resvd & 2)) {
      myGpioSetMode (PI_ASPI_CE1, PI_OUTPUT);
      myGpioWrite (PI_ASPI_CE1, !(cspols & 2));
      }

    if (!(resvd & 4)) {
      myGpioSetMode (PI_ASPI_CE2, PI_OUTPUT);
      myGpioWrite (PI_ASPI_CE2, !(cspols & 4));
      }

    // set gpios to SPI mode
    myGpioSetMode (PI_ASPI_SCLK, PI_ALT4);
    myGpioSetMode (PI_ASPI_MISO, PI_ALT4);
    myGpioSetMode (PI_ASPI_MOSI, PI_ALT4);
    }

  else {
    // save original state
    old_mode_ce0 = gpioGetMode (PI_SPI_CE0);
    old_mode_ce1 = gpioGetMode (PI_SPI_CE1);
    old_mode_sclk = gpioGetMode (PI_SPI_SCLK);
    old_mode_miso = gpioGetMode (PI_SPI_MISO);
    old_mode_mosi = gpioGetMode (PI_SPI_MOSI);
    old_spi_cs = spiReg[SPI_CS];
    old_spi_clk = spiReg[SPI_CLK];

    // set gpios to SPI mode
    if (!(resvd & 1))
      myGpioSetMode (PI_SPI_CE0, PI_ALT0);
    if (!(resvd & 2))
      myGpioSetMode (PI_SPI_CE1, PI_ALT0);

    myGpioSetMode (PI_SPI_SCLK, PI_ALT0);
    myGpioSetMode (PI_SPI_MISO, PI_ALT0);
    myGpioSetMode (PI_SPI_MOSI, PI_ALT0);
    }
  }
//}}}

//{{{
static void spiGoS (uint32_t speed, uint32_t flags, char* txBuf, char* rxBuf, uint32_t count) {

  uint32_t channel = PI_SPI_FLAGS_GET_CHANNEL (flags);
  uint32_t mode   =  PI_SPI_FLAGS_GET_MODE (flags);
  uint32_t cspols =  PI_SPI_FLAGS_GET_CSPOLS (flags);
  uint32_t cspol  =  (cspols >> channel) & 1;
  uint32_t flag3w =  PI_SPI_FLAGS_GET_3WIRE (flags);
  uint32_t ren3w  =  PI_SPI_FLAGS_GET_3WREN (flags);

  uint32_t spiDefaults = SPI_CS_MODE(mode) | SPI_CS_CSPOLS(cspols) |
                         SPI_CS_CS(channel) | SPI_CS_CSPOL(cspol) | SPI_CS_CLEAR(3);

  // undocumented, stops inter-byte gap
  spiReg[SPI_DLEN] = 2;

  // stop
  spiReg[SPI_CS] = spiDefaults;

  if (!count)
    return;

  uint32_t cnt4w;
  uint32_t cnt3w;
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

  uint32_t cnt = cnt4w;
  uint32_t txCnt = 0;
  uint32_t rxCnt = 0;
  while ((txCnt < cnt) || (rxCnt < cnt)) {
    while ((rxCnt < cnt) && ((spiReg[SPI_CS] & SPI_CS_RXD))) {
      if (rxBuf)
        rxBuf[rxCnt] = spiReg[SPI_FIFO];
      else
        spiDummy = spiReg[SPI_FIFO];
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
        spiDummy = spiReg[SPI_FIFO];
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
static uint32_t spiTXBits (char* buf, int pos, int bitlen, int msbf) {

  uint32_t bits = 0;
  if (buf) {
    if (bitlen <=  8)
      bits = *((( uint8_t*)buf) + pos);
    else if (bitlen <= 16)
      bits = *(((uint16_t*)buf) + pos);
    else
      bits = *(((uint32_t*)buf) + pos);

    if (msbf)
      bits <<= (32 - bitlen);
    }

  return bits;
  }
//}}}
//{{{
static void spiRXBits (char *buf, int pos, int bitlen, int msbf, uint32_t bits) {

  if (buf) {
    if (!msbf)
      bits >>= (32-bitlen);

    if (bitlen <=  8)
      *((( uint8_t*)buf) + pos) = bits;
    else if (bitlen <= 16)
      *(((uint16_t*)buf) + pos) = bits;
    else
      *(((uint32_t*)buf) + pos) = bits;
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
static void spiGoA (uint32_t speed, uint32_t flags, char* txBuf, char* rxBuf, uint32_t count) {

  char bit_ir[4] = {1, 0, 0, 1}; // read on rising edge
  char bit_or[4] = {0, 1, 1, 0}; // write on rising edge
  char bit_ic[4] = {0, 0, 1, 1}; // invert clock

  int channel = PI_SPI_FLAGS_GET_CHANNEL (flags);
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
  int cs = PI_SPI_FLAGS_GET_CSPOLS (flags) & (1 << channel);

  uint32_t spiDefaults = AUXSPI_CNTL0_SPEED ((125000000 / speed) - 1)|
                         AUXSPI_CNTL0_IN_RISING (bit_ir[mode])  |
                         AUXSPI_CNTL0_OUT_RISING (bit_or[mode]) |
                         AUXSPI_CNTL0_INVERT_CLK (bit_ic[mode]) |
                         AUXSPI_CNTL0_MSB_FIRST (txmsbf)        |
                         AUXSPI_CNTL0_SHIFT_LEN (bitlen);

  if (!count) {
    auxReg[AUX_SPI0_CNTL0_REG] = AUXSPI_CNTL0_ENABLE | AUXSPI_CNTL0_CLR_FIFOS;
    myGpioDelay (10);
    auxReg[AUX_SPI0_CNTL0_REG] = AUXSPI_CNTL0_ENABLE | spiDefaults;
    auxReg[AUX_SPI0_CNTL1_REG] = AUXSPI_CNTL1_MSB_FIRST (rxmsbf);
    return;
    }

  auxReg[AUX_SPI0_CNTL0_REG] = AUXSPI_CNTL0_ENABLE | spiDefaults;
  auxReg[AUX_SPI0_CNTL1_REG] = AUXSPI_CNTL1_MSB_FIRST (rxmsbf);
  spiACS(channel, cs);

  uint32_t txCnt = 0;
  uint32_t rxCnt = 0;
  while ((txCnt < count) || (rxCnt < count)) {
     uint32_t statusReg = auxReg[AUX_SPI0_STAT_REG];
     int rxEmpty = statusReg & AUXSPI_STAT_RX_EMPTY;
     int txFull = (((statusReg >> 28) & 15) > 2);
     if (rxCnt < count)
       if (!rxEmpty)
         spiRXBits (rxBuf, rxCnt++, bitlen, rxmsbf, auxReg[AUX_SPI0_IO_REG]);

     if (txCnt < count) {
       if (!txFull) {
         if (txCnt != (count - 1))
           auxReg[AUX_SPI0_TX_HOLD] = spiTXBits (txBuf, txCnt++, bitlen, txmsbf);
         else
           auxReg[AUX_SPI0_IO_REG] = spiTXBits (txBuf, txCnt++, bitlen, txmsbf);
         }
      }
    }

  while (auxReg[AUX_SPI0_STAT_REG] & AUXSPI_STAT_BUSY) {}
  spiACS (channel, !cs);
  }
//}}}
//{{{
static void spiGo (uint32_t speed, uint32_t flags, char* txBuf, char* rxBuf, uint32_t count) {

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
    if (!(resvd & 1))
      myGpioSetMode (PI_ASPI_CE0, old_mode_ace0);
    if (!(resvd & 2))
      myGpioSetMode (PI_ASPI_CE1, old_mode_ace1);
    if (!(resvd & 4))
      myGpioSetMode (PI_ASPI_CE2, old_mode_ace2);

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
int spiOpen (uint32_t spiChan, uint32_t baud, uint32_t spiFlags) {

  static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

  int i;
  if (PI_SPI_FLAGS_GET_AUX_SPI (spiFlags))
    i = PI_NUM_AUX_SPI_CHANNEL;
  else
    i = PI_NUM_STD_SPI_CHANNEL;

  if (!spiAnyOpen (spiFlags)) {
    // initialise on first open
    spiInit (spiFlags);
    spiGo (baud, spiFlags, NULL, NULL, 0);
    }

  int slot = -1;
  pthread_mutex_lock (&mutex);
  for (i = 0; i < PI_SPI_SLOTS; i++) {
    if (spiInfo[i].state == PI_SPI_CLOSED) {
      slot = i;
      spiInfo[slot].state = PI_SPI_RESERVED;
      break;
      }
    }
  pthread_mutex_unlock (&mutex);

  if (slot < 0)
    LOG_ERROR ("no SPI handles");

  spiInfo[slot].speed = baud;
  spiInfo[slot].flags = spiFlags | PI_SPI_FLAGS_CHANNEL (spiChan);
  spiInfo[slot].state = PI_SPI_OPENED;

  return slot;
  }
//}}}
//{{{
int spiRead (uint32_t handle, char* buf, uint32_t count) {

  spiGo (spiInfo[handle].speed, spiInfo[handle].flags, NULL, buf, count);
  return count;
  }
//}}}
//{{{
int spiWrite (uint32_t handle, char* buf, uint32_t count) {

  if (PI_SPI_FLAGS_GET_AUX_SPI (spiInfo[handle].flags))
    spiGoA (spiInfo[handle].speed, spiInfo[handle].flags, buf, NULL, count);
  else
    spiGoS (spiInfo[handle].speed, spiInfo[handle].flags, buf, NULL, count);

  return count;
  }
//}}}
//{{{
void spiWriteMainFast (const uint8_t* buf, uint32_t count) {
// write single byte or 16bit frameBuffer using main hw cs
// - assumes no other use of spi main since spiOpen

  spiReg[SPI_CS] |= SPI_CS_TA;

  if (count == 1) {
    // single byte
    spiReg[SPI_FIFO] = *buf;
    while (!(spiReg[SPI_CS] & SPI_CS_RXD)) {}
    spiDummyRead = spiReg[SPI_FIFO];
    }

  else {
    // multiple bytes, byteSwap words
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
void spiWriteAuxFast (const uint8_t* buf, uint32_t count) {
// write single byte or 16bit frameBuffer using aux sw cs2
// - assumes no other use of spi aux since spiOpen

  auxReg[AUX_SPI0_CNTL0_REG] |= AUXSPI_CNTL0_ENABLE;
  myGpioWrite (PI_ASPI_CE2, 0);

  if (count == 1) {
    // single byte
    auxReg[AUX_SPI0_IO_REG] = (*buf) << 24;
    while (auxReg[AUX_SPI0_STAT_REG] & AUXSPI_STAT_RX_EMPTY) {}
    spiDummy = auxReg[AUX_SPI0_IO_REG];
    }

  else {
    uint32_t txCount = 0;
    uint32_t rxCount = 0;
    while ((txCount < count) || (rxCount < count)) {
      uint32_t statusReg = auxReg[AUX_SPI0_STAT_REG];

      if (rxCount < count)
        if (!(statusReg & AUXSPI_STAT_RX_EMPTY)) {
          // rx not empty, read
          spiDummy = auxReg[AUX_SPI0_IO_REG];
          rxCount++;
          }

      if (txCount < count) {
        if (((statusReg >> 28) & 15) <= 2) {
          // tx not full, write
          uint32_t bits;
          if (txCount & 1) {
            bits = (*buf) << 24;
            buf += 2;
            }
          else
            bits = (*(buf+1)) << 24;
          txCount++;
          //if (txCount == count) // last byte, hw cs deassert
            auxReg[AUX_SPI0_IO_REG] = bits;
          //else // not last byte, hw cs keep cs asserted
          //  auxReg[AUX_SPI0_TX_HOLD] = bits;
          }
        }
      }
    }

  while (auxReg[AUX_SPI0_STAT_REG] & AUXSPI_STAT_BUSY) {}
  myGpioWrite (PI_ASPI_CE2, 1);
  }
//}}}
//{{{
int spiXfer (uint32_t handle, char* txBuf, char* rxBuf, uint32_t count) {

  spiGo (spiInfo[handle].speed, spiInfo[handle].flags, txBuf, rxBuf, count);
  return count;
  }
//}}}
//{{{
int spiClose (uint32_t handle) {

  spiInfo[handle].state = PI_SPI_CLOSED;
  if (!spiAnyOpen (spiInfo[handle].flags))
    // terminate on last close
    spiTerminate (spiInfo[handle].flags);

  return 0;
  }
//}}}

// bitbang helpers
//{{{
static void wfRx_lock (int i) {
  pthread_mutex_lock (&wfRx[i].mutex);
  }
//}}}
//{{{
static void wfRx_unlock (int i) {
  pthread_mutex_unlock (&wfRx[i].mutex);
  }
//}}}
//{{{
static void set_CS (wfRx_t* w) {
  myGpioWrite (w->S.CS, PI_SPI_FLAGS_GET_CSPOL(w->S.spiFlags));
  }
//}}}
//{{{
static void clear_CS (wfRx_t* w) {
  myGpioWrite(w->S.CS, !PI_SPI_FLAGS_GET_CSPOL(w->S.spiFlags));
  }
//}}}
//{{{
static void set_SCLK (wfRx_t* w) {
  myGpioWrite (w->S.SCLK, !PI_SPI_FLAGS_GET_CPOL (w->S.spiFlags));
  }
//}}}
//{{{
static void clear_SCLK (wfRx_t* w) {
  myGpioWrite (w->S.SCLK, PI_SPI_FLAGS_GET_CPOL (w->S.spiFlags));
  }
//}}}
//{{{
static void SPI_delay (wfRx_t* w) {
  myGpioDelay (w->S.delay);
  }
//}}}
//{{{
static void bbSPIStart (wfRx_t* w) {

  clear_SCLK(w);
  SPI_delay(w);
  set_CS(w);
  SPI_delay(w);
  }
//}}}
//{{{
static void bbSPIStop (wfRx_t* w) {

  SPI_delay(w);
  clear_CS(w);
  SPI_delay(w);
  clear_SCLK(w);
  }
//}}}
//{{{
static uint8_t bbSPIXferByte (wfRx_t* w, char txByte) {

  uint8_t bit;
  uint8_t rxByte = 0;

  if (PI_SPI_FLAGS_GET_CPHA(w->S.spiFlags)) {
    // CPHA = 1 write on set clock read on clear clock
    for (bit = 0; bit < 8; bit++) {
      set_SCLK(w);

      if (PI_SPI_FLAGS_GET_TX_LSB(w->S.spiFlags)) {
        myGpioWrite (w->S.MOSI, txByte & 0x01);
        txByte >>= 1;
        }
      else {
        myGpioWrite (w->S.MOSI, txByte & 0x80);
        txByte <<= 1;
        }

      SPI_delay (w);
      clear_SCLK (w);
      if (PI_SPI_FLAGS_GET_RX_LSB (w->S.spiFlags))
        rxByte = (rxByte >> 1) | myGpioRead (w->S.MISO) << 7;
      else
        rxByte = (rxByte << 1) | myGpioRead (w->S.MISO);
      SPI_delay(w);
      }
    }

  else {
    // CPHA = 0 read on set clock write on clear clock
    for (bit=0; bit<8; bit++) {
      if (PI_SPI_FLAGS_GET_TX_LSB (w->S.spiFlags)) {
        myGpioWrite (w->S.MOSI, txByte & 0x01);
        txByte >>= 1;
        }
      else {
        myGpioWrite (w->S.MOSI, txByte & 0x80);
        txByte <<= 1;
        }

      SPI_delay(w);
      set_SCLK(w);
      if (PI_SPI_FLAGS_GET_RX_LSB (w->S.spiFlags))
        rxByte = (rxByte >> 1) | myGpioRead (w->S.MISO) << 7;
      else
        rxByte = (rxByte << 1) | myGpioRead (w->S.MISO);

      SPI_delay (w);
      clear_SCLK (w);
      }
    }

  return rxByte;
  }
//}}}

// external bitBang
//{{{
int bbSPIOpen (uint32_t CS, uint32_t MISO, uint32_t MOSI, uint32_t SCLK, uint32_t baud, uint32_t spiFlags) {

  int valid;
  uint32_t bits;
  valid = 0;

  // check all GPIO unique
  bits = (1 << CS) | (1 << MISO) | (1 << MOSI) | (1 << SCLK);

  if (__builtin_popcount (bits) == 4) {
    if ((wfRx[MISO].mode == PI_WFRX_NONE) &&
        (wfRx[MOSI].mode == PI_WFRX_NONE) &&
        (wfRx[SCLK].mode == PI_WFRX_NONE)) {
      valid = 1; // first time GPIO used for SPI
      }
    else {
      if ((wfRx[MISO].mode == PI_WFRX_SPI_MISO) &&
          (wfRx[MOSI].mode == PI_WFRX_SPI_MOSI) &&
          (wfRx[SCLK].mode == PI_WFRX_SPI_SCLK)) {
        valid = 2; // new CS for existing SPI GPIO
        }
      }
    }

  if (!valid) {
    LOG_ERROR ( "GPIO already being used (%d=%d %d=%d, %d=%d %d=%d)",
         CS,   wfRx[CS].mode, MISO, wfRx[MISO].mode, MOSI, wfRx[MOSI].mode, SCLK, wfRx[SCLK].mode);
    }

  wfRx[CS].mode = PI_WFRX_SPI_CS;
  wfRx[CS].baud = baud;
  wfRx[CS].S.CS = CS;
  wfRx[CS].S.SCLK = SCLK;
  wfRx[CS].S.CSMode = gpioGetMode(CS);
  wfRx[CS].S.delay = (500000 / baud) - 1;
  wfRx[CS].S.spiFlags = spiFlags;

  // preset CS to off
  if (PI_SPI_FLAGS_GET_CSPOL(spiFlags))
    gpioWrite(CS, 0); // active high
  else
    gpioWrite(CS, 1); // active low

  // The SCLK entry is used to store full information
  if (valid == 1) {
    // first time GPIO for SPI
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
    gpioWrite(MOSI, 0); // low output
    }
  else
    wfRx[SCLK].S.usage++;

  return 0;
  }
//}}}
//{{{
int bbSPIXfer (uint32_t CS, char* inBuf, char* outBuf, uint32_t count) {

  int SCLK = wfRx[CS].S.SCLK;
  wfRx[SCLK].S.CS = CS;
  wfRx[SCLK].baud = wfRx[CS].baud;
  wfRx[SCLK].S.delay = wfRx[CS].S.delay;
  wfRx[SCLK].S.spiFlags = wfRx[CS].S.spiFlags;

  wfRx_t* w = &wfRx[SCLK];

  wfRx_lock (SCLK);

  bbSPIStart (w);
  for (int pos = 0; pos < (int)count; pos++)
    outBuf[pos] = bbSPIXferByte(w, inBuf[pos]);
  bbSPIStop(w);

  wfRx_unlock(SCLK);

  return count;
  }
//}}}
//{{{
int bbSPIClose (uint32_t CS) {

  int SCLK;
  switch(wfRx[CS].mode) {
    case PI_WFRX_SPI_CS:
      myGpioSetMode(wfRx[CS].S.CS, wfRx[CS].S.CSMode);
      wfRx[CS].mode = PI_WFRX_NONE;

      SCLK = wfRx[CS].S.SCLK;
      if (--wfRx[SCLK].S.usage <= 0) {
        myGpioSetMode(wfRx[SCLK].S.MISO, wfRx[SCLK].S.MISOMode);
        myGpioSetMode(wfRx[SCLK].S.MOSI, wfRx[SCLK].S.MOSIMode);
        myGpioSetMode(wfRx[SCLK].S.SCLK, wfRx[SCLK].S.SCLKMode);

        wfRx[wfRx[SCLK].S.MISO].mode = PI_WFRX_NONE;
        wfRx[wfRx[SCLK].S.MOSI].mode = PI_WFRX_NONE;
        wfRx[wfRx[SCLK].S.SCLK].mode = PI_WFRX_NONE;
        }
      break;

    default:
      LOG_ERROR ("no SPI on gpio (%d)", CS);
      break;
    }

  return 0;
  }
//}}}
//}}}
//{{{  i2c
// hw helpers
//{{{
static int myI2CGetPar (char* inBuf, int* inPos, int inLen, int* esc) {

  int bytes;
  if (*esc)
    bytes = 2;
  else
    bytes = 1;

  *esc = 0;

  if (*inPos <= (inLen - bytes)) {
    if (bytes == 1)
      return inBuf[(*inPos)++];
    else {
      (*inPos) += 2;
      return inBuf[*inPos-2] + (inBuf[*inPos-1]<<8);
      }
    }

  return -1;
  }
//}}}
//{{{
static int mySmbusAccess (int fd, char rw, uint8_t cmd, int size, union my_smbus_data* data) {

  struct my_smbus_ioctl_data args;

  args.read_write = rw;
  args.command = cmd;
  args.size = size;
  args.data = data;

  return ioctl (fd, PI_I2C_SMBUS, &args);
  }
//}}}

// external hw
//{{{
int i2cWriteQuick (uint32_t handle, uint32_t bit) {

  int status = mySmbusAccess (i2cInfo[handle].fd, bit, 0, PI_I2C_SMBUS_QUICK, NULL);
  if (status < 0)
    return PI_I2C_WRITE_FAILED;

  return status;
  }
//}}}

//{{{
int i2cReadByte (uint32_t handle) {

  union my_smbus_data data;
  int status = mySmbusAccess (i2cInfo[handle].fd, PI_I2C_SMBUS_READ, 0, PI_I2C_SMBUS_BYTE, &data);

  if (status < 0)
   return PI_I2C_READ_FAILED;

  return 0xFF & data.byte;
  }
//}}}
//{{{
int i2cWriteByte (uint32_t handle, uint32_t bVal) {

  int status = mySmbusAccess (i2cInfo[handle].fd, PI_I2C_SMBUS_WRITE, bVal, PI_I2C_SMBUS_BYTE, NULL);
  if (status < 0)
    return PI_I2C_WRITE_FAILED;

  return status;
  }
//}}}

//{{{
int i2cReadByteData (uint32_t handle, uint32_t reg) {

  union my_smbus_data data;
  int status = mySmbusAccess (i2cInfo[handle].fd, PI_I2C_SMBUS_READ, reg, PI_I2C_SMBUS_BYTE_DATA, &data);
  if (status < 0)
    return PI_I2C_READ_FAILED;

  return 0xFF & data.byte;
  }
//}}}
//{{{
int i2cWriteByteData (uint32_t handle, uint32_t reg, uint32_t bVal) {

  union my_smbus_data data;

  data.byte = bVal;

  int status = mySmbusAccess (i2cInfo[handle].fd, PI_I2C_SMBUS_WRITE, reg, PI_I2C_SMBUS_BYTE_DATA, &data);
  if (status < 0)
    return PI_I2C_WRITE_FAILED;

  return status;
  }
//}}}

//{{{
int i2cReadWordData (uint32_t handle, uint32_t reg) {

  union my_smbus_data data;
  int status = (mySmbusAccess (i2cInfo[handle].fd, PI_I2C_SMBUS_READ, reg, PI_I2C_SMBUS_WORD_DATA, &data));
  if (status < 0)
    return PI_I2C_READ_FAILED;

  return 0xFFFF & data.word;
  }
//}}}
//{{{
int i2cWriteWordData (uint32_t handle, uint32_t reg, uint32_t wVal) {

  union my_smbus_data data;
  data.word = wVal;
  int status = mySmbusAccess (i2cInfo[handle].fd, PI_I2C_SMBUS_WRITE, reg, PI_I2C_SMBUS_WORD_DATA, &data);
  if (status < 0)
     return PI_I2C_WRITE_FAILED;

  return status;
  }
//}}}

//{{{
int i2cProcessCall (uint32_t handle, uint32_t reg, uint32_t wVal) {

  union my_smbus_data data;
  data.word = wVal;
  int status = mySmbusAccess (i2cInfo[handle].fd, PI_I2C_SMBUS_WRITE, reg, PI_I2C_SMBUS_PROC_CALL, &data);
  if (status < 0)
    return PI_I2C_READ_FAILED;

  return 0xFFFF & data.word;
  }
//}}}

//{{{
int i2cReadBlockData (uint32_t handle, uint32_t reg, char* buf) {

  union my_smbus_data data;
  int status = mySmbusAccess (i2cInfo[handle].fd, PI_I2C_SMBUS_READ, reg, PI_I2C_SMBUS_BLOCK_DATA, &data);
  if (status < 0)
    return PI_I2C_READ_FAILED;
  else {
    if (data.block[0] <= PI_I2C_SMBUS_BLOCK_MAX) {
      for (int i = 0; i < data.block[0]; i++) buf[i] = data.block[i+1];
      return data.block[0];
      }
    else
      return PI_I2C_READ_FAILED;
    }
  }
//}}}
//{{{
int i2cWriteBlockData (uint32_t handle, uint32_t reg, char* buf, uint32_t count) {

  union my_smbus_data data;
  for (int i = 1; i <= (int)count; i++)
    data.block[i] = buf[i-1];
  data.block[0] = count;

  int status = mySmbusAccess (i2cInfo[handle].fd, PI_I2C_SMBUS_WRITE, reg, PI_I2C_SMBUS_BLOCK_DATA, &data);
  if (status < 0)
    return PI_I2C_WRITE_FAILED;

  return status;
  }
//}}}

//{{{
int i2cBlockProcessCall (uint32_t handle, uint32_t reg, char* buf, uint32_t count) {

  union my_smbus_data data;
  for (int i = 1; i <= (int)count; i++)
    data.block[i] = buf[i-1];
  data.block[0] = count;

  int status = mySmbusAccess (i2cInfo[handle].fd, PI_I2C_SMBUS_WRITE, reg, PI_I2C_SMBUS_BLOCK_PROC_CALL, &data);
  if (status < 0)
    return PI_I2C_READ_FAILED;
  else {
    if (data.block[0] <= PI_I2C_SMBUS_BLOCK_MAX) {
      for (int i = 0; i < data.block[0]; i++)
        buf[i] = data.block[i+1];
      return data.block[0];
      }
    else
      return PI_I2C_READ_FAILED;
    }
  }
//}}}

//{{{
int i2cReadI2CBlockData (uint32_t handle, uint32_t reg, char* buf, uint32_t count) {

  uint32_t size = count == 32 ? PI_I2C_SMBUS_I2C_BLOCK_BROKEN : PI_I2C_SMBUS_I2C_BLOCK_DATA;

  union my_smbus_data data;
  data.block[0] = count;

  int status = mySmbusAccess (i2cInfo[handle].fd, PI_I2C_SMBUS_READ, reg, size, &data);
  if (status < 0)
     return PI_I2C_READ_FAILED;

  else {
    if (data.block[0] <= PI_I2C_SMBUS_I2C_BLOCK_MAX) {
      for (int i = 0; i < data.block[0]; i++)
        buf[i] = data.block[i+1];
      return data.block[0];
      }
    else
      return PI_I2C_READ_FAILED;
    }
  }
//}}}
//{{{
int i2cWriteI2CBlockData (uint32_t handle, uint32_t reg, char* buf, uint32_t count) {

  union my_smbus_data data;
  for (int i = 1; i <= (int)count; i++)
    data.block[i] = buf[i-1];
  data.block[0] = count;

  int status = mySmbusAccess (i2cInfo[handle].fd, PI_I2C_SMBUS_WRITE, reg, PI_I2C_SMBUS_I2C_BLOCK_BROKEN, &data);
  if (status < 0)
    return PI_I2C_WRITE_FAILED;

  return status;
  }
//}}}

//{{{
int i2cWriteDevice (uint32_t handle, char* buf, uint32_t count) {

  int bytes = write (i2cInfo[handle].fd, buf, count);
  if (bytes != (int)count)
    return PI_I2C_WRITE_FAILED;

  return 0;
  }
//}}}
//{{{
int i2cReadDevice (uint32_t handle, char* buf, uint32_t count) {

  int bytes = read (i2cInfo[handle].fd, buf, count);
  if (bytes != (int)count)
    return PI_I2C_READ_FAILED;

  return bytes;
  }
//}}}

//{{{
int i2cOpen (uint32_t i2cBus, uint32_t i2cAddr, uint32_t i2cFlags) {

  static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

  char dev[32];
  int fd;
  uint32_t funcs;
  int slot = -1;

  int i;
  pthread_mutex_lock (&mutex);
  for (i = 0; i < PI_I2C_SLOTS; i++) {
    if (i2cInfo[i].state == PI_I2C_CLOSED) {
      slot = i;
      i2cInfo[slot].state = PI_I2C_RESERVED;
      break;
      }
    }
  pthread_mutex_unlock (&mutex);

  if (slot < 0)
    LOG_ERROR ("no I2C handles");

  sprintf (dev, "/dev/i2c-%d", i2cBus);
  if ((fd = open(dev, O_RDWR)) < 0) {
    /* try a modprobe */
    if (system ("/sbin/modprobe i2c_dev") == -1) { /* ignore errors */}
    if (system ("/sbin/modprobe i2c_bcm2835") == -1) { /* ignore errors */}

    myGpioDelay (100000);

    if ((fd = open (dev, O_RDWR)) < 0) {
      i2cInfo[slot].state = PI_I2C_CLOSED;
      return PI_BAD_I2C_BUS;
      }
    }

  if (ioctl (fd, PI_I2C_SLAVE, i2cAddr) < 0) {
    close (fd);
    i2cInfo[slot].state = PI_I2C_CLOSED;
    return PI_I2C_OPEN_FAILED;
     }

  if (ioctl (fd, PI_I2C_FUNCS, &funcs) < 0)
    funcs = -1; /* assume all smbus commands allowed */

  i2cInfo[slot].fd = fd;
  i2cInfo[slot].addr = i2cAddr;
  i2cInfo[slot].flags = i2cFlags;
  i2cInfo[slot].funcs = funcs;
  i2cInfo[i].state = PI_I2C_OPENED;

  return slot;
  }
//}}}
//{{{
int i2cClose (uint32_t handle) {

  if (i2cInfo[handle].fd >= 0)
    close (i2cInfo[handle].fd);

  i2cInfo[handle].fd = -1;
  i2cInfo[handle].state = PI_I2C_CLOSED;

  return 0;
  }
//}}}

//{{{
void i2cSwitchCombined (int setting) {

  int fd = open ("/sys/module/i2c_bcm2708/parameters/combined", O_WRONLY);
  if (fd >= 0) {
    if (setting) {
      if (write (fd, "1\n", 2) == -1) {}
      }
    else {
      if (write (fd, "0\n", 2) == -1) {}
      }

    close(fd);
    }
  }
//}}}
//{{{
int i2cSegments (uint32_t handle, pi_i2c_msg_t *segs, uint32_t numSegs) {

  my_i2c_rdwr_ioctl_data_t rdwr;
  rdwr.msgs = segs;
  rdwr.nmsgs = numSegs;
  int retval = ioctl (i2cInfo[handle].fd, PI_I2C_RDWR, &rdwr);

  if (retval >= 0)
    return retval;
  else
    return PI_BAD_I2C_SEG;
  }
//}}}
//{{{
int i2cZip (uint32_t handle, char *inBuf, uint32_t inLen, char *outBuf, uint32_t outLen) {

  int bytes;
  int numSegs = 0;
  int inPos = 0;
  int outPos = 0;
  int status = 0;
  int addr = i2cInfo[handle].addr;
  int flags = 0;
  int esc = 0;
  int setesc = 0;
  pi_i2c_msg_t segs[PI_I2C_RDRW_IOCTL_MAX_MSGS];

  while (!status && (inPos < (int)inLen)) {
    switch (inBuf[inPos++]) {
      //{{{
      case PI_I2C_END:
        status = 1;
        break;
      //}}}
      //{{{
      case PI_I2C_COMBINED_ON:
        // Run prior transactions before setting combined flag
        if (numSegs) {
          status = i2cSegments (handle, segs, numSegs);
          if (status >= 0)
            status = 0; //* continue
          numSegs = 0;
          }

        i2cSwitchCombined(1);
        break;
      //}}}
      //{{{
      case PI_I2C_COMBINED_OFF:
        // Run prior transactions before clearing combined flag
        if (numSegs) {
          status = i2cSegments (handle, segs, numSegs);
          if (status >= 0)
            status = 0; // continue
          numSegs = 0;
          }
        i2cSwitchCombined(0);
        break;
      //}}}
      //{{{
      case PI_I2C_ADDR:
        addr = myI2CGetPar (inBuf, &inPos, inLen, &esc);
        if (addr < 0)
          status = PI_BAD_I2C_CMD;
        break;
      //}}}
      //{{{
      case PI_I2C_FLAGS:
        // cheat to force two byte flags
        esc = 1;
        flags = myI2CGetPar(inBuf, &inPos, inLen, &esc);
        if (flags < 0)
          status = PI_BAD_I2C_CMD;
        break;
      //}}}
      //{{{
      case PI_I2C_ESC:
        setesc = 1;
        break;
      //}}}
      //{{{
      case PI_I2C_READ:
        bytes = myI2CGetPar(inBuf, &inPos, inLen, &esc);
        if (bytes >= 0) {
          if ((bytes + outPos) < (int)outLen) {
            segs[numSegs].addr = addr;
            segs[numSegs].flags = (flags|1);
            segs[numSegs].len = bytes;
            segs[numSegs].buf = (uint8_t *)(outBuf + outPos);
            outPos += bytes;
            numSegs++;
            if (numSegs >= PI_I2C_RDRW_IOCTL_MAX_MSGS) {
              status = i2cSegments(handle, segs, numSegs);
              if (status >= 0)
                status = 0; // continue
              numSegs = 0;
              }
            }
          else
            status = PI_BAD_I2C_RLEN;
          }
        else
          status = PI_BAD_I2C_RLEN;
        break;
      //}}}
      //{{{
      case PI_I2C_WRITE:

        bytes = myI2CGetPar(inBuf, &inPos, inLen, &esc);
        if (bytes >= 0) {
          if ((bytes + inPos) < (int)inLen) {
            segs[numSegs].addr = addr;
            segs[numSegs].flags = (flags&0xfffe);
            segs[numSegs].len = bytes;
            segs[numSegs].buf = (uint8_t *)(inBuf + inPos);
            inPos += bytes;
            numSegs++;
            if (numSegs >= PI_I2C_RDRW_IOCTL_MAX_MSGS) {
              status = i2cSegments(handle, segs, numSegs);
              if (status >= 0)
                status = 0; // continue
              numSegs = 0;
              }
            }
          else
            status = PI_BAD_I2C_WLEN;
          }
        else
           status = PI_BAD_I2C_WLEN;
        break;
      //}}}
      //{{{
      default:
        status = PI_BAD_I2C_CMD;
      //}}}
      }

    if (setesc)
      esc = 1;
    else
      esc = 0;
    setesc = 0;
    }

  if (status >= 0)
     if (numSegs)
       status = i2cSegments(handle, segs, numSegs);

  if (status >= 0)
    status = outPos;

  return status;
  }
//}}}

// bitBang helpers
//{{{
static int readSDA (wfRx_t* w) {

  myGpioSetMode (w->I.SDA, PI_INPUT);
  return myGpioRead (w->I.SDA);
  }
//}}}
//{{{
static void setSDA (wfRx_t* w) {
  myGpioSetMode (w->I.SDA, PI_INPUT);
  }
//}}}
//{{{
static void clearSDA (wfRx_t* w) {

  myGpioSetMode (w->I.SDA, PI_OUTPUT);
  myGpioWrite (w->I.SDA, 0);
  }
//}}}
//{{{
static void clearSCL (wfRx_t* w) {
  myGpioSetMode (w->I.SCL, PI_OUTPUT);
  myGpioWrite (w->I.SCL, 0);
  }
//}}}

//{{{
static void I2Cdelay (wfRx_t* w) {
  myGpioDelay (w->I.delay);
  }
//}}}
//{{{
static void I2CclockStretch (wfRx_t* w) {

  uint32_t now;
  uint32_t max_stretch = 100000;

  myGpioSetMode (w->I.SCL, PI_INPUT);
  now = gpioTick();
  while ((myGpioRead(w->I.SCL) == 0) && ((gpioTick() - now) < max_stretch)) {}
  }
//}}}
//{{{
static void I2CStart (wfRx_t* w) {

  if (w->I.started) {
    setSDA (w);
    I2Cdelay (w);
    I2CclockStretch (w);
    I2Cdelay (w);
    }

  clearSDA (w);
  I2Cdelay (w);
  clearSCL (w);
  I2Cdelay (w);

  w->I.started = 1;
  }
//}}}
//{{{
static void I2CStop (wfRx_t* w) {

  clearSDA (w);
  I2Cdelay (w);
  I2CclockStretch (w);
  I2Cdelay (w);
  setSDA (w);
  I2Cdelay (w);

  w->I.started = 0;
  }
//}}}
//{{{
static void I2CPutBit (wfRx_t* w, int bit) {

  if (bit)
    setSDA (w);
  else
   clearSDA (w);

  I2Cdelay (w);
  I2CclockStretch (w);
  I2Cdelay (w);
  clearSCL (w);
  }
//}}}
//{{{
static int I2CGetBit (wfRx_t* w) {

  // let SDA float
  setSDA (w);
  I2Cdelay (w);
  I2CclockStretch (w);

  int bit = readSDA (w);
  I2Cdelay (w);
  clearSCL (w);
  return bit;
  }
//}}}
//{{{
static int I2CPutByte (wfRx_t* w, int byte) {

  for (int bit = 0; bit < 8; bit++) {
    I2CPutBit (w, byte & 0x80);
    byte <<= 1;
    }

  int nack = I2CGetBit (w);
  return nack;
  }
//}}}
//{{{
static uint8_t I2CGetByte (wfRx_t* w, int nack) {

  int byte = 0;
  for (int bit = 0; bit < 8; bit++)
    byte = (byte << 1) | I2CGetBit(w);

  I2CPutBit (w, nack);

  return byte;
  }
//}}}

// external bitBang
//{{{
int bbI2COpen (uint32_t SDA, uint32_t SCL, uint32_t baud) {

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
int bbI2CClose (uint32_t SDA) {

  switch(wfRx[SDA].mode) {
    case PI_WFRX_I2C_SDA:
      myGpioSetMode(wfRx[SDA].I.SDA, wfRx[SDA].I.SDAMode);
      myGpioSetMode(wfRx[SDA].I.SCL, wfRx[SDA].I.SCLMode);
      wfRx[wfRx[SDA].I.SDA].mode = PI_WFRX_NONE;
      wfRx[wfRx[SDA].I.SCL].mode = PI_WFRX_NONE;
      break;

    default:
      LOG_ERROR ("no I2C on gpio (%d)", SDA);
      break;
    }

  return 0;
  }
//}}}
//{{{
int bbI2CZip (uint32_t SDA, char* inBuf, uint32_t inLen, char* outBuf, uint32_t outLen) {

  int i, ack, inPos, outPos, status, bytes;
  int addr, flags, esc, setesc;
  wfRx_t* w = &wfRx[SDA];

  inPos = 0;
  outPos = 0;
  status = 0;
  addr = 0;
  flags = 0;
  esc = 0;
  setesc = 0;

  wfRx_lock (SDA);

  while (!status && (inPos < (int)inLen)) {
    cLog::log (LOGINFO, "status=%d inpos=%d inlen=%d cmd=%d addr=%d flags=%x",
                       status, inPos, inLen, inBuf[inPos], addr, flags);

    switch (inBuf[inPos++]) {
      //{{{
      case PI_I2C_END:
        status = 1;
        break;
      //}}}
      //{{{
      case PI_I2C_START:
        I2CStart(w);
        break;
      //}}}
      //{{{
      case PI_I2C_STOP:
        I2CStop(w);
        break;
      //}}}
      //{{{
      case PI_I2C_ADDR:
        addr = myI2CGetPar(inBuf, &inPos, inLen, &esc);
        if (addr < 0)
          status = PI_BAD_I2C_CMD;
        break;
      //}}}
      //{{{
      case PI_I2C_FLAGS:
        /* cheat to force two byte flags */
        esc = 1;
        flags = myI2CGetPar (inBuf, &inPos, inLen, &esc);
        if (flags < 0)
          status = PI_BAD_I2C_CMD;
        break;
      //}}}
      //{{{
      case PI_I2C_ESC:
        setesc = 1;
        break;
      //}}}
      //{{{
      case PI_I2C_READ:
        bytes = myI2CGetPar(inBuf, &inPos, inLen, &esc);
        if (bytes >= 0)
          ack = I2CPutByte(w, (addr<<1)|1);

         if (bytes > 0) {
           if (!ack) {
             if ((bytes + outPos) <= (int)outLen) {
               for (i=0; i<(bytes-1); i++)
                 outBuf[outPos++] = I2CGetByte(w, 0);
               outBuf[outPos++] = I2CGetByte(w, 1);
               }
             else
               status = PI_BAD_I2C_RLEN;
             }
           else
             status = PI_I2C_READ_FAILED;
          }
        else
          status = PI_BAD_I2C_CMD;
        break;
      //}}}
      //{{{
      case PI_I2C_WRITE:
        bytes = myI2CGetPar(inBuf, &inPos, inLen, &esc);
        if (bytes >= 0)
          ack = I2CPutByte(w, addr<<1);

        if (bytes > 0) {
          if (!ack) {
            if ((bytes + inPos) <= (int)inLen) {
              for (i=0; i<(bytes-1); i++) {
                ack = I2CPutByte(w, inBuf[inPos++]);
                if (ack)
                  status = PI_I2C_WRITE_FAILED;
                }
              ack = I2CPutByte(w, inBuf[inPos++]);
              }
            else
              status = PI_BAD_I2C_WLEN;
            }
          else
            status = PI_I2C_WRITE_FAILED;
          }
        else
          status = PI_BAD_I2C_CMD;
        break;
      //}}}
      //{{{
      default:
        status = PI_BAD_I2C_CMD;
      //}}}
      }

    if (setesc)
      esc = 1;
    else
      esc = 0;
    setesc = 0;
    }

  wfRx_unlock (SDA);

  if (status >= 0)
    status = outPos;

  return status;
  }
//}}}

// bsc mode
//{{{
static void bscInit (int mode) {

  bscsReg[BSC_CR] = 0; /* clear device */
  bscsReg[BSC_RSR] = 0; /* clear underrun and overrun errors */
  bscsReg[BSC_SLV] = 0; /* clear I2C slave address */
  bscsReg[BSC_IMSC] = 0xf; /* mask off all interrupts */
  bscsReg[BSC_ICR] = 0x0f; /* clear all interrupts */

  int sda, scl, miso, ce;
  if (pi_is_2711) {
    sda = BSC_SDA_MOSI_2711;
    scl = BSC_SCL_SCLK_2711;
    miso = BSC_MISO_2711;
    ce = BSC_CE_N_2711;
    }
  else {
    sda = BSC_SDA_MOSI;
    scl = BSC_SCL_SCLK;
    miso = BSC_MISO;
    ce = BSC_CE_N;
    }

  gpioSetMode (sda, PI_ALT3);
  gpioSetMode (scl, PI_ALT3);

  if (mode > 1) {
    // SPI uses all GPIO
    gpioSetMode (miso, PI_ALT3);
    gpioSetMode (ce, PI_ALT3);
    }
  }
//}}}
//{{{
static void bscTerm (int mode) {

  bscsReg[BSC_CR] = 0; // clear device
  bscsReg[BSC_RSR] = 0; // clear underrun and overrun errors
  bscsReg[BSC_SLV] = 0; // clear I2C slave address

  int sda, scl, miso, ce;
  if (pi_is_2711) {
    sda = BSC_SDA_MOSI_2711;
    scl = BSC_SCL_SCLK_2711;
    miso = BSC_MISO_2711;
    ce = BSC_CE_N_2711;
    }
  else {
    sda = BSC_SDA_MOSI;
    scl = BSC_SCL_SCLK;
    miso = BSC_MISO;
    ce = BSC_CE_N;
    }

  gpioSetMode (sda, PI_INPUT);
  gpioSetMode (scl, PI_INPUT);

  if (mode > 1) {
    gpioSetMode (miso, PI_INPUT);
    gpioSetMode (ce, PI_INPUT);
    }
  }
//}}}
//{{{
int bscXfer (bsc_xfer_t* xfer) {

  static int bscMode = 0;

  int mode;
  if (xfer->control) {
    // bscMode (0=None, 1=I2C, 2=SPI) tracks which GPIO have been set to BSC mode
    if (xfer->control & 2)
      mode = 2; /* SPI */
    else
      mode = 1; /* assume I2C */

    if (mode > bscMode) {
      bscInit (mode);
      bscMode = mode;
      }
    }
  else {
    if (bscMode)
      bscTerm (bscMode);
    bscMode = 0;
    return 0; /* leave ignore set */
    }

  xfer->rxCnt = 0;

  bscsReg[BSC_SLV] = ((xfer->control)>>16) & 127;
  bscsReg[BSC_CR] = (xfer->control) & 0x3fff;
  bscsReg[BSC_RSR]=0; /* clear underrun and overrun errors */

  int copied = 0;
  int active = 1;
  while (active) {
    active = 0;

    while ((copied < xfer->txCnt) && (!(bscsReg[BSC_FR] & BSC_FR_TXFF))) {
      bscsReg[BSC_DR] = xfer->txBuf[copied++];
      active = 1;
      }

    while ((xfer->rxCnt < kBscFifoSize) && (!(bscsReg[BSC_FR] & BSC_FR_RXFE))) {
      xfer->rxBuf[xfer->rxCnt++] = bscsReg[BSC_DR];
      active = 1;
      }

    if (!active)
      active = bscsReg[BSC_FR] & (BSC_FR_RXBUSY | BSC_FR_TXBUSY);

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
int serOpen (char* tty, uint32_t serBaud, uint32_t serFlags) {

  static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  struct termios newTermios;
  int speed;
  int fd;
  int i, slot;

  if (strncmp ("/dev/tty", tty, 8) && strncmp ("/dev/serial", tty, 11))
    LOG_ERROR ("bad device (%s)", tty);

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
      LOG_ERROR ("bad speed (%d)", serBaud);
    }

  if (serFlags)
   LOG_ERROR ("bad flags (0x%X)", serFlags);

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
    LOG_ERROR ("no serial handles");

  if ((fd = open(tty, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK)) == -1) {
    serInfo[slot].state = PI_SER_CLOSED;
    return PI_SER_OPEN_FAILED;
    }

  tcgetattr (fd, &newTermios);

  cfmakeraw (&newTermios);

  cfsetispeed (&newTermios, speed);
  cfsetospeed (&newTermios, speed);

  newTermios.c_cc [VMIN]  = 0;
  newTermios.c_cc [VTIME] = 0;

  tcflush (fd, TCIFLUSH);
  tcsetattr (fd, TCSANOW, &newTermios);
  //fcntl(fd, F_SETFL, O_RDWR);

  serInfo[slot].fd = fd;
  serInfo[slot].flags = serFlags;
  serInfo[slot].state = PI_SER_OPENED;

  return slot;
  }
//}}}
//{{{
int serClose (uint32_t handle) {

  if (serInfo[handle].fd >= 0)
    close (serInfo[handle].fd);

  serInfo[handle].fd = -1;
  serInfo[handle].state = PI_SER_CLOSED;

  return 0;
  }
//}}}

//{{{
int serWriteByte (uint32_t handle, uint32_t bVal) {

  char c = bVal;
  if (write (serInfo[handle].fd, &c, 1) != 1)
    return PI_SER_WRITE_FAILED;
  else
    return 0;
  }
//}}}
//{{{
int serReadByte (uint32_t handle) {

  int r;
  char x;

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
int serWrite (uint32_t handle, char *buf, uint32_t count) {

  int written = 0;
  int wrote = 0;

  while ((written != (int)count) && (wrote >= 0)) {
    wrote = write(serInfo[handle].fd, buf+written, count-written);
    if (wrote >= 0) {
      written += wrote;

      if (written != (int)count)
        timeSleep (0.05);
      }
    }

  if (written != (int)count)
     return PI_SER_WRITE_FAILED;
  else
     return 0;
  }
//}}}
//{{{
int serRead (uint32_t handle, char *buf, uint32_t count) {

  int r = read (serInfo[handle].fd, buf, count);
  if (r == -1) {
    if (errno == EAGAIN)
      return PI_SER_READ_NO_DATA;
    else
      return PI_SER_READ_FAILED;
    }
  else {
    if (r < (int)count) buf[r] = 0;
      return r;
    }
  }
//}}}

//{{{
int serDataAvailable (uint32_t handle) {

  int result;
  if (ioctl (serInfo[handle].fd, FIONREAD, &result) == -1)
    return 0;

  return result;
  }
//}}}

// bitbang
//{{{
int gpioSerialReadOpen (uint32_t gpio, uint32_t baud, uint32_t data_bits) {

  int bitTime = (1000 * 1000000) / baud; // nanos
  int timeout  = ((data_bits+2) * bitTime)/1000000; // millis
  if (timeout < 1)
    timeout = 1;

  wfRx[gpio].gpio = gpio;
  wfRx[gpio].mode = PI_WFRX_SERIAL;
  wfRx[gpio].baud = baud;

  wfRx[gpio].s.buf      = (char*)malloc(SRX_BUF_SIZE);
  wfRx[gpio].s.bufSize  = SRX_BUF_SIZE;
  wfRx[gpio].s.timeout  = timeout;
  wfRx[gpio].s.fullBit  = bitTime;         // nanos
  wfRx[gpio].s.halfBit  = (bitTime/2)+500; // nanos (500 for rounding)
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
int gpioSerialReadInvert (uint32_t gpio, uint32_t invert) {

  wfRx[gpio].s.invert = invert;

  return 0;
  }
//}}}
//{{{
int gpioSerialRead (uint32_t gpio, void* buf, size_t bufSize) {

  uint32_t bytes = 0;
  uint32_t wpos;
  volatile wfRx_t* w = &wfRx[gpio];
  if (w->s.readPos != w->s.writePos) {
    wpos = w->s.writePos;

    if ((int)wpos > w->s.readPos)
      bytes = wpos - w->s.readPos;
    else
      bytes = w->s.bufSize - w->s.readPos;

    if (bytes > bufSize)
      bytes = bufSize;

    // copy in multiples of the data size in bytes
    bytes = (bytes / w->s.bytes) * w->s.bytes;
    if (buf)
      memcpy (buf, w->s.buf+w->s.readPos, bytes);

    w->s.readPos += bytes;

    if (w->s.readPos >= (int)w->s.bufSize)
      w->s.readPos = 0;
    }

  return bytes;
  }
//}}}
//{{{
int gpioSerialReadClose (uint32_t gpio) {

  switch(wfRx[gpio].mode) {
    case PI_WFRX_NONE:
      LOG_ERROR ("no serial read on gpio (%d)", gpio);
      break;

    case PI_WFRX_SERIAL:
      free(wfRx[gpio].s.buf);
      wfRx[gpio].mode = PI_WFRX_NONE;
      break;
    }

  return 0;
  }
//}}}
//}}}
