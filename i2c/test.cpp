// test.cpp
//includes
#include <cstdint>
#include <string>

#include "../../shared/utils/utils.h"
#include "../../shared/utils/cLog.h"

using namespace std;


int main (int numArgs, char* args[]) {

  bool draw = false;
  bool drawRadial = false;
  cLcd::eRotate rotate = cLcd::e0;
  cLcd::eInfo info = cLcd::eNone;
  cLcd::eMode mode = cLcd::eCoarse;
  eLogLevel logLevel = LOGINFO;

  // dumb command line option parser
  for (int argIndex = 1; argIndex < numArgs; argIndex++) {
    string str (args[argIndex]);
    if (str == "0") rotate = cLcd::e0;
    else if (str == "90") rotate = cLcd::e90;
    else if (str == "180") rotate = cLcd::e180;
    else if (str == "270") rotate = cLcd::e270;
    else if (str == "o") info = cLcd::eOverlay;
    else if (str == "a") mode = cLcd::eAll;
    else if (str == "s") mode = cLcd::eSingle;
    else if (str == "c") mode = cLcd::eCoarse;
    else if (str == "e") mode = cLcd::eExact;
    else if (str == "1") logLevel = LOGINFO1;
    else if (str == "2") logLevel = LOGINFO2;
    else if (str == "r") drawRadial = true;
    else if (str == "d") draw = true;
    else
      cLog::log (LOGERROR, "unrecognised option " + str);
    }

  cLog::init (logLevel, false, "", "gpio");

  return 0;
  }
