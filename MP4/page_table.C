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
    vmpool = NULL;

    // get a frame from process pool to store page directory
    unsigned long directory_frame = process_mem_pool->get_frames(1);
    page_directory = (unsigned long*) (directory_frame * PAGE_SIZE);

    // set all PDE as invalid, read/write, and supervisor level
    for(int i = 0; i < ENTRIES_PER_PAGE; i++){
        page_directory[i] = 0x2;
    }

    // the last PDF to point the dirctory itself
    page_directory[ENTRIES_PER_PAGE-1] = ((unsigned long) page_directory) | 0x3;

    // get a frame from process pool to store page table page for the first 4MB
    unsigned long pt_frame = process_mem_pool->get_frames(1);
    unsigned long* page_table = (unsigned long*) (pt_frame * PAGE_SIZE);

    // the first 4MB is direct-mapped
    // set PTE as valid, read/write, and supervisor level 
    unsigned long address = 0x0;
    for(int i = 0; i < ENTRIES_PER_PAGE; i++){
        page_table[i] = address | 0x3;
        address += PAGE_SIZE;
    }

    // set the first PDE as valid
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
    unsigned long logical_address = read_cr2();
    current_page_table->page_fault(logical_address);
}

void PageTable::page_fault(unsigned long logical_address)
{   
    bool legitimate = false;
    VMPool *vm = vmpool;
    // check the logical_address is legitimate or not
    while(vm){
        legitimate = vm->is_legitimate(logical_address);
        if(legitimate) break;
        vm = vm->next;
    }

    if(legitimate){
        unsigned long* pde = PDE_address(logical_address);
        // if PDE is invalid, create a new page table
        if((*pde & 0x1) == 0){
            // get a frame from process pool and store it in page directory
            unsigned long pt_frame = vmpool->get_frame_pool()->get_frames(1);
            *pde = (pt_frame * PAGE_SIZE) | 0x3;

            // initial all PTE as invalid
            unsigned long* new_pte = PTE_address(logical_address & 0xFFC00000);
            for(int i = 0; i < ENTRIES_PER_PAGE; i++){
                new_pte[i] = 0x0 | 0x2;
            }
        }

        // get a frame from process pool
        // store it in page table and set it as valid
        unsigned long* pte = PTE_address(logical_address);
        if((*pte & 0x1) == 0){
            unsigned long p_frame = vmpool->get_frame_pool()->get_frames(1);
            *pte = (p_frame * PAGE_SIZE) | 0x3;
        }
    }
}

unsigned long* PageTable::PDE_address(unsigned long addr)
{
    return (unsigned long*) (0xFFFFF000 + (addr >> 22 << 2));
}

unsigned long* PageTable::PTE_address(unsigned long addr)
{
    return (unsigned long*) (0xFFC00000 + (addr >> 12 << 2));
}

void PageTable::register_pool(VMPool * _vm_pool)
{
    _vm_pool->next = vmpool;
    vmpool = _vm_pool;
}

void PageTable::free_page(unsigned long _page_no) {
    unsigned long* pte = PTE_address(_page_no);
    if((*pte & 0x1) == 1){
        ContFramePool::release_frames(*pte >> 12);
        *pte = 0x0 | 0x2;
        write_cr3((unsigned long) page_directory);
    }
}
