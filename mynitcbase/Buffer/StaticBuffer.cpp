#include "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

StaticBuffer::StaticBuffer() {
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
        // set metainfo[bufferindex] with the following values
        metainfo[bufferIndex].free = true;
        metainfo[bufferIndex].dirty = false;
        metainfo[bufferIndex].timeStamp = -1;
        metainfo[bufferIndex].blockNum = -1;
    }
}

// write back all modified blocks on system exit
StaticBuffer::~StaticBuffer() {
    /* iterate through all the buffer blocks,
       write back blocks with metainfo as free=false, dirty=true
       using Disk::writeBlock() */
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
        if (!metainfo[bufferIndex].free && metainfo[bufferIndex].dirty) {
            Disk::writeBlock(blocks[bufferIndex], metainfo[bufferIndex].blockNum);
        }
    }
}

int StaticBuffer::getFreeBuffer(int blockNum) {
    // Check if blockNum is valid (non zero and less than DISK_BLOCKS)
    // and return E_OUTOFBOUND if not valid.
    if (blockNum < 0 || blockNum >= DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }

    // increase the timeStamp in metaInfo of all occupied buffers.
    for (int i = 0; i < BUFFER_CAPACITY; i++) {
        if (!metainfo[i].free) {
            metainfo[i].timeStamp++;
        }
    }

    // let bufferNum be used to store the buffer number of the free/freed buffer.
    int bufferNum = -1;

    // iterate through metainfo and check if there is any buffer free
    for (int i = 0; i < BUFFER_CAPACITY; i++) {
        if (metainfo[i].free) {
            bufferNum = i;
            break;
        }
    }

    // if a free buffer is not available,
    //      find the buffer with the largest timestamp
    //      IF IT IS DIRTY, write back to the disk using Disk::writeBlock()
    //      set bufferNum = index of this buffer
    if (bufferNum == -1) {
        int maxTimeStamp = -1;
        for (int i = 0; i < BUFFER_CAPACITY; i++) {
            if (metainfo[i].timeStamp > maxTimeStamp) {
                maxTimeStamp = metainfo[i].timeStamp;
                bufferNum = i;
            }
        }

        if (metainfo[bufferNum].dirty) {
            Disk::writeBlock(blocks[bufferNum], metainfo[bufferNum].blockNum);
        }
    }

    // update the metaInfo entry corresponding to bufferNum with
    // free:false, dirty:false, blockNum:the input block number, timeStamp:0.
    metainfo[bufferNum].free = false;
    metainfo[bufferNum].dirty = false;
    metainfo[bufferNum].blockNum = blockNum;
    metainfo[bufferNum].timeStamp = 0;

    // return the bufferNum.
    return bufferNum;
}
int StaticBuffer::setDirtyBit(int blockNum){
    // find the buffer index corresponding to the block using getBufferNum().
    int bufferNum = getBufferNum(blockNum);

    // if block is not present in the buffer (bufferNum = E_BLOCKNOTINBUFFER)
    //      return E_BLOCKNOTINBUFFER
    if (bufferNum == E_BLOCKNOTINBUFFER) {
        return E_BLOCKNOTINBUFFER;
    }

    // if blockNum is out of bound (bufferNum = E_OUTOFBOUND)
    //      return E_OUTOFBOUND
    if (bufferNum == E_OUTOFBOUND) {
        return E_OUTOFBOUND;
    }

    // else
    //      (the bufferNum is valid)
    //      set the dirty bit of that buffer to true in metainfo
    metainfo[bufferNum].dirty = true;

    // return SUCCESS
    return SUCCESS;
}

int StaticBuffer::getBufferNum(int blockNum){
    if(blockNum<0||blockNum>DISK_BLOCKS){
        return E_OUTOFBOUND;
    }
    int bufferNum=-1;
    for(int i=0;i<BUFFER_CAPACITY;i++){
        if(metainfo[i].blockNum==blockNum){
            bufferNum=i;
            break;
        }
    }
    if(bufferNum!=-1) return bufferNum;
    else return E_BLOCKNOTINBUFFER;
}

