/*
 File: ContFramePool.C
 
 Author:
 Date  : 
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

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
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/

ContFramePool* ContFramePool::head = NULL;

ContFramePool::FrameState ContFramePool::get_state(unsigned long _frame_no){
    // 00: Free, 01: Used, 10: HoS
    unsigned int bitmap_index = _frame_no / 4;
    unsigned char mask = 0x1 << ((_frame_no % 4) * 2);
    unsigned char mask_head = 0x2 << ((_frame_no % 4) * 2);

    if((bitmap[bitmap_index] & mask_head) != 0){
        return FrameState::HoS;
    } else{
        return ((bitmap[bitmap_index] & mask) == 0) ? FrameState::Free : FrameState::Used;
    }
}

void ContFramePool::set_state(unsigned long _frame_no, FrameState _state){
    unsigned int bitmap_index = _frame_no / 4;
    unsigned char mask;
    switch(_state){
        case FrameState::Used:
            // 00 | 01 = 01
            mask = 0x1 << ((_frame_no % 4) * 2);
            bitmap[bitmap_index] |= mask;
            break;
        case FrameState::Free:
            // HoS: 10 & 00 = 00 or Used: 01 & 00 = 00
            mask = 0x3 << ((_frame_no % 4) * 2);
            bitmap[bitmap_index] &= (~mask);
            break;
        case FrameState::HoS:
            // 00 | 10 = 10
            mask = 0x2 << ((_frame_no % 4) * 2);
            bitmap[bitmap_index] |= mask;
            break;
    }
}

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no)
{
    // Bitmap must fit in a single frame!
    assert(_n_frames <= FRAME_SIZE*4);

    base_frame_no = _base_frame_no;
    n_frames = _n_frames;
    info_frame_no = _info_frame_no;
    nFreeFrames = _n_frames;
    next = NULL;

    // set the address of bitmap
    if(info_frame_no == 0){
        bitmap = (unsigned char*) (base_frame_no * FRAME_SIZE);
    } else{
        bitmap = (unsigned char*) (_info_frame_no * FRAME_SIZE);
    }
    
    // initial all frame as Free
    for(int fno = 0; fno < n_frames; fno++){
        set_state(fno, FrameState::Free);
    }

    // if bitmap is stored in this pool, set frame 0 as HoS
    if(_info_frame_no == 0){
        set_state(0, FrameState::HoS);
        nFreeFrames--;
    }

    // link all the ContFramePool
    if(head != NULL){
        ContFramePool *temp = head;
        while(temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = this;
    } else{
        head = this;
    }

    Console::puts("Frame Pool Initialized\n");
}

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    // There must be enough free frames
    assert(nFreeFrames > _n_frames);
    
    for(unsigned int frame_no = 0; frame_no < n_frames;){
        if(get_state(frame_no) == FrameState::Free){
            bool succ = true;
            for(int i = 1; i < _n_frames; i++){
                // if one frame is not Free, keep searching from the next frame 
                if(get_state(frame_no + i) != FrameState::Free){
                    frame_no = frame_no + i + 1;
                    succ = false;
                    break;
                }
            }
            if(succ){ 
                // if find contiguous frames, set their state to HoS or Used
                for(int i = 0; i < _n_frames; i++){
                    if(i == 0)
                        set_state(frame_no, FrameState::HoS);
                    else
                        set_state(frame_no + i, FrameState::Used);
                }
                nFreeFrames -= _n_frames;
                return (frame_no + base_frame_no);
            }
        }
        else{
            frame_no++;
        }
    }
    return 0;
}

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    // TODO: IMPLEMENTATION NEEEDED!
    for(int fno = _base_frame_no; fno < _base_frame_no + _n_frames; fno++){
        set_state(fno - base_frame_no, FrameState::HoS);
    }
}

void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    // TODO: IMPLEMENTATION NEEEDED!
    // determin which pool contains the frame
    ContFramePool *pool = head;
    while(pool){
        if(_first_frame_no >= pool->base_frame_no && 
           _first_frame_no <= pool->base_frame_no + pool->n_frames){
            pool->release_frame(_first_frame_no);
            break;
        } else{
            pool = pool->next;
        }
    }
}

void ContFramePool::release_frame(unsigned long _first_frame_no)
{
    int first_no = _first_frame_no - base_frame_no;
    // check the first frame state is HoS
    if(get_state(first_no) == FrameState::HoS){
        set_state(first_no, FrameState::Free);
        nFreeFrames++;
        int i = first_no + 1;
        while(get_state(i) == FrameState::Used){
            set_state(i, FrameState::Free);
            nFreeFrames++;
            i++;
        }
    }
}

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
    // TODO: IMPLEMENTATION NEEEDED!
    // 2 bits per frame
    // one frame could manege 4KB * 4 = 16KB frames
    return (_n_frames / (16*1024) + (_n_frames % (16*1024) > 0 ? 1 : 0));
}
