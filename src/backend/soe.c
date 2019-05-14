#include "soe.h"
#include "soe_c.h"

#ifdef UNSAFE
#include "Enclave_dt.h"
#else
#include "Enclave_t.h"
#endif

#include "access/soe_heapam.h"
#include "storage/soe_bufmgr.h"
#include "storage/soe_hash_ofile.h"
#include "storage/soe_heap_ofile.h"
#include "logger/logger.h"

#include <oram/oram.h>
#include <oram/plblock.h>
#include <oram/stash.h>
#include <oram/pmap.h>
#include <oram/ofile.h>

// Bucket capacity
#define BKCAP 1

/* Predefined max tuple size for sgx to copy the real tuple to*/
#define MAX_TUPLE_SIZE 200

ORAMState stateTable = NULL;
ORAMState stateIndex = NULL;
VRelation oTable;
VRelation oIndex;



void initSOE(const char* tName, const char* iName, int tNBlocks, int iNBlocks,
    unsigned int tOid, unsigned int iOid){
    //selog(DEBUG1, "Initializing SOE for relation %s and index  %s", tName, iName);
    
    stateTable = initORAMState(tName, tNBlocks, &heap_ofileCreate);
    stateIndex = initORAMState(iName, iNBlocks, &hash_ofileCreate);
    oTable = InitVRelation(stateTable, tOid, tNBlocks, &heap_pageInit);
    oIndex = InitVRelation(stateIndex, iOid, iNBlocks, &hash_pageInit);
}

 ORAMState initORAMState(const char *name, int nBlocks, AMOFile* (*ofile)()){
 	size_t fileSize = nBlocks * BLCKSZ;

    Amgr* amgr = (Amgr*) malloc(sizeof(Amgr));
    amgr->am_stash = stashCreate();
    amgr->am_pmap = pmapCreate();
    amgr->am_ofile = ofile();

    return init(name, fileSize, BLCKSZ, BKCAP, amgr);
 }

void insert(const char* heapTuple, unsigned int size){
//    HeapTuple tupleData = (HeapTuple) heapTuple;

}

char* getTuple(const char* key, int scanKeySize){
   // ScanKey scankey = (ScanKey) key;
    return NULL;
}


void insertHeap(const char* heapTuple, unsigned int tupleSize){
    Item tuple = (Item) heapTuple;
    //selog(DEBUG1, "soe InsertHeapp tuplesize is %d", tupleSize);
    if(tupleSize <= MAX_TUPLE_SIZE){
        heap_insert(oTable, tuple, (uint32) tupleSize);
    }else{
        selog(WARNING, "Can't insert tuple of size %d", tupleSize);
    }
}

void getTupleTID(unsigned int blkno, unsigned int offnum, char* tuple, unsigned int tupleLen, char* tupleData, unsigned int tupleDataLen){

    HeapTuple heapTuple = (HeapTuple) malloc(sizeof(HeapTupleData));
    ItemPointerData tid;

    ItemPointerSet(&tid, BufferGetBlockNumber((BlockNumber) blkno),  (OffsetNumber) offnum);
    heap_gettuple(oTable, &tid, heapTuple);

    //selog(DEBUG1, "Input tuple len %d, output tuple len %d", tupleLen, heapTuple->t_len);

    if(heapTuple->t_len > MAX_TUPLE_SIZE){
        selog(ERROR, "Tuple len does not match %d != %d", tupleDataLen, heapTuple->t_len);
    }else{
        memcpy(tuple, (char*) heapTuple, sizeof(HeapTupleData));
        memcpy(tupleData, (char*) (heapTuple->t_data), tupleDataLen);
    }
    free(heapTuple);
}

/*
* This function is never used. 
* Should update the ORAM lib so its not necessary to create an empty function.
*/
AMOFile *ofileCreate(void){
    return NULL;
}
