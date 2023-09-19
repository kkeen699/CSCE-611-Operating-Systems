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
#include "blocking_disk.H"
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

// #define _MIRRORED_DISK
/* used for testing MirroredDisk*/

// #define _INTERRUPT
/* used for testing interrupt*/

// #define _THREAD_SAFE
/* used for testing thread-safe*/


#ifndef _MIRRORED_DISK
extern BlockingDisk * SYSTEM_DISK;
#else
extern MirroredDisk * SYSTEM_DISK;
#endif

Scheduler::Scheduler() : ready_queue() {
}

void Scheduler::yield() {
#ifdef _INTERRUPT
  bool enable = false;
  if(Machine::interrupts_enabled()){
    Machine::disable_interrupts();
    enable = true;
  }
#endif
#ifndef _INTERRUPT
  if(SYSTEM_DISK->disk_ready() && !SYSTEM_DISK->is_wait_CPU()){
    resume(SYSTEM_DISK->dequeue());
    Console::puts("Disk is ready, put thread into ready queue\n");
  }
#endif
#ifdef _THREAD_SAFE
  if(SYSTEM_DISK->need_issued())
    Console::puts("Resume thread for issue operation\n");
  Thread * next = SYSTEM_DISK->need_issued() ?
                  SYSTEM_DISK->get_head() : ready_queue.dequeue();
#else
  Thread * next = ready_queue.dequeue();
#endif
  Thread::dispatch_to(next);

#ifdef _INTERRUPT
  if(enable)
    Machine::enable_interrupts();
#endif
}

void Scheduler::resume(Thread * _thread) {
#ifdef _INTERRUPT
  bool enable = false;
  if(Machine::interrupts_enabled()){
    Machine::disable_interrupts();
    enable = true;
  }
#endif

  ready_queue.enqueue(_thread);

#ifdef _INTERRUPT
  if(enable)
    Machine::enable_interrupts();
#endif
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
    // if(Machine::interrupts_enabled())
    //   Machine::disable_interrupts();

    ready_queue.remove(_thread);

    // Machine::enable_interrupts();
  }
}