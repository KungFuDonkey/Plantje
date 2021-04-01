#include "EventQueue.h"

// Empty EventQueue
EventQueue::EventQueue()
{
  for(int i = 0; i < QueSize; i++){
    queue[i] = nullptr;
  }
}

/// Enqueue an event into the queue
void EventQueue::Enqueue(Event *e)
{
#ifndef RELEASE
  if(Count == QueSize){
    Serial.print("WARNING QUEUE IS FULL");
  }
#endif
  CheckMillis();
  queue[Count] = e;
  Count++;
  Rootify(Count);
}

void EventQueue::Enqueue(void (*action)(), unsigned long time){
  Enqueue(new Event(action, time));
}


/// longtime for catching the 49.71 day period
#define longtime 2147483648
/// Check if an event must be handled and handle if so
void EventQueue::PerformEvents()
{
  CheckMillis();
  if(Count == 0 || millis() < queue[0]->time || (millis() > queue[0]->time && millis() - queue[0]->time > longtime)) return;
  PerformEvent(queue[0]);
  Count--;
  queue[0] = queue[Count];
  Heapify(1);
}

/// Handle event e
void EventQueue::PerformEvent(Event* e){
  if(e != NULL){
    e->action();
    delete(e);
  }
}

/// Checks if the millis went back to 0
void EventQueue::CheckMillis(){
  unsigned long mil = millis();
  if(mil >= prevMillis){
    prevMillis = mil;
    return;
  } 
  for(int i = 0; i < Count; i++)
  {
    if(queue[i]->time > longtime){
      queue[i]->time = 0;
    }
  }
}

/// From heap, orders the heap from a leaf to a root
void EventQueue::Rootify(int index){
  if(index == 1 || queue[index - 1]->time > queue[index / 2 - 1]->time || queue[index / 2 - 1]->time - queue[index - 1]->time > longtime) return;
  Swap(index - 1, index / 2 - 1);
  Rootify(index / 2);
}

/// From heap, orders the heap from the index to a leaf
void EventQueue::Heapify(int index){
  if(index * 2 > Count) return;
  Event* e = queue[index - 1];
  int otherindex = index;
  if((e->time > queue[index * 2 - 1]->time && e->time - queue[index * 2 - 1]->time < longtime) || (e->time < queue[index * 2 - 1]->time && queue[index * 2 - 1]->time - e->time > longtime)){
    otherindex = index * 2;
    e = queue[otherindex - 1];
  }
  if(index * 2 < Count && (e->time > queue[index * 2]->time && (e->time - queue[index * 2]->time < longtime) || (e->time < queue[index * 2]->time && queue[index * 2]->time - e->time > longtime)))
    otherindex = (index * 2) + 1;
  
  if (otherindex != index){
    Swap(index - 1, otherindex - 1);
    Heapify(otherindex);
  }
}

/// Swaps 2 elements in an array
void EventQueue::Swap(int index1, int index2){
  Event* tmp = queue[index1];
  queue[index1] = queue[index2];
  queue[index2] = tmp;
}

/// for debugging
/*
void EventQueue::Debug(){
  Serial.print("Count: ");
  Serial.print(Count);
  Serial.println(" Queue:");
  for(int i = 0; i < QueSize; i++){
    if(queue[i] != nullptr){
      Serial.print(queue[i]->time);
      Serial.print(" ");
    }
    else{
      Serial.print("NULL ");
    }
  }
  Serial.println("");
}
*/
