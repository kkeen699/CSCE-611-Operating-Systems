/*
     File        : file_system.C

     Author      : Riccardo Bettati
     Modified    : 2021/11/28

     Description : Implementation of simple File System class.
                   Has support for numerical file identifiers.
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

#define INODES_BLOCK_NO 0
#define FREELIST_BLOCK_NO 1

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "console.H"
#include "file_system.H"

/*--------------------------------------------------------------------------*/
/* CLASS Inode */
/*--------------------------------------------------------------------------*/

/* You may need to add a few functions, for example to help read and store 
   inodes from and to disk. */

void Inode::initialization(FileSystem * _fs, long _file_id, unsigned long _index_block){
    fs = _fs;
    id = _file_id;
    index_block_no = _index_block;
    is_free = false;
    size = 0;
    num_block = 1;
}

void Inode::update(){
    fs->WriteDisk(INODES_BLOCK_NO, (unsigned char*)fs->inodes);
}

unsigned long Inode::allocate_block(){
    unsigned long block_no = fs->GetFreeBlock();
    if(block_no != SimpleDisk::BLOCK_SIZE){
        fs->WriteDisk(FREELIST_BLOCK_NO, fs->free_blocks);
    }
    return block_no;
}

/*--------------------------------------------------------------------------*/
/* CLASS FileSystem */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

FileSystem::FileSystem() {
    Console::puts("In file system constructor.\n");
    disk = NULL;
    inodes = new Inode[MAX_INODES];
    free_blocks = new unsigned char[SimpleDisk::BLOCK_SIZE];
}

FileSystem::~FileSystem() {
    Console::puts("unmounting file system\n");
    /* Make sure that the inode list and the free list are saved. */
    WriteDisk(INODES_BLOCK_NO, (unsigned char*)inodes);
    WriteDisk(FREELIST_BLOCK_NO, free_blocks);
    disk = NULL;
    delete []inodes;
    delete []free_blocks;
}


/*--------------------------------------------------------------------------*/
/* FILE SYSTEM FUNCTIONS */
/*--------------------------------------------------------------------------*/
Inode * FileSystem::GetFreeInode(){
    Inode * free_inode = NULL;

    for(int i = 0; i < MAX_INODES; i++){
        if(inodes[i].is_free){
            inodes[i].is_free = false;
            free_inode = &inodes[i];
            break;
        }
    }

    return free_inode;
}

unsigned long FileSystem::GetFreeBlock(){
    unsigned long free_block = SimpleDisk::BLOCK_SIZE;

    for(int i = 0; i < SimpleDisk::BLOCK_SIZE; i++){
        if(free_blocks[i] == 'F'){
            free_blocks[i] = 'U';
            free_block = i;
            break;
        }
    }

    return free_block;
}

bool FileSystem::Mount(SimpleDisk * _disk) {
    Console::puts("mounting file system from disk\n");

    /* Here you read the inode list and the free list into memory */
    if(disk){
        Console::puts("there is already a disk\n");
        return false;
    }

    disk = _disk;
    ReadDisk(INODES_BLOCK_NO, (unsigned char*)inodes);
    ReadDisk(FREELIST_BLOCK_NO, free_blocks);
    return true;
}

bool FileSystem::Format(SimpleDisk * _disk, unsigned int _size) { // static!
    Console::puts("formatting disk\n");
    /* Here you populate the disk with an initialized (probably empty) inode list
       and a free list. Make sure that blocks used for the inodes and for the free list
       are marked as used, otherwise they may get overwritten. */
    
    // initialize all inodes as free
    Inode *inode_buf = new Inode[MAX_INODES];
    for(int i = 0; i < MAX_INODES; i++){
        inode_buf[i].is_free = true;
    }
    _disk->write(INODES_BLOCK_NO, (unsigned char*)inode_buf);
    delete []inode_buf;

    // initialize all data block as free
    // except the first (inode list) and the second (free block list)
    unsigned char *block_buf = new unsigned char[SimpleDisk::BLOCK_SIZE];
    for(int i = 0; i < SimpleDisk::BLOCK_SIZE; i++){
        block_buf[i] = 'F';
    }
    block_buf[0] = 'U'; block_buf[1] = 'U';
    _disk->write(FREELIST_BLOCK_NO, block_buf);
    delete []block_buf;

    return true;
}

