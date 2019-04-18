#include "soe.h"

#include <oram/plblock.h>
#include <oram/oram.h>
#include <oram/stash.h>
#include <oram/pmap.h>
#include <oram/ofile.h>
#include "buffer/sgxbufmgr.h"

// Bucket capacity
#define BKCAP 1;


ORAMState stateTable = NULL;
ORAMState stateIndex = NULL;
VRelation table = NULL;
VRelation index = NULL;

void init(static char* tName, static char* iName, int tNBlocks, int iNBlocks){
    stateTable = initORAMState(tName, tNBlocks);
    stateIndex = initORAMState(iName, iNBlocks);
    table = initVRelation(stateTable);
    index = initVRelation(index);
}


 ORAMState initORAMState(static char *name, int nBlocks){

 	size_t fileSize = nblocks * BLCKSZ;
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

void insert(HeapTupleData heapTuple){
    

}

HeapTupleData getTuple(PScanKey scankey);