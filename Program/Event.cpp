#include "Event.h"

// Constructor for an event
Event::Event(void (*action)(), unsigned long time){
  this->action = action;
  this->time = time + millis();
}
