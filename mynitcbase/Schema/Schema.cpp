#include "Schema.h"

#include <cmath>
#include <cstring>

int Schema::openRel(char relName[ATTR_SIZE]){
    int ret = OpenRelTable::openRel(relName);
    if(ret>=0){
        return SUCCESS;
    }
    return ret;
}

int Schema::closeRel(char relName[ATTR_SIZE]){
    if(strcmp(relName,RELCAT_RELNAME)==0||strcmp(relName,ATTRCAT_RELNAME)==0){
        return E_NOTPERMITTED;
    }
    int relId=OpenRelTable::getRelId(relName);
    if(relId==E_RELNOTOPEN){
        return E_RELNOTOPEN;
    }
    return OpenRelTable::closeRel(relId);
}

int Schema::renameRel(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]) {
    // Check if the oldRelName or newRelName is either Relation Catalog or Attribute Catalog
    if (strcmp(oldRelName, RELCAT_RELNAME) == 0 || strcmp(oldRelName, ATTRCAT_RELNAME) == 0 ||
        strcmp(newRelName, RELCAT_RELNAME) == 0 || strcmp(newRelName, ATTRCAT_RELNAME) == 0) {
        return E_NOTPERMITTED;
    }

    // Check if the relation is open (if getRelId does NOT return E_RELNOTOPEN, it is open)
    if (OpenRelTable::getRelId(oldRelName) != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    // Rename the relation
    int retVal = BlockAccess::renameRelation(oldRelName, newRelName);
    return retVal;
}

int Schema::renameAttr(char *relName,char* oldRelName, char* newRelName){
    if (strcmp(oldRelName, RELCAT_RELNAME) == 0 || strcmp(oldRelName, ATTRCAT_RELNAME) == 0 ||
        strcmp(newRelName, RELCAT_RELNAME) == 0 || strcmp(newRelName, ATTRCAT_RELNAME) == 0) {
        return E_NOTPERMITTED;
    }
    if (OpenRelTable::getRelId(oldRelName) != E_RELNOTOPEN) {
        return E_RELOPEN;
    }

    // Rename the relation
    int retVal = BlockAccess::renameAttribute(relName,oldRelName, newRelName);
    return retVal;

}
