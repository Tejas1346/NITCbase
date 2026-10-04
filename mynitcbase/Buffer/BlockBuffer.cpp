#include "BlockBuffer.h"
#include <cstdlib>
#include <cstring>

BlockBuffer::BlockBuffer(int blockNum){
   this->blockNum=blockNum;
}
RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

int BlockBuffer::getHeader(struct HeadInfo *head){
    unsigned char* bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret!=SUCCESS){
        return ret;
    }
    memcpy(&head->numSlots, bufferPtr + 24, 4);
    memcpy(&head->numEntries, bufferPtr + 16, 4);
    memcpy(&head->numAttrs, bufferPtr + 20, 4);
    memcpy(&head->rblock, bufferPtr + 12, 4);
    memcpy(&head->lblock, bufferPtr + 8, 4);
    return SUCCESS;
}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char ** buffPtr) {
    
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

    
    if (bufferNum != E_BLOCKNOTINBUFFER) {
        // set the timestamp of the corresponding buffer to 0 and increment the
        // timestamps of all other occupied buffers in BufferMetaInfo.
        StaticBuffer::metainfo[bufferNum].timeStamp = 0;
        for (int i = 0; i < BUFFER_CAPACITY; i++) {
            if (i != bufferNum && !StaticBuffer::metainfo[i].free) {
                StaticBuffer::metainfo[i].timeStamp++;
            }
        }
    }
    
    else {
        // get a free buffer using StaticBuffer.getFreeBuffer()
        bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

        // if the call returns E_OUTOFBOUND, return E_OUTOFBOUND here as
        // the blockNum is invalid
        if (bufferNum == E_OUTOFBOUND) {
            return E_OUTOFBOUND;
        }

        // Read the block into the free buffer using readBlock()
        Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
    }

    // store the pointer to this buffer (blocks[bufferNum]) in *buffPtr
    *buffPtr = StaticBuffer::blocks[bufferNum];

    // return SUCCESS;
    return SUCCESS;
}

int RecBuffer::getRecord(union Attribute* rec,int slotNum){
    struct HeadInfo head;
    this->getHeader(&head);
    int attrCount = head.numAttrs;
    int slotCount=head.numSlots;
    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret!=SUCCESS){
        return ret;
    }
    int recordSize = attrCount*ATTR_SIZE;
    int offset = HEADER_SIZE+slotCount+(recordSize*slotNum);
    unsigned char *slotPointer = bufferPtr+offset;
    memcpy(rec,slotPointer,recordSize);
    return SUCCESS;
}


int RecBuffer::getSlotMap(unsigned char* slotmap){
    unsigned char* bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret!=SUCCESS){
        return ret;
    }

    struct HeadInfo head;
    getHeader(&head);
    int slotCount=head.numSlots;
    unsigned char* slotMapInBuffer = bufferPtr+HEADER_SIZE;
    memcpy(slotmap,slotMapInBuffer,slotCount);
    return SUCCESS;
}

int RecBuffer::setRecord(union Attribute *rec, int slotNum) {
    unsigned char *bufferPtr;
    
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS) {
        return ret;
    }

    
    HeadInfo header;
    this->getHeader(&header);

    
    int numAttrs = header.numAttrs;
    int numSlots = header.numSlots;

    // if input slotNum is not in the permitted range return E_OUTOFBOUND.
    if (slotNum < 0 || slotNum >= numSlots) {
        return E_OUTOFBOUND;
    }

    
    int recordSize = ATTR_SIZE * numAttrs;
    unsigned char *slotMap = bufferPtr + HEADER_SIZE;
    unsigned char *recordPtr = bufferPtr + HEADER_SIZE + numSlots + (slotNum * recordSize);
    
    memcpy(recordPtr, rec, recordSize);

    // update dirty bit using setDirtyBit()
    StaticBuffer::setDirtyBit(this->blockNum);

    return SUCCESS;
}

