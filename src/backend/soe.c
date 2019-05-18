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
Amgr* tamgr;
Amgr* iamgr;
BlockNumber blkno;
OffsetNumber off;


void initSOE(const char* tName, const char* iName, int tNBlocks, int iNBlocks,
    unsigned int tOid, unsigned int iOid){
    //VALGRIND_DO_LEAK_CHECK;

    //selog(DEBUG1, "Initializing SOE for relation %s and index  %s", tName, iName);
    
    stateTable = initORAMState(tName, tNBlocks, &heap_ofileCreate, 0);
    stateIndex = initORAMState(iName, iNBlocks, &hash_ofileCreate, 1);
    oTable = InitVRelation(stateTable, tOid, tNBlocks, &heap_pageInit);
    oIndex = InitVRelation(stateIndex, iOid, iNBlocks, &hash_pageInit);

    blkno=0;
    off=1;
}

 ORAMState initORAMState(const char *name, int nBlocks, AMOFile* (*ofile)(), int isIndex){
    Amgr* amgr;
 	size_t fileSize = nBlocks * BLCKSZ;

    amgr = (Amgr*) malloc(sizeof(Amgr));
    amgr->am_stash = stashCreate();
    amgr->am_pmap = pmapCreate();
    amgr->am_ofile = ofile();

    if(isIndex == 0){
        tamgr = amgr; 
    }else{
        iamgr = amgr;
    }

    return init_oram(name, fileSize, BLCKSZ, BKCAP, amgr);
 }

void insert(const char* heapTuple, unsigned int size){
//    HeapTuple tupleData = (HeapTuple) heapTuple;

}

int getTuple(const char* key, int scanKeySize, char* tuple, unsigned int tupleLen, char* tupleData, unsigned int tupleDataLen){
    HeapTuple heapTuple = (HeapTuple) malloc(sizeof(HeapTupleData));
    ItemPointerData tid;
    int hasNext = 0;


    if(strcmp(key, "NEXT")==0){

         /**
         * If there are no more blocks or the current block has no more tuples.
         * The prototype assumes a sequential insertion.
         */ 
        if( blkno == oTable->totalBlocks || off - 1 >= oTable->fsm[blkno] ){
            free(heapTuple);
            return 1;
        }

        ItemPointerSet_s(&tid, blkno, off);
        heap_gettuple_s(oTable, &tid, heapTuple); 


        //If the current block still has tuples
        if(off + 1 <= oTable->fsm[blkno]){
            // continue to search on current block   
            off +=1;
        }else{
            //Move to the next block
            blkno += 1;
            off = 1;
        }
       

    }

     if(heapTuple->t_len > MAX_TUPLE_SIZE){
        selog(ERROR, "Tuple len does not match %d != %d", tupleDataLen, heapTuple->t_len);
    }else{
        selog(DEBUG1, "Going to copy tuple of size %d", heapTuple->t_len);
        memcpy(tuple, (char*) heapTuple, sizeof(HeapTupleData));
        memcpy(tupleData, (char*) (heapTuple->t_data), (heapTuple->t_len));
    }
    /*TODO: check if heapTuple->t_data should be freed*/
    free(heapTuple->t_data);
    free(heapTuple);


    return hasNext;
}


void insertHeap(const char* heapTuple, unsigned int tupleSize){

    
    Item tuple = (Item) heapTuple;
    if(tupleSize <= MAX_TUPLE_SIZE){
        selog(DEBUG1, "Going to insert tuple of size %d", tupleSize);
        heap_insert_s(oTable, tuple, (uint32) tupleSize);
    }else{
        selog(WARNING, "Can't insert tuple of size %d", tupleSize);
    }
    

}

void getTupleTID(unsigned int blkno, unsigned int offnum, char* tuple, unsigned int tupleLen, char* tupleData, unsigned int tupleDataLen){

    HeapTuple heapTuple = (HeapTuple) malloc(sizeof(HeapTupleData));
    ItemPointerData tid;

    ItemPointerSet_s(&tid, BufferGetBlockNumber_s((BlockNumber) blkno),  (OffsetNumber) offnum);
    heap_gettuple_s(oTable, &tid, heapTuple);

    //selog(DEBUG1, "Input tuple len %d, output tuple len %d", tupleLen, heapTuple->t_len);

    if(heapTuple->t_len > MAX_TUPLE_SIZE){
        selog(ERROR, "Tuple len does not match %d != %d", tupleDataLen, heapTuple->t_len);
    }else{
        selog(DEBUG1, "Going to copy tuple of size %d", tupleDataLen);
        memcpy(tuple, (char*) heapTuple, sizeof(HeapTupleData));
        memcpy(tupleData, (char*) (heapTuple->t_data), tupleDataLen);
    }
    free(heapTuple);
}


void closeSoe(){
    selog(DEBUG1, "Going to close soe");
    closeVRelation(oTable);
    closeVRelation(oIndex);
    free(tamgr);
    free(iamgr);
    //VALGRIND_DO_LEAK_CHECK;

}
/*
* This function is never used. 
* Should update the ORAM lib so its not necessary to create an empty function.
*/
AMOFile *ofileCreate(void){
    return NULL;
}
