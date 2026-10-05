#include "RelCacheTable.h"

#include <cstring>

RelCacheEntry* RelCacheTable::relCache[MAX_OPEN];

int RelCacheTable::getRelCatEntry(int relId,RelCatEntry* relCatBuf){
    if(relId<0||relId>=MAX_OPEN){
        return E_OUTOFBOUND;
    }
    if(relCache[relId]==nullptr){
        return E_RELNOTOPEN;
    }
    *relCatBuf=relCache[relId]->relCatEntry;
    return SUCCESS;
}

void RelCacheTable::recordToRelCatEntry(union Attribute record[RELCAT_NO_ATTRS],RelCatEntry* relCatEntry){
    strcpy(relCatEntry->relName,record[RELCAT_REL_NAME_INDEX].sVal);
    relCatEntry->numAttrs=(int) record[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
    relCatEntry->numRecs = (int)record[RELCAT_NO_RECORDS_INDEX].nVal;
    relCatEntry->firstBlk = (int)record[RELCAT_FIRST_BLOCK_INDEX].nVal;
    relCatEntry->lastBlk = (int)record[RELCAT_LAST_BLOCK_INDEX].nVal;
    relCatEntry->numSlotsPerBlk = (int)record[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal;
}

int RelCacheTable::getSearchIndex(int relId,RecId* searchIndex){
    if(relId<0||relId>=MAX_OPEN){
        return E_OUTOFBOUND;
    }
    if(relCache[relId]==nullptr){
        return E_RELNOTOPEN;
    }
    *searchIndex=relCache[relId]->searchIndex;
    return SUCCESS;
}

int RelCacheTable::setSearchIndex(int relId,RecId* searchIndex){
    if(relId<0||relId>MAX_OPEN){
        return E_OUTOFBOUND;
    }
    if(relCache[relId]==nullptr){
        return E_RELNOTOPEN;
    }
    relCache[relId]->searchIndex=*searchIndex;
    return SUCCESS;
}

int RelCacheTable::resetSearchIndex(int relId){
    RecId reset;
    reset.block=-1;
    reset.slot=-1;
    return setSearchIndex(relId,&reset);
}

int RelCacheTable::setRelCatEntry(int relId,RelCatEntry* relCatBuf){
    if(relId<0||relId>=MAX_OPEN){
        return E_OUTOFBOUND;
    }    
    if(relCache[relId]==nullptr){
        return E_RELNOTOPEN;
    }
    relCache[relId]->relCatEntry=*relCatBuf;
    relCache[relId]->dirty=1;
    return SUCCESS;
}

void RelCacheTable::relCatEntryToRecord(RelCatEntry *relCatEntry, union Attribute record[RELCAT_NO_ATTRS]) {
    // Copy the relation name into the first attribute (string)
    strcpy(record[0].sVal, relCatEntry->relName);

    // Copy the number of attributes
    record[1].nVal = relCatEntry->numAttrs;

    // Copy the number of records
    record[2].nVal = relCatEntry->numRecs;

    // Copy the first block number of the relation
    record[3].nVal = relCatEntry->firstBlk;

    // Copy the last block number of the relation
    record[4].nVal = relCatEntry->lastBlk;

    // Copy the total block count of the relation
    record[5].nVal = relCatEntry->numSlotsPerBlk;
}