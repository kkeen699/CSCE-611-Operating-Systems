#ifndef QUEUE_H
#define QUEUE_H

#include "thread.H"

#define QUEUE_SIZE 100

class Queue {
private:
  Thread * ready_queue[QUEUE_SIZE];
  int capacity;
  int queue_size;
  int front;
  int rear;

public:
  Queue() {
    capacity = QUEUE_SIZE;
    queue_size = 0;
    front = 0;
    rear = -1;
    for(int i = 0; i < capacity; i++)
      ready_queue[i] = NULL;
  }

  bool is_empty(){
    return (queue_size == 0);
  }
  
  void enqueue(Thread * _thread){
    assert(queue_size < capacity);
    rear = (rear+1) % capacity;
    ready_queue[rear] = _thread;
    queue_size++;
  }

  Thread * dequeue(){
    assert(0 < queue_size);
    Thread * next = ready_queue[front];
    front = (front+1) % capacity;
    queue_size--;
    return next;
  }

  Thread * get_head(){
    assert(0 < queue_size);
    return ready_queue[front];
  }

  void remove(Thread * _thread){
    assert(0 < queue_size)
    bool removed = false;
    for(int i = 0; i < queue_size; i++){
      int idx = (front+i) % capacity;
      if(ready_queue[idx] == _thread)
        removed = true;

      if(removed){
        int n = (idx+1) % capacity;
        ready_queue[idx] = ready_queue[n];
      }
    }
    if(removed){
      queue_size--;
      rear = (rear == 0) ? capacity-1 : rear-1;
    }
  }
};

#endif