/*
     File        : blocking_disk.H

     Author      : 

     Date        : 
     Description : 

*/

#ifndef _BLOCKING_DISK_H_
#define _BLOCKING_DISK_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "simple_disk.H"
#include "queue.H"
/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */ 
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* B l o c k i n g D i s k  */
/*--------------------------------------------------------------------------*/

class BlockingDisk : public SimpleDisk {
private:
   Queue block_queue;
   bool wait_CPU;
   bool done;
   bool issued;
protected:
   virtual void issue_operation(DISK_OPERATION _op, unsigned long _block_no);
   virtual void wait_until_ready();
public:
   BlockingDisk(DISK_ID _disk_id, unsigned int _size); 
   /* Creates a BlockingDisk device with the given size connected to the 
      MASTER or SLAVE slot of the primary ATA controller.
      NOTE: We are passing the _size argument out of laziness. 
      In a real system, we would infer this information from the 
      disk controller. */

   /* DISK OPERATIONS */
   bool disk_ready();
   bool is_wait_CPU();
   bool is_done();
   bool need_issued();
   
   Thread * get_head();
   Thread * dequeue();

   virtual void read(unsigned long _block_no, unsigned char * _buf);
   /* Reads 512 Bytes from the given block of the disk and copies them 
      to the given buffer. No error check! */

   virtual void write(unsigned long _block_no, unsigned char * _buf);
   /* Writes 512 Bytes from the buffer to the given block on the disk. */

};


/* MirroredDisk */
class MirroredDisk : public SimpleDisk{
private:
   Queue block_queue;
   bool wait_CPU;
protected:
   virtual void issue_operation(DISK_OPERATION _op, unsigned long _block_no, DISK_ID _disk_id);
   virtual void wait_until_ready();
   void _wtite(unsigned long _block_no, unsigned char * _buf, DISK_ID _disk_id);
public:
   MirroredDisk(unsigned int _size);
   bool disk_ready();
   bool is_wait_CPU();
   Thread * dequeue();
   virtual void read(unsigned long _block_no, unsigned char * _buf);
   virtual void write(unsigned long _block_no, unsigned char * _buf);
};

#endif
