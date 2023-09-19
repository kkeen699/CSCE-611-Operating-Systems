/*
     File        : file.C

     Author      : Riccardo Bettati
     Modified    : 2021/11/28

     Description : Implementation of simple File class, with support for
                   sequential read/write operations.
*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "console.H"
#include "file.H"

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR/DESTRUCTOR */
/*--------------------------------------------------------------------------*/

File::File(FileSystem *_fs, int _id) {
    Console::puts("Opening file.\n");
    fs = _fs;
    inode = fs->LookupFile(_id);
    current_pos = 0;
    current_block = 0;
    // read the index block
    fs->ReadDisk(inode->index_block_no, (unsigned char*) index_block);
    // read the first data block
    fs->ReadDisk(index_block[current_block], block_cache);
}

File::~File() {
    Console::puts("Closing file.\n");
    /* Make sure that you write any cached data to disk. */
    /* Also make sure that the inode in the inode list is updated. */
    inode->update();
    fs->WriteDisk(index_block[current_block], block_cache);
    // update the index block
    fs->WriteDisk(inode->index_block_no, (unsigned char*) index_block);
}

/*--------------------------------------------------------------------------*/
/* FILE FUNCTIONS */
/*--------------------------------------------------------------------------*/

int File::Read(unsigned int _n, char *_buf) {
    Console::puts("reading from file\n");
    
    int count = 0;

    while(count < _n && !EoF()){
        unsigned int block = current_pos / SimpleDisk::BLOCK_SIZE;
        unsigned int idx = current_pos % SimpleDisk::BLOCK_SIZE;
        
        // if the current block is finished 
        // read the next data block from the disk
        if(block > current_block){
            current_block = block;
            fs->ReadDisk(index_block[current_block], block_cache);
        }

        _buf[count] = block_cache[idx];
        count++;
        current_pos++;
    }

    return count;
}

int File::Write(unsigned int _n, const char *_buf) {
    Console::puts("writing to file\n");
    
    int count = 0;
    
    while(count < _n && current_pos < FileSystem::FILE_SIZE){
        unsigned int block = current_pos / SimpleDisk::BLOCK_SIZE;
        unsigned int idx = current_pos % SimpleDisk::BLOCK_SIZE;

        // if the current block is finished
        // get the next data block
        if(block > current_block){
            fs->WriteDisk(index_block[current_block], block_cache);
            current_block = block;

            // if there are more data block in the file, read the next one
            // if not, allocate a new data block
            if(current_block < inode->num_block){
                fs->ReadDisk(index_block[current_block], block_cache);
            }
            else{
                unsigned long new_block = inode->allocate_block();
                if(new_block != SimpleDisk::BLOCK_SIZE){
                    // update the index block and the inode in disk
                    index_block[current_block] = new_block;
                    fs->WriteDisk(inode->index_block_no, (unsigned char*) index_block);
                    inode->num_block++;
                    inode->update();
                }
                else{
                    Console::puts("fail to allocate a data block\n");
                    return count;
                }
            }
        }

        block_cache[idx] = _buf[count];
        count++;
        current_pos++;
    }
    
    if(current_pos > inode->size)
        inode->size = current_pos;
    
    return count;
}

void File::Reset() {
    Console::puts("resetting file\n");
    current_pos = 0;
    
    // if the current block is not the first data block,
    // store the data into disk and read the first data block
    if(current_block != 0){
        fs->WriteDisk(index_block[current_block], block_cache);
        current_block = 0;
        fs->ReadDisk(index_block[current_block], block_cache);
    }
}

bool File::EoF() {
    // Console::puts("checking for EoF\n");
    return (current_pos == inode->size);
}
