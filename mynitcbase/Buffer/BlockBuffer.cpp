#include "BlockBuffer.h"
#include <cstdlib>
#include <cstring>


BlockBuffer::BlockBuffer(char blockType){
    int blockTypeEnum;
    if(blockType=='R') blockTypeEnum=REC;
    else if(blockType=='I') blockTypeEnum=IND_INTERNAL;
    else  blockTypeEnum=IND_LEAF;
    int blockNum = BlockBuffer::getFreeBlock(blockTypeEnum);
    
    this->blockNum=blockNum;
}
BlockBuffer::BlockBuffer(int blockNum){
   this->blockNum=blockNum;
}
RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}
RecBuffer::RecBuffer() : BlockBuffer('R'){}


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
    memcpy(&head->pblock, bufferPtr + 4, 4);
    memcpy(&head->blockType,bufferPtr,4);
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

int BlockBuffer::setBlockType(int blockType){
    unsigned char* bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret!=SUCCESS) return ret;
    *((int32_t*)bufferPtr)=blockType;
    StaticBuffer::blockAllocMap[this->blockNum]=blockType;
    ret =StaticBuffer::setDirtyBit(this->blockNum);
    if(ret!=SUCCESS) return ret;
    return SUCCESS;
}

int BlockBuffer::getFreeBlock(int blockType){
    int freeBlock=-1;
    for(int i=0;i<DISK_BLOCKS;i++){
        if(StaticBuffer::blockAllocMap[i]==UNUSED_BLK){
            freeBlock=i;
            break;
        }
    }
    if(freeBlock==-1){
        return E_DISKFULL;
    }
    this->blockNum=freeBlock;
    int freeBuffer = StaticBuffer::getFreeBuffer(freeBlock);
    if(freeBuffer<0||freeBuffer>=BUFFER_CAPACITY){
        return freeBuffer;
    }
    struct HeadInfo head;
    //head blockType set in setBlockType so no need to mention it here
    head.pblock = -1;
    head.lblock = -1;
    head.rblock = -1;
    head.numEntries = 0;
    head.numAttrs = 0;
    head.numSlots = 0;
    
    
    setHeader(&head);
    setBlockType(blockType);
    return freeBlock;
}

int BlockBuffer::setHeader(struct HeadInfo* head){
    unsigned char* bufferPtr;
    int ret=loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret!=SUCCESS){
        return ret;
    }

    struct HeadInfo *bufferHeader=(struct HeadInfo*)bufferPtr;
    bufferHeader->blockType = head->blockType;
    bufferHeader->pblock = head->pblock;
    bufferHeader->lblock = head->lblock;
    bufferHeader->rblock = head->rblock;
    bufferHeader->numEntries = head->numEntries;
    bufferHeader->numAttrs = head->numAttrs;
    bufferHeader->numSlots = head->numSlots;
    ret = StaticBuffer::setDirtyBit(this->blockNum);
    if(ret!=SUCCESS){
        return ret;
    }
    return SUCCESS;
}

int RecBuffer::setSlotMap(unsigned char* slotMap){
    unsigned char* bufferPtr;
    loadBlockAndGetBufferPtr(&bufferPtr);
    struct HeadInfo header;
    getHeader(&header);
    int numSlots = header.numSlots;
    memcpy(bufferPtr+HEADER_SIZE,slotMap,numSlots);
    int ret=StaticBuffer::setDirtyBit(this->blockNum);
    if(ret!=SUCCESS) return ret;
    return SUCCESS;
}

int BlockBuffer::getBlockNum(){
    return this->blockNum;
}
