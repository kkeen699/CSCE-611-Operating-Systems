/*
     File        : blocking_disk.c

     Author      : 
     Modified    : 

     Description : 

*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

// #define _INTERRUPT
/* used for testing interrupt*/

// #define _THREAD_SAFE
/* used for testing thread-safe*/

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "utils.H"
#include "console.H"
#include "blocking_disk.H"
#include "thread.H"
#include "scheduler.H"

extern Scheduler * SYSTEM_SCHEDULER;
/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

BlockingDisk::BlockingDisk(DISK_ID _disk_id, unsigned int _size) 
  : SimpleDisk(_disk_id, _size), block_queue() {
    wait_CPU = false;
    done = true;
    issued = false;
}

/*--------------------------------------------------------------------------*/
/* SIMPLE_DISK FUNCTIONS */
/*--------------------------------------------------------------------------*/

void BlockingDisk::issue_operation(DISK_OPERATION _op, unsigned long _block_no){
  SimpleDisk::issue_operation(_op, _block_no);
}

void BlockingDisk::wait_until_ready(){
  block_queue.enqueue(Thread::CurrentThread());
  Console::puts("Wait for disk, yield\n");
  SYSTEM_SCHEDULER->yield();
}

bool BlockingDisk::disk_ready(){
  return SimpleDisk::is_ready() && !block_queue.is_empty();
}

bool BlockingDisk::is_wait_CPU(){
  return wait_CPU;
}

bool BlockingDisk::is_done(){
  return done;
}

bool BlockingDisk::need_issued(){
  return !issued && !block_queue.is_empty();
}

Thread * BlockingDisk::get_head(){
  return block_queue.get_head();
}

Thread * BlockingDisk::dequeue(){
  wait_CPU = true;
  return block_queue.dequeue();
}

void BlockingDisk::read(unsigned long _block_no, unsigned char * _buf) {
#ifdef _INTERRUPT
  bool enable = false;
  if(Machine::interrupts_enabled()){
    Machine::disable_interrupts();
    enable = true;
  }
#endif
  block_queue.enqueue(Thread::CurrentThread());
#ifdef _THREAD_SAFE
  Console::puts("Wait for issuing operation, yield\n");
  SYSTEM_SCHEDULER->yield();
#endif

  SimpleDisk::issue_operation(DISK_OPERATION::READ, _block_no);
  issued = true;
  done = false;

  Console::puts("Wait for disk, yield\n");
  SYSTEM_SCHEDULER->yield();

  /* read data from port */
  int i;
  unsigned short tmpw;
  for (i = 0; i < 256; i++) {
    tmpw = Machine::inportw(0x1F0);
    _buf[i*2]   = (unsigned char)tmpw;
    _buf[i*2+1] = (unsigned char)(tmpw >> 8);
  }

  wait_CPU = false;
  done = true;
  issued = false;
  Console::puts("READ DONE\n");

#ifdef _INTERRUPT
  if(enable)
    Machine::enable_interrupts();
#endif
}


void BlockingDisk::write(unsigned long _block_no, unsigned char * _buf) {
#ifdef _INTERRUPT
bool enable = false;
  if(Machine::interrupts_enabled()){
    Machine::disable_interrupts();
    enable = true;
  }
#endif
#ifndef _INTERRUPT
  block_queue.enqueue(Thread::CurrentThread());
#endif
#ifdef _THREAD_SAFE
  Console::puts("Wait for issuing operation, yield\n");
  SYSTEM_SCHEDULER->yield();
#endif

  SimpleDisk::issue_operation(DISK_OPERATION::WRITE, _block_no);
  done = false;
  issued = true;
#ifdef _INTERRUPT
  while(!is_ready()){}
#else
  Console::puts("Wait for disk, yield\n");
  SYSTEM_SCHEDULER->yield();
#endif
  /* write data to port */
  int i; 
  unsigned short tmpw;
  for (i = 0; i < 256; i++) {
    tmpw = _buf[2*i] | (_buf[2*i+1] << 8);
    Machine::outportw(0x1F0, tmpw);
  }

  wait_CPU = false;
  done = true;
  issued = false;
  Console::puts("WRITE DONE\n");

#ifdef _INTERRUPT
  if(enable)
    Machine::enable_interrupts();
#endif
}



/* MirroredDisk */
MirroredDisk::MirroredDisk(unsigned int _size) 
  : SimpleDisk(DISK_ID::MASTER, _size), block_queue(){ 
    wait_CPU = false;
}

void MirroredDisk::issue_operation(DISK_OPERATION _op, unsigned long _block_no, DISK_ID _disk_id){
  Machine::outportb(0x1F1, 0x00); /* send NULL to port 0x1F1         */
  Machine::outportb(0x1F2, 0x01); /* send sector count to port 0X1F2 */
  Machine::outportb(0x1F3, (unsigned char)_block_no);
                         /* send low 8 bits of block number */
  Machine::outportb(0x1F4, (unsigned char)(_block_no >> 8));
                         /* send next 8 bits of block number */
  Machine::outportb(0x1F5, (unsigned char)(_block_no >> 16));
                         /* send next 8 bits of block number */
  unsigned int disk_no = _disk_id == DISK_ID::MASTER ? 0 : 1;
  Machine::outportb(0x1F6, ((unsigned char)(_block_no >> 24)&0x0F) | 0xE0 | (disk_no << 4));
                         /* send drive indicator, some bits, 
                            highest 4 bits of block no */

  Machine::outportb(0x1F7, (_op == DISK_OPERATION::READ) ? 0x20 : 0x30);

}

void MirroredDisk::wait_until_ready(){
  block_queue.enqueue(Thread::CurrentThread());
  Console::puts("Wait for disk, yield\n");
  SYSTEM_SCHEDULER->yield();
}

void MirroredDisk::_wtite(unsigned long _block_no, unsigned char * _buf, DISK_ID _disk_id){
  issue_operation(DISK_OPERATION::WRITE, _block_no, _disk_id);
  wait_until_ready();
  int i; 
  unsigned short tmpw;
  for (i = 0; i < 256; i++) {
    tmpw = _buf[2*i] | (_buf[2*i+1] << 8);
    Machine::outportw(0x1F0, tmpw);
  }
}

bool MirroredDisk::disk_ready(){
  return SimpleDisk::is_ready() && !block_queue.is_empty();
}

bool MirroredDisk::is_wait_CPU(){
  return wait_CPU;
}

Thread * MirroredDisk::dequeue(){
  wait_CPU = true;
  return block_queue.dequeue();
}

void MirroredDisk::read(unsigned long _block_no, unsigned char * _buf){
  issue_operation(DISK_OPERATION::READ, _block_no, DISK_ID::MASTER);
  issue_operation(DISK_OPERATION::READ, _block_no, DISK_ID::DEPENDENT);

  wait_until_ready();

  int i;
  unsigned short tmpw;
  for (i = 0; i < 256; i++) {
    tmpw = Machine::inportw(0x1F0);
    _buf[i*2]   = (unsigned char)tmpw;
    _buf[i*2+1] = (unsigned char)(tmpw >> 8);
  }
  wait_CPU = false;
  Console::puts("READ DONE\n");
}

void MirroredDisk::write(unsigned long _block_no, unsigned char * _buf){
  _wtite(_block_no, _buf, DISK_ID::MASTER);
  wait_CPU = false;
  Console::puts("WRITE MASTER DONE\n");

  _wtite(_block_no, _buf, DISK_ID::DEPENDENT);
  wait_CPU = false;
  Console::puts("WRITE DEPENDENT DONE\n");
}





