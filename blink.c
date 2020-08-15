#include <stdio.h>
#include <pigpio.h>

int main(int argc, char *argv[]) {

  double start;

  if (gpioInitialise() < 0) {
    fprintf(stderr, "pigpio initialisation failed\n");
    return 1;
    }

  gpioSetMode (18, PI_OUTPUT);

  start = time_time();
  while ((time_time() - start) < 60.0) {
    gpioWrite (18, 1); /* on */
    time_sleep (0.5);
    gpioWrite (18, 0); /* off */
    time_sleep (0.5);
    }

  gpioTerminate();

  return 0;
  }
