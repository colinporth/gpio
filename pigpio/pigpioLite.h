// pigpio.h
#pragma once
#include <stdint.h>

//{{{  defines
#define PIGPIO_VERSION 77

// gpio: 0-53
#define PI_MIN_GPIO       0
#define PI_MAX_USER_GPIO 31
#define PI_MAX_GPIO      53

// level: 0-1
#define PI_OFF   0
#define PI_ON    1
#define PI_CLEAR 0
#define PI_SET   1
#define PI_LOW   0
#define PI_HIGH  1
#define PI_TIMEOUT 2

// mode: 0-7
#define PI_INPUT  0
#define PI_OUTPUT 1

#define PI_ALT0   4
#define PI_ALT1   5
#define PI_ALT2   6
#define PI_ALT3   7
#define PI_ALT4   3
#define PI_ALT5   2

// pud 0-2
#define PI_PUD_OFF  0
#define PI_PUD_DOWN 1
#define PI_PUD_UP   2

//{{{  dutyCycle
#define PI_DEFAULT_DUTYCYCLE_RANGE   255
#define PI_MIN_DUTYCYCLE_RANGE        25
#define PI_MAX_DUTYCYCLE_RANGE     40000
//}}}
//{{{  pulsewidth: 0, 500-2500
#define PI_SERVO_OFF 0

#define PI_MIN_SERVO_PULSEWIDTH 500
#define PI_MAX_SERVO_PULSEWIDTH 2500
//}}}
//{{{  hardware PWM
#define PI_HW_PWM_MIN_FREQ 1

#define PI_HW_PWM_MAX_FREQ      125000000
#define PI_HW_PWM_MAX_FREQ_2711 187500000

#define PI_HW_PWM_RANGE 1000000
//}}}
//{{{  hardware clock
#define PI_HW_CLK_MIN_FREQ       4689
#define PI_HW_CLK_MIN_FREQ_2711 13184

#define PI_HW_CLK_MAX_FREQ      250000000
#define PI_HW_CLK_MAX_FREQ_2711 375000000
//}}}

//{{{  I2C, SPI, SER
//#define PI_FILE_SLOTS 16
#define PI_I2C_SLOTS  512
#define PI_SPI_SLOTS  32
#define PI_SER_SLOTS  16

#define PI_MAX_I2C_ADDR 0x7F

#define PI_NUM_AUX_SPI_CHANNEL 3
#define PI_NUM_STD_SPI_CHANNEL 2

#define PI_MAX_I2C_DEVICE_COUNT (1<<16)
#define PI_MAX_SPI_DEVICE_COUNT (1<<16)

/* max pi_i2c_msg_t per transaction */
#define  PI_I2C_RDRW_IOCTL_MAX_MSGS 42

/* flags for i2cTransaction, pi_i2c_msg_t */
#define PI_I2C_M_WR           0x0000 /* write data */
#define PI_I2C_M_RD           0x0001 /* read data */
#define PI_I2C_M_TEN          0x0010 /* ten bit chip address */
#define PI_I2C_M_RECV_LEN     0x0400 /* length will be first received byte */
#define PI_I2C_M_NO_RD_ACK    0x0800 /* if I2C_FUNC_PROTOCOL_MANGLING */
#define PI_I2C_M_IGNORE_NAK   0x1000 /* if I2C_FUNC_PROTOCOL_MANGLING */
#define PI_I2C_M_REV_DIR_ADDR 0x2000 /* if I2C_FUNC_PROTOCOL_MANGLING */
#define PI_I2C_M_NOSTART      0x4000 /* if I2C_FUNC_PROTOCOL_MANGLING */

