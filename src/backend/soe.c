#include "soe.h"
#include "Enclave_t.h"
#include "storage/soe_bufmgr.h"

#include <oram/oram.h>
#include <oram/plblock.h>
#include <oram/stash.h>
#include <oram/pmap.h>
#include <oram/ofile.h>

// Bucket capacity
#define BKCAP 1


ORAMState stateTable = NULL;
ORAMState stateIndex = NULL;
VRelation oTable;
VRelation oIndex;


void initSOE(const char* tName, const char* iName, int tNBlocks, int iNBlocks){
    stateTable = initORAMState(tName, tNBlocks);
    stateIndex = initORAMState(iName, iNBlocks);
    oTable = initVRelation(stateTable);
    oIndex = initVRelation(stateIndex);
}

 ORAMState initORAMState(const char *name, int nBlocks){

 	size_t fileSize = nBlocks * BLCKSZ;
	AMStash *stash = NULL;
    AMPMap *pmap = NULL;
    AMOFile *ofile = NULL;
    Amgr* amgr = (Amgr*) malloc(sizeof(Amgr));

    stash = stashCreate();
    pmap = pmapCreate();
    ofile = ofileCreate();

    amgr->am_stash = stash;
    amgr->am_pmap = pmap;
    amgr->am_ofile = ofile;

    return init(name, fileSize, BLCKSZ, BKCAP, amgr);
 }

void insert(const char* heapTuple){
    HeapTuple tupleData = (HeapTuple) heapTuple;

}

char* getTuple(const char* key){
    ScanKey scankey = (ScanKey) key;
    return NULL;
}