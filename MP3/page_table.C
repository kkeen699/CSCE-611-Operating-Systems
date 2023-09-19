#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

PageTable * PageTable::current_page_table = NULL;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = NULL;
ContFramePool * PageTable::process_mem_pool = NULL;
unsigned long PageTable::shared_size = 0;



void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
                            const unsigned long _shared_size)
{
   kernel_mem_pool = _kernel_mem_pool;
   process_mem_pool = _process_mem_pool;
   shared_size = _shared_size;
}

PageTable::PageTable()
{
   // get a frame from kernel pool to store page directory
   unsigned long directory_frame = kernel_mem_pool->get_frames(1);
   page_directory = (unsigned long*) (directory_frame * PAGE_SIZE);
   
   // set all PDE as invalid, read/write, and supervisor level
   for(int i = 0; i < ENTRIES_PER_PAGE; i++){
      page_directory[i] = 0x2;
   }

   // get a frame from kernel pool to store page table page for the first 4MB
   unsigned long pt_frame = kernel_mem_pool->get_frames(1);
   unsigned long* page_table = (unsigned long*) (pt_frame * PAGE_SIZE);

   // the first 4MB is direct-mapped
   // set PTE as valid, read/write, and supervisor level 
   unsigned long address = 0x0;
   for(int i = 0; i < ENTRIES_PER_PAGE; i++){
      page_table[i] = address | 0x3;
      address += PAGE_SIZE;
   }

   // update the first PDE and set it as valid
   page_directory[0] = ((unsigned long) page_table) | 0x3;
}


void PageTable::load()
{
   write_cr3((unsigned long) page_directory);
   current_page_table = this;
}

void PageTable::enable_paging()
{
   write_cr0(read_cr0() | 0x80000000);
   paging_enabled = 1;
}

void PageTable::handle_fault(REGS * _r)
{
  unsigned long logic_address = read_cr2();
  current_page_table->page_fault(logic_address);
}

void PageTable::page_fault(unsigned long logic_address)
{
   int pt_no = logic_address >> 22;
   // if PDE is invalid, create a new page table
   if((page_directory[pt_no] & 0x1) == 0){
      // get a frame from kernel pool and store it in page directory
      unsigned long pt_frame = kernel_mem_pool->get_frames(1);
      page_directory[pt_no] = (pt_frame * PAGE_SIZE) | 0x3;

      // initial all PTE as invalid
      unsigned long* new_page_table = (unsigned long*) (pt_frame * PAGE_SIZE);
      for(int i = 0; i < ENTRIES_PER_PAGE; i++){
         new_page_table[i] = 0x0 | 0x2;
      }
   }

   // get a frame from process pool
   // store it in page table and set it as valid
   int p_no = (logic_address >> 12) & 0x3ff;
   unsigned long* page_table = (unsigned long*) (page_directory[pt_no] & 0xfffff000);
   if((page_table[p_no] & 0x1) == 0){
      unsigned long p_frame = process_mem_pool->get_frames(1);
      page_table[p_no] = (p_frame * PAGE_SIZE) | 0x3;
   }
}