/* bbI2CZip and i2cZip commands */
#define PI_I2C_END          0
#define PI_I2C_ESC          1
#define PI_I2C_START        2
#define PI_I2C_COMBINED_ON  2
#define PI_I2C_STOP         3
#define PI_I2C_COMBINED_OFF 3
#define PI_I2C_ADDR         4
#define PI_I2C_FLAGS        5
#define PI_I2C_READ         6
#define PI_I2C_WRITE        7
//}}}
//{{{  SPI
#define PI_SPI_FLAGS_BITLEN(x) ((x&63)<<16)
#define PI_SPI_FLAGS_RX_LSB(x)  ((x&1)<<15)
#define PI_SPI_FLAGS_TX_LSB(x)  ((x&1)<<14)
#define PI_SPI_FLAGS_3WREN(x)  ((x&15)<<10)
#define PI_SPI_FLAGS_3WIRE(x)   ((x&1)<<9)
#define PI_SPI_FLAGS_AUX_SPI(x) ((x&1)<<8)
#define PI_SPI_FLAGS_RESVD(x)   ((x&7)<<5)
#define PI_SPI_FLAGS_CSPOLS(x)  ((x&7)<<2)
#define PI_SPI_FLAGS_MODE(x)    ((x&3))
//}}}
//{{{  BSC registers
#define BSC_DR         0
#define BSC_RSR        1
#define BSC_SLV        2
#define BSC_CR         3
#define BSC_FR         4
#define BSC_IFLS       5
#define BSC_IMSC       6
#define BSC_RIS        7
#define BSC_MIS        8
#define BSC_ICR        9
#define BSC_DMACR     10
#define BSC_TDR       11
#define BSC_GPUSTAT   12
#define BSC_HCTRL     13
#define BSC_DEBUG_I2C 14
#define BSC_DEBUG_SPI 15

#define BSC_CR_TESTFIFO 2048
#define BSC_CR_RXE  512
#define BSC_CR_TXE  256
#define BSC_CR_BRK  128
#define BSC_CR_CPOL  16
#define BSC_CR_CPHA   8
#define BSC_CR_I2C    4
#define BSC_CR_SPI    2
#define BSC_CR_EN     1

#define BSC_FR_RXBUSY 32
#define BSC_FR_TXFE   16
#define BSC_FR_RXFF    8
#define BSC_FR_TXFF    4
#define BSC_FR_RXFE    2
#define BSC_FR_TXBUSY  1
//}}}
//{{{  BSC GPIO

#define BSC_SDA_MOSI 18
#define BSC_SCL_SCLK 19
#define BSC_MISO     20
#define BSC_CE_N     21

#define BSC_SDA_MOSI_2711 10
#define BSC_SCL_SCLK_2711 11
#define BSC_MISO_2711      9
#define BSC_CE_N_2711      8
//}}}
//{{{  pi defines
#define PI_MAX_BUSY_DELAY 100
#define PI_MIN_WDOG_TIMEOUT 0
#define PI_MAX_WDOG_TIMEOUT 60000
#define PI_MIN_TIMER 0
#define PI_MAX_TIMER 9
#define PI_MIN_MS 10
#define PI_MAX_MS 60000

/* timetype: 0-1 */
#define PI_TIME_RELATIVE 0
#define PI_TIME_ABSOLUTE 1
#define PI_MAX_MICS_DELAY 1000000 /* 1 second */
#define PI_MAX_MILS_DELAY 60000   /* 60 seconds */

/* cfgMillis */
#define PI_BUF_MILLIS_MIN 100
#define PI_BUF_MILLIS_MAX 10000

/* cfgMicros: 1, 2, 4, 5, 8, or 10 */
/* cfgPeripheral: 0-1 */
#define PI_CLOCK_PWM 0
#define PI_CLOCK_PCM 1

/* DMA channel: 0-15, 15 is unset */
#define PI_MIN_DMA_CHANNEL 0
#define PI_MAX_DMA_CHANNEL 15

/* ifFlags: */
#define PI_DISABLE_FIFO_IF   1
#define PI_DISABLE_SOCK_IF   2
#define PI_LOCALHOST_SOCK_IF 4
#define PI_DISABLE_ALERT     8

/* memAllocMode */
#define PI_MEM_ALLOC_AUTO    0
#define PI_MEM_ALLOC_PAGEMAP 1
#define PI_MEM_ALLOC_MAILBOX 2

/* filters */
#define PI_MAX_STEADY  300000
#define PI_MAX_ACTIVE 1000000

/* gpioCfgInternals */
#define PI_CFG_DBG_LEVEL         0 /* bits 0-3 */
#define PI_CFG_ALERT_FREQ        4 /* bits 4-7 */
#define PI_CFG_RT_PRIORITY       (1<<8)
#define PI_CFG_STATS             (1<<9)
#define PI_CFG_NOSIGHANDLER      (1<<10)
#define PI_CFG_ILLEGAL_VAL       (1<<11)

