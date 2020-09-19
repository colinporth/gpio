// test.cpp
//includes
#include <cstdint>
#include <string>

#include "../pigpio/pigpio.h"
#include "../../shared/utils/utils.h"
#include "../../shared/utils/cLog.h"

using namespace std;


int main (int numArgs, char* args[]) {

  cLog::init (LOGINFO, false, "", "gpio");
  cLog::log (LOGINFO, "initialise hwRev:" + hex (gpioHardwareRevision(),8) +
                      " version:" + dec (gpioVersion()));

  if (gpioInitialise() <= 0)
    return false;

  cLog::log (LOGINFO, "test");

  int handle = i2cOpen (1, 0x50, 0);
  cLog::log (LOGINFO, "open %x", handle);

  int res = i2cWriteByte (handle, 0);
  int res1 = i2cReadByte (handle);
  cLog::log (LOGINFO, "write %x %x", res, res1);

  //uint8_t buf[256];
  //int res2 = i2cReadI2CBlockData (handle, 0, (char*)buf, 32);

  for (int j = 0; j < 8; j++) {
    string nnn;
    for (int i = 0; i < 32; i++) {
      int value = i2cReadByte (handle);
      nnn += hex(value,2) + " ";
      }
    cLog::log (LOGINFO, nnn);
    }

  return 0;
  }
