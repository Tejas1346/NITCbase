#include "OpenRelTable.h"
#include <stdlib.h>
#include <cstring>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

OpenRelTable::OpenRelTable() {
    // 1. Initialize all cache entries to nullptr and free
    for (int i = 0; i < MAX_OPEN; i++) {
        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;
        tableMetaInfo[i].free = true;
    }

    // 2. Set up Relation Cache for RELCAT and ATTRCAT
    RecBuffer relCatBlock(RELCAT_BLOCK);
    Attribute relCatRecord[RELCAT_NO_ATTRS];

    // RELCAT (relId 0)
    relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);
    RelCacheEntry* relCatRelEntry = (RelCacheEntry*)malloc(sizeof(RelCacheEntry));
    RelCacheTable::recordToRelCatEntry(relCatRecord, &(relCatRelEntry->relCatEntry));
    relCatRelEntry->recId.block = RELCAT_BLOCK;
    relCatRelEntry->recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;
    relCatRelEntry->dirty = false;
    relCatRelEntry->searchIndex = {-1, -1};
    RelCacheTable::relCache[RELCAT_RELID] = relCatRelEntry;

    // ATTRCAT (relId 1)
    relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);
    RelCacheEntry* attrCatRelEntry = (RelCacheEntry*)malloc(sizeof(RelCacheEntry));
    RelCacheTable::recordToRelCatEntry(relCatRecord, &(attrCatRelEntry->relCatEntry));
    attrCatRelEntry->recId.block = RELCAT_BLOCK;
    attrCatRelEntry->recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;
    attrCatRelEntry->dirty = false;
    attrCatRelEntry->searchIndex = {-1, -1};
    RelCacheTable::relCache[ATTRCAT_RELID] = attrCatRelEntry;

    // 3. Set up Attribute Cache for Relation Catalog (relId 0)
    RecBuffer attrCatBlock(ATTRCAT_BLOCK);
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    AttrCacheEntry* head = nullptr;
    AttrCacheEntry* current = nullptr;

    for (int i = 0; i < RELCAT_NO_ATTRS; i++) {
        attrCatBlock.getRecord(attrCatRecord, i);
        AttrCacheEntry* entry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(entry->attrCatEntry));
        entry->recId.block = ATTRCAT_BLOCK;
        entry->recId.slot = i;
        entry->dirty = false;
        entry->searchIndex = {-1, -1};
        entry->next = nullptr;

        if (head == nullptr) {
            head = entry;
            current = entry;
        } else {
            current->next = entry;
            current = entry;
        }
    }
    AttrCacheTable::attrCache[RELCAT_RELID] = head;

    // 4. Set up Attribute Cache for Attribute Catalog (relId 1)
    head = nullptr;
    current = nullptr;

    for (int i = 0; i < ATTRCAT_NO_ATTRS; ++i) {
        int slot = RELCAT_NO_ATTRS + i;
        attrCatBlock.getRecord(attrCatRecord, slot);

        AttrCacheEntry* entry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(entry->attrCatEntry));
        entry->recId.block = ATTRCAT_BLOCK;
        entry->recId.slot = slot;
        entry->dirty = false;
        entry->searchIndex = {-1, -1};
        entry->next = nullptr;

        if (head == nullptr) {
            head = entry;
            current = entry;
        } else {
            current->next = entry;
            current = entry;
        }
    }
    AttrCacheTable::attrCache[ATTRCAT_RELID] = head;

    // 5. Update OpenRelTable Meta Info for RELCAT and ATTRCAT
    tableMetaInfo[RELCAT_RELID].free = false;
    strcpy(tableMetaInfo[RELCAT_RELID].relName, (char*)RELCAT_RELNAME);
    tableMetaInfo[ATTRCAT_RELID].free = false;
    strcpy(tableMetaInfo[ATTRCAT_RELID].relName, (char*)ATTRCAT_RELNAME);
}

OpenRelTable::~OpenRelTable() {
    for (int i = 2; i < MAX_OPEN; i++) {
        if (!tableMetaInfo[i].free) {
            OpenRelTable::closeRel(i);
        }
    }

    for (int i = 0; i < MAX_OPEN; ++i) {
        if (RelCacheTable::relCache[i] != nullptr) {
            free(RelCacheTable::relCache[i]);
            RelCacheTable::relCache[i] = nullptr;
        }

        if (AttrCacheTable::attrCache[i] != nullptr) {
            AttrCacheEntry* curr = AttrCacheTable::attrCache[i];
            while (curr != nullptr) {
                AttrCacheEntry* next = curr->next;
                free(curr);
                curr = next;
            }
            AttrCacheTable::attrCache[i] = nullptr;
        }
    }
}