Inode * FileSystem::LookupFile(int _file_id) {
    Console::puts("looking up file with id = "); Console::puti(_file_id); Console::puts("\n");
    /* Here you go through the inode list to find the file. */
    
    Inode * inode = NULL;
    
    for(int i = 0; i < MAX_INODES; i++){
        if(inodes[i].id == _file_id && !inodes[i].is_free){
            inode = &inodes[i];
            break;
        }
    }

    return inode;
}

bool FileSystem::CreateFile(int _file_id) {
    Console::puts("creating file with id:"); Console::puti(_file_id); Console::puts("\n");
    /* Here you check if the file exists already. If so, throw an error.
       Then get yourself a free inode and initialize all the data needed for the
       new file. After this function there will be a new file on disk. */
    if(LookupFile(_file_id)){
        Console::puts("file already exists\n");
        return false;
    }

    // allocate an inode, a index block, and a data block
    Inode * inode = GetFreeInode();
    unsigned long index_block_no = GetFreeBlock();
    unsigned long data_block_no = GetFreeBlock();

    if(inode && index_block_no != SimpleDisk::BLOCK_SIZE &&
        data_block_no != SimpleDisk::BLOCK_SIZE){

        // initialize the inode
        inode->initialization(this, _file_id, index_block_no);

        // update inode list and free block list in disk
        WriteDisk(INODES_BLOCK_NO, (unsigned char*)inodes);
        WriteDisk(FREELIST_BLOCK_NO, free_blocks);

        // write data block number into index block
        unsigned long *index_block = new unsigned long[SimpleDisk::BLOCK_SIZE/4];
        index_block[0] = data_block_no;
        WriteDisk(index_block_no, (unsigned char*)index_block);
        delete []index_block;
        
        return true;
    }
    else{
        if(inode) 
            inode->is_free = true;
        if(index_block_no != SimpleDisk::BLOCK_SIZE)
            free_blocks[index_block_no] = 'F';
        if(data_block_no != SimpleDisk::BLOCK_SIZE)
            free_blocks[data_block_no] = 'F';
        Console::puts("fail to create a new file\n");
        return false;
    }
}

bool FileSystem::DeleteFile(int _file_id) {
    Console::puts("deleting file with id:"); Console::puti(_file_id); Console::puts("\n");
    /* First, check if the file exists. If not, throw an error. 
       Then free all blocks that belong to the file and delete/invalidate 
       (depending on your implementation of the inode list) the inode. */
    
    Inode * inode = LookupFile(_file_id); 
    
    if(!inode){
        Console::puts("file does not exist\n");
        return false;
    }

    // free the inode
    inode->is_free = true;

    // free the index block
    unsigned long index_block_no = inode->index_block_no;
    free_blocks[index_block_no] = 'F';
    
    // free all the data block
    unsigned long * index_block = new unsigned long[SimpleDisk::BLOCK_SIZE/4];
    ReadDisk(index_block_no, (unsigned char*)index_block);
    for(int i = 0; i < inode->num_block; i++){
        unsigned long block_no = index_block[i];
        free_blocks[block_no] = 'F';
    }
    delete []index_block;

    // update the inode list and free block list in disk
    WriteDisk(INODES_BLOCK_NO, (unsigned char*)inodes);
    WriteDisk(FREELIST_BLOCK_NO, free_blocks);
    
    return true;
}

void FileSystem::ReadDisk(unsigned long _block_no, unsigned char * _buf){
    disk->read(_block_no, _buf);
}

void FileSystem::WriteDisk(unsigned long _block_no, unsigned char * _buf){
    disk->write(_block_no, _buf);
}