/* gpioISR */
#define RISING_EDGE  0
#define FALLING_EDGE 1
#define EITHER_EDGE  2

/* pads */
#define PI_MAX_PAD 2
#define PI_MIN_PAD_STRENGTH 1
#define PI_MAX_PAD_STRENGTH 16
//}}}
//{{{  default
#define PI_DEFAULT_BUFFER_MILLIS           120

#define PI_DEFAULT_CLK_MICROS              5
#define PI_DEFAULT_CLK_PERIPHERAL          PI_CLOCK_PCM

#define PI_DEFAULT_IF_FLAGS                0
#define PI_DEFAULT_FOREGROUND              0

#define PI_DEFAULT_DMA_CHANNEL             14
#define PI_DEFAULT_DMA_PRIMARY_CHANNEL     14
#define PI_DEFAULT_DMA_SECONDARY_CHANNEL   6
#define PI_DEFAULT_DMA_PRIMARY_CH_2711     7
#define PI_DEFAULT_DMA_SECONDARY_CH_2711   6
#define PI_DEFAULT_DMA_NOT_SET             15

#define PI_DEFAULT_UPDATE_MASK_UNKNOWN     0x0000000FFFFFFCLL
#define PI_DEFAULT_UPDATE_MASK_B1          0x03E7CF93
#define PI_DEFAULT_UPDATE_MASK_A_B2        0xFBC7CF9C
#define PI_DEFAULT_UPDATE_MASK_APLUS_BPLUS 0x0080480FFFFFFCLL
#define PI_DEFAULT_UPDATE_MASK_ZERO        0x0080000FFFFFFCLL
#define PI_DEFAULT_UPDATE_MASK_PI2B        0x0080480FFFFFFCLL
#define PI_DEFAULT_UPDATE_MASK_PI3B        0x0000000FFFFFFCLL
#define PI_DEFAULT_UPDATE_MASK_PI4B        0x0000000FFFFFFCLL
#define PI_DEFAULT_UPDATE_MASK_COMPUTE     0x00FFFFFFFFFFFFLL

#define PI_DEFAULT_MEM_ALLOC_MODE          PI_MEM_ALLOC_AUTO

#define PI_DEFAULT_CFG_INTERNALS           0
//}}}
//}}}

constexpr int kBscFifoSize = 512;
//{{{
struct bsc_xfer_t {
  uint32_t control;         // Write
  int rxCnt;                // Read only
  char rxBuf[kBscFifoSize]; // Read only
  int txCnt;                // Write
  char txBuf[kBscFifoSize]; // Write
  };
//}}}
//{{{
struct pi_i2c_msg_t {
  uint16_t addr;  // slave address
  uint16_t flags;
  uint16_t len;   // msg length
  uint8_t* buf;   // pointer to msg data
  };
//}}}

//{{{  time
int gpioTime (uint32_t timetype, int* seconds, int* micros);
int gpioSleep (uint32_t timetype, int seconds, int micros);
uint32_t gpioDelay (uint32_t micros);
uint32_t gpioTick();
double timeTime();
void timeSleep (double seconds);
//}}}

uint32_t gpioHardwareRevision();
int gpioInitialise();
void gpioTerminate();

//{{{  gpio
int gpioGetMode (uint32_t gpio);
void gpioSetMode (uint32_t gpio, uint32_t mode);

int gpioGetPad (uint32_t pad);
void gpioSetPad (uint32_t pad, uint32_t padStrength);
void gpioSetPullUpDown (uint32_t gpio, uint32_t pud);

int gpioRead (uint32_t gpio);
uint32_t gpioRead_Bits_0_31();

void gpioWrite (uint32_t gpio, uint32_t level);
void gpioWrite_Bits_0_31_Clear (uint32_t bits);
void gpioWrite_Bits_0_31_Set (uint32_t bits);

void gpioPWM (uint32_t user_gpio, uint32_t dutycycle);

int gpioGetPWMdutycycle (uint32_t user_gpio);
int gpioGetPWMrange (uint32_t user_gpio);
int gpioGetPWMrealRange (uint32_t user_gpio);
void gpioSetPWMrange (uint32_t user_gpio, uint32_t range);