int OpenRelTable::getFreeOpenRelTableEntry() {
    for (int i = 2; i < MAX_OPEN; i++) {
        if (tableMetaInfo[i].free) {
            return i;
        }
    }
    return E_CACHEFULL;
}

int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {
    for (int i = 0; i < MAX_OPEN; i++) {
        if (!tableMetaInfo[i].free && strcmp(relName, tableMetaInfo[i].relName) == 0) {
            return i;
        }
    }
    return E_RELNOTOPEN;
}

int OpenRelTable::openRel(char relName[ATTR_SIZE]) {
    int ret = OpenRelTable::getRelId(relName);
    if (ret != E_RELNOTOPEN) {
        return ret;
    }

    int relId = OpenRelTable::getFreeOpenRelTableEntry();
    if (relId == E_CACHEFULL) {
        return E_CACHEFULL;
    }

    /****** Setting Relation Cache entry for relation ******/
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute attrVal;
    strcpy(attrVal.sVal, relName);

    RecId relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, attrVal, EQ);
    if (relcatRecId.block == -1 && relcatRecId.slot == -1) {
        return E_RELNOTEXIST;
    }

    Attribute relCatRecord[RELCAT_NO_ATTRS];
    RecBuffer relCatBlock(relcatRecId.block);
    relCatBlock.getRecord(relCatRecord, relcatRecId.slot);

    RelCacheEntry* relCacheEntry = (RelCacheEntry*)malloc(sizeof(RelCacheEntry));
    RelCacheTable::recordToRelCatEntry(relCatRecord, &(relCacheEntry->relCatEntry));
    relCacheEntry->recId = relcatRecId;
    relCacheEntry->dirty = false;
    relCacheEntry->searchIndex = {-1, -1};
    RelCacheTable::relCache[relId] = relCacheEntry;

    /****** Setting Attribute Cache entry for relation ******/
    AttrCacheEntry* listHead = nullptr;
    AttrCacheEntry* current = nullptr;
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    while (true) {
        RecId attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, attrVal, EQ);

        if (attrcatRecId.block == -1 && attrcatRecId.slot == -1) {
            break;
        }

        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        RecBuffer attrCatBlock(attrcatRecId.block);
        attrCatBlock.getRecord(attrCatRecord, attrcatRecId.slot);

        AttrCacheEntry* attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(attrCacheEntry->attrCatEntry));
        attrCacheEntry->recId = attrcatRecId;
        attrCacheEntry->dirty = false;
        attrCacheEntry->searchIndex = {-1, -1};
        attrCacheEntry->next = nullptr;

        if (listHead == nullptr) {
            listHead = attrCacheEntry;
            current = attrCacheEntry;
        } else {
            current->next = attrCacheEntry;
            current = current->next;
        }
    }
    AttrCacheTable::attrCache[relId] = listHead;

    /****** Setting up metadata for relation ******/
    tableMetaInfo[relId].free = false;
    strcpy(tableMetaInfo[relId].relName, relName);

    return relId;
}

int OpenRelTable::closeRel(int relId){
    if(relId==RELCAT_RELID||relId==ATTRCAT_RELID){
        return E_NOTPERMITTED;
    }
    if(relId<0||relId>=MAX_OPEN){
        return E_OUTOFBOUND;
    }
    if(tableMetaInfo[relId].free){
        return E_RELNOTOPEN;
    }

    if (RelCacheTable::relCache[relId] != nullptr) {
        free(RelCacheTable::relCache[relId]);
    }
    if (AttrCacheTable::attrCache[relId] != nullptr) {
        AttrCacheEntry* current = AttrCacheTable::attrCache[relId];
        while (current != nullptr) {
        AttrCacheEntry* next = current->next;
        free(current);
        current = next;
        }
    }
    tableMetaInfo[relId].free=1;
    RelCacheTable::relCache[relId] = nullptr;
    AttrCacheTable::attrCache[relId] = nullptr;
    return SUCCESS;
}