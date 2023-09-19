/*
 File: vm_pool.C
 
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

#include "vm_pool.H"
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
/* METHODS FOR CLASS   V M P o o l */
/*--------------------------------------------------------------------------*/

VMPool::VMPool(unsigned long  _base_address,
               unsigned long  _size,
               ContFramePool *_frame_pool,
               PageTable     *_page_table) {
    base_address = _base_address;
    size = _size;
    frame_pool = _frame_pool;
    page_table = _page_table;
    next = NULL;

    page_table->register_pool(this);

    // using the first page to store allocated list and free list
    // the first half for allocated list and the other for free list
    // allocated_list[0] stores these two lists
    allocated_list = (Region*) _base_address;
    allocated_size = 1;
    allocated_list[0].base_address = base_address;
    allocated_list[0].size = 1;

    // free_list[0] stores rest of the pool
    free_list = (Region*) (_base_address + PAGE_SIZE/2);
    free_size = 1;
    free_list[0].base_address = base_address + PAGE_SIZE;
    free_list[0].size = size/PAGE_SIZE - 1;
}

unsigned long VMPool::allocate(unsigned long _size) {
    // the size of allocated_list must be less than 256
    assert(allocated_size < 256);

    // calculate how many pages are needed
    unsigned long alloc_size = _size / PAGE_SIZE + (_size % PAGE_SIZE > 0 ? 1 : 0);

    // search proper memory region in free list 
    for(int i = 0; i < free_size; i++){
        if(free_list[i].size >= alloc_size){
            unsigned long ret_address = free_list[i].base_address;
            free_list[i].size -= alloc_size;
            
            // if the free region is greater than allocated size, update it
            if(free_list[i].size){
                free_list[i].base_address += alloc_size*PAGE_SIZE;
            }
            // if the free region is equal to allocated size, delete it
            else{
                free_list[i] = free_list[--free_size];
            }

            // update allocated list
            allocated_list[allocated_size].base_address = ret_address;
            allocated_list[allocated_size].size = alloc_size;
            allocated_size++;

            return ret_address;
        }
    }
    
    return 0;
}

void VMPool::release(unsigned long _start_address) {
    // the size of free_list must be less than 256
    assert(free_size < 256);

    for(int i = 0; i < allocated_size; i++){
        // find the region needed to release
        if(allocated_list[i].base_address = _start_address){
            // free all the pages in the region
            for(int j = 0; j < allocated_list[i].size; j++){
                page_table->free_page(_start_address + j * PAGE_SIZE);
            }
            
            // update allocated list and free list
            free_list[free_size] = allocated_list[i];
            free_size++;
            allocated_list[i] = allocated_list[--allocated_size];
            break;
        }
    }
    
}

bool VMPool::is_legitimate(unsigned long _address) {
    // The first page is used for allocated list and free list
    if( base_address <= _address && _address < base_address + PAGE_SIZE)
        return true;

    for(int i = 0; i < allocated_size; i++){
        Region r = allocated_list[i];
        // if _address is in allocated region, return true
        if(r.base_address <= _address && _address < (r.base_address + r.size*PAGE_SIZE))
            return true;
    }
    return false;
}

ContFramePool* VMPool::get_frame_pool(){
    return frame_pool;
}

