#ifndef EVENTHEADER
#define EVENTHEADER
#include "Arduino.h"
class Event {
  public:
    Event(void (*action)(), unsigned long time);
    unsigned long time;
    void (*action)();
};

#endif