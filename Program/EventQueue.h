#ifndef EVENTQUEUEHEADER
#define EVENTQUEUEHEADER
#define QueSize 24


#include "Event.h"
#include "Arduino.h"

/// A 0-based eventqueue for scheduling
class EventQueue
{
  public:
    EventQueue();
    void Enqueue(Event *e);
    void Enqueue(void (*action)(), unsigned long time);
    void PerformEvents();
    int Count = 0;
  private:
    Event* queue[QueSize];
    void Rootify(int index);
    void Heapify(int index);
    void CheckMillis();
    void Swap(int index1, int index2);
    void PerformEvent(Event* e);
    unsigned long prevMillis = 0;
};

#endif