int gpioGetPWMfrequency (uint32_t user_gpio);
void gpioSetPWMfrequency (uint32_t user_gpio, uint32_t frequency);

int gpioGetServoPulsewidth (uint32_t user_gpio);
void gpioServo (uint32_t user_gpio, uint32_t pulsewidth);
//}}}
//{{{  spi
int spiOpen (uint32_t spiChan, uint32_t baud, uint32_t spiFlags);

int spiRead (uint32_t handle, char* buf, uint32_t count);
int spiWrite (uint32_t handle, char* buf, uint32_t count);
void spiWriteMainFast (const uint8_t* buf, uint32_t count);
void spiWriteAuxFast (const uint8_t* buf, uint32_t count);
int spiXfer (uint32_t handle, char* txBuf, char* rxBuf, uint32_t count);

int spiClose (uint32_t handle);

// bitbang
int bbSPIOpen (uint32_t CS, uint32_t MISO, uint32_t MOSI, uint32_t SCLK, uint32_t baud, uint32_t spiFlags);
int bbSPIXfer (uint32_t CS, char* inBuf, char* outBuf, uint32_t count);
int bbSPIClose (uint32_t CS);
//}}}
//{{{  i2c
int i2cOpen (uint32_t i2cBus, uint32_t i2cAddr, uint32_t i2cFlags);
int i2cClose (uint32_t handle);

int i2cWriteQuick (uint32_t handle, uint32_t bit);
int i2cWriteByte (uint32_t handle, uint32_t bVal);
int i2cWriteByteData (uint32_t handle, uint32_t i2cReg, uint32_t bVal);
int i2cWriteWordData (uint32_t handle, uint32_t i2cReg, uint32_t wVal);
int i2cWriteBlockData (uint32_t handle, uint32_t i2cReg, char* buf, uint32_t count);
int i2cWriteI2CBlockData (uint32_t handle, uint32_t i2cReg, char* buf, uint32_t count);

int i2cReadByte (uint32_t handle);
int i2cReadByteData (uint32_t handle, uint32_t i2cReg);
int i2cReadWordData (uint32_t handle, uint32_t i2cReg);
int i2cReadBlockData (uint32_t handle, uint32_t i2cReg, char* buf);
int i2cReadI2CBlockData (uint32_t handle, uint32_t i2cReg, char* buf, uint32_t count);

int i2cProcessCall (uint32_t handle, uint32_t i2cReg, uint32_t wVal);
int i2cBlockProcessCall (uint32_t handle, uint32_t i2cReg, char* buf, uint32_t count);

int i2cReadDevice (uint32_t handle, char* buf, uint32_t count);
int i2cWriteDevice (uint32_t handle, char* buf, uint32_t count);

void i2cSwitchCombined (int setting);
int i2cSegments (uint32_t handle, pi_i2c_msg_t* segs, uint32_t numSegs);
int i2cZip (uint32_t handle, char* inBuf, uint32_t inLen, char* outBuf, uint32_t outLen);

int bbI2COpen (uint32_t SDA, uint32_t SCL, uint32_t baud);
int bbI2CClose (uint32_t SDA);
int bbI2CZip (uint32_t SDA, char* inBuf, uint32_t inLen, char* outBuf, uint32_t outLen);

int bscXfer (bsc_xfer_t* bsc_xfer);
//}}}
//{{{  serial
int serOpen (char* sertty, uint32_t baud, uint32_t serFlags);
int serClose (uint32_t handle);

int serWriteByte (uint32_t handle, uint32_t bVal);
int serReadByte (uint32_t handle);
int serWrite (uint32_t handle, char* buf, uint32_t count);
int serRead (uint32_t handle, char* buf, uint32_t count);
int serDataAvailable (uint32_t handle);

int gpioSerialReadOpen (uint32_t user_gpio, uint32_t baud, uint32_t data_bits);
int gpioSerialReadInvert (uint32_t user_gpio, uint32_t invert);
int gpioSerialRead (uint32_t user_gpio, void* buf, size_t bufSize);
int gpioSerialReadClose (uint32_t user_gpio);
//}}}
