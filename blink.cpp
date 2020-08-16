#include <stdio.h>
#include "pigpio/pigpio.h"

int main (int argc, char *argv[]) {

  unsigned hardwareRevision = gpioHardwareRevision();
  unsigned version = gpioVersion();
  printf ("pigpio %d %d\n", hardwareRevision, version);

  if (gpioInitialise() < 0) {
    printf ("pigpio initialisation failed\n");
    return 1;
    }

  gpioSetMode (18, PI_OUTPUT);

  double start = time_time();
  while ((time_time() - start) < 60.0) {
    gpioWrite (18, 1); /* on */
    time_sleep (0.5);
    gpioWrite (18, 0); /* off */
    time_sleep (0.5);
    }

  gpioTerminate();

  return 0;
  }
