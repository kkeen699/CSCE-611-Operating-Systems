/*
 File: scheduler.C
 
 Author:
 Date  :
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "scheduler.H"
#include "thread.H"
#include "console.H"
#include "utils.H"
#include "assert.H"
#include "simple_keyboard.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   S c h e d u l e r  */
/*--------------------------------------------------------------------------*/

Scheduler::Scheduler() {
  capacity = QUEUE_SIZE;
  queue_size = 0;
  front = 0;
  rear = -1;
  for(int i = 0; i < capacity; i++)
    ready_queue[i] = NULL;
}

void Scheduler::yield() {
  assert(0 < queue_size);

  if(Machine::interrupts_enabled())
    Machine::disable_interrupts();

  Thread * next = ready_queue[front];
  front = (front+1) % capacity;
  queue_size--;

  Thread::dispatch_to(next);

  Machine::enable_interrupts();
}

void Scheduler::resume(Thread * _thread) {
  assert(queue_size < capacity);

  if(Machine::interrupts_enabled())
    Machine::disable_interrupts();

  rear = (rear+1) % capacity;
  ready_queue[rear] = _thread;
  queue_size++;

  Machine::enable_interrupts();
}

void Scheduler::add(Thread * _thread) {
  resume(_thread);
}

void Scheduler::terminate(Thread * _thread) {
  if(_thread == Thread::CurrentThread()){
    // if _thread is current thread, switch to next thread
    yield();
  }
  else{
    // if _thread is not current thread, remove it from the ready queue
    assert(0 < queue_size)

    if(Machine::interrupts_enabled())
      Machine::disable_interrupts();

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

    Machine::enable_interrupts();
  }
}

RRScheduler::RRScheduler(EOQTimer * _timer) : Scheduler() {
  timer = _timer;
}

void RRScheduler::yield(){
  assert(0 < queue_size);

  if(Machine::interrupts_enabled())
    Machine::disable_interrupts();

  Thread * next = ready_queue[front];
  front = (front+1) % capacity;
  queue_size--;

  timer->reset_ticks();

  Thread::dispatch_to(next);

  Machine::enable_interrupts();
}