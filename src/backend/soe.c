#include "soe.h"
#include "soe_c.h"
#ifdef UNSAFE
#include "Enclave_dt.h"
#else
#include "Enclave_t.h"
#endif

#include "ops.h"

#include "access/soe_heapam.h"
#include "storage/soe_bufmgr.h"
#include "access/soe_hash.h"
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
IndexScanDesc scan;

void initSOE(const char* tName, const char* iName, int tNBlocks, int iNBlocks,
    unsigned int tOid, unsigned int iOid, unsigned int functionOid, char* attrDesc, unsigned int attrDescLength){
    //VALGRIND_DO_LEAK_CHECK;

    //selog(DEBUG1, "Initializing SOE for relation %s and index  %s", tName, iName);
    stateTable = initORAMState(tName, tNBlocks, &heap_ofileCreate, 0);
    stateIndex = initORAMState(iName, iNBlocks, &hash_ofileCreate, 1);
    oTable = InitVRelation(stateTable, tOid, tNBlocks, &heap_pageInit);
    oIndex = InitVRelation(stateIndex, iOid, iNBlocks, &hash_pageInit);
    
    blkno=0;
    off=1;
  
    oIndex->foid = functionOid;
    oIndex->tDesc->natts = 1;
    oIndex->tDesc->attrs = (FormData_pg_attribute*) malloc(sizeof(struct FormData_pg_attribute));
    memcpy(oIndex->tDesc->attrs, attrDesc, attrDescLength);
    selog(DEBUG1, "the key length is %d", oIndex->tDesc->attrs->attlen);
    _hash_init_s(oIndex, 0);
    scan = NULL;
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

void insert(const char* heapTuple, unsigned int tupleSize,  const char* datum, unsigned int datumSize){

    HeapTuple hTuple = (HeapTuple) malloc(sizeof(HeapTupleData));

    
    Item tuple = (Item) heapTuple;
    if(tupleSize <= MAX_TUPLE_SIZE){
        //selog(DEBUG1, "Going to insert tuple of size %d", tupleSize);
        heap_insert_s(oTable, tuple, (uint32) tupleSize, hTuple);
        hashinsert_s(oIndex, &(hTuple->t_self), datum, datumSize);

    }else{
        selog(WARNING, "Can't insert tuple of size %d", tupleSize);
    }

    free(hTuple);
}

int getTuple(unsigned int opmode, const char* key, int scanKeySize, char* tuple, unsigned int tupleLen, char* tupleData, unsigned int tupleDataLen){
    
    HeapTuple heapTuple;
    ItemPointerData tid;
    int hasNext;
    bool hasMore = false;

    heapTuple = (HeapTuple) malloc(sizeof(HeapTupleData));
    hasNext = 0;

    if(opmode == TEST_MODE){

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
       

    }else{

        if(scan == NULL){
            scan = hashbeginscan_s(oIndex, key, scanKeySize);
        }

        hasMore =  hashgettuple_s(scan);

        if(!hasMore){
            hasNext = 0;
            selog(DEBUG1, "Going to free scan resources");
            hashendscan_s(scan);
            scan = NULL;
            free(heapTuple);
            return 1;
        }else{
            selog(DEBUG1, "Going to access the heap");
            tid =  scan->xs_ctup.t_self;
            heap_gettuple_s(oTable, &tid, heapTuple);
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

    HeapTuple hTuple = (HeapTuple) malloc(sizeof(HeapTupleData));

    
    Item tuple = (Item) heapTuple;
    if(tupleSize <= MAX_TUPLE_SIZE){
        selog(DEBUG1, "Going to insert tuple of size %d", tupleSize);
        heap_insert_s(oTable, tuple, (uint32) tupleSize, hTuple);

    }else{
        selog(WARNING, "Can't insert tuple of size %d", tupleSize);
    }

    free(hTuple);
    
}


void closeSoe(){
    selog(DEBUG1, "Going to close soe");
    closeVRelation(oTable);
    closeVRelation(oIndex);
    free(tamgr);
    free(iamgr);
}
/*
* This function is never used. 
* Should update the ORAM lib so its not necessary to create an empty function.
*/
AMOFile *ofileCreate(void){
    return NULL;
}
