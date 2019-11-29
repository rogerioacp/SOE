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
#include "storage/soe_ost_bufmgr.h"
#include "access/soe_hash.h"
#include "access/soe_nbtree.h"
#include "access/soe_ost.h"
#include "storage/soe_hash_ofile.h"
#include "storage/soe_heap_ofile.h"
#include "storage/soe_nbtree_ofile.h"
#include "storage/soe_ost_ofile.h"
#include "logger/logger.h"

#include <oram/oram.h>
#include <oram/plblock.h>
#include <oram/stash.h>
#include <oram/pmap.h>
#include <oram/ofile.h>


/*  Bucket capacity */
#define BKCAP 1

/* Predefined max tuple size for sgx to copy the real tuple to*/
#define MAX_TUPLE_SIZE 1400

ORAMState	stateTable = NULL;
ORAMState	stateIndex = NULL;

OSTreeState ostTable = NULL;

VRelation	oTable;
VRelation	oIndex;
OSTRelation ostIndex;

Amgr	   *tamgr;
Amgr	   *iamgr;


//Index scan global status
IndexScanDesc scan;



//Operation mode
Mode        mode;

void
initSOE(const char *tName, const char *iName, int tNBlocks, int iNBlocks,
		unsigned int tOid, unsigned int iOid, unsigned int functionOid, unsigned int indexOid, char *attrDesc, unsigned int attrDescLength)
{
	/* VALGRIND_DO_LEAK_CHECK; */


#ifdef SINGLE_ORAM
    /**
     *
     *  Simulates a single ORAM for both the Heap and Index. If there was just
     *  one oram for both files, then the size would be sum of both, and the
     *  oram requests would depend on the tree height for the size.
     *
     *  If both ORAMS have the sum of both sizes then the three height of both
     *  orams will be the same and the requests read/write the same number of
     *  blocks. Thus, it has the same bandwidth overhead. Furthermore, this
     *  approach does not require changing any other part of the code and is
     *  suffient for the paper tests.
     *
     */
    tNBlocks += iNBlocks;
    iNBlocks += tNBlocks;
#endif
	selog(DEBUG1, "Initializing SOE for relation %s and index  %s", tName, iName);
	stateTable = initORAMState(tName, tNBlocks, &heap_ofileCreate, true);
	oTable = InitVRelation(stateTable, tOid, tNBlocks, &heap_pageInit);

	if (indexOid == F_HASHHANDLER)
	{
		selog(DEBUG1, "going to init oblivious hash file");
		stateIndex = initORAMState(iName, iNBlocks, &hash_ofileCreate, false);
		oIndex = InitVRelation(stateIndex, iOid, iNBlocks, &hash_pageInit);
	}
	else if (indexOid == F_BTHANDLER)
	{
		selog(DEBUG1, "going to init nbtree oblivious heap file");
		stateIndex = initORAMState(iName, iNBlocks, &nbtree_ofileCreate, false);
		oIndex = InitVRelation(stateIndex, iOid, iNBlocks, &nbtree_pageInit);
	}
	else
	{
		selog(ERROR, "unsupported index");
	}

	oIndex->foid = functionOid;
	oIndex->indexOid = indexOid;
	oIndex->tDesc->natts = 1;
	oIndex->tDesc->attrs = (FormData_pg_attribute *) malloc(sizeof(struct FormData_pg_attribute));
	memcpy(oIndex->tDesc->attrs, attrDesc, attrDescLength);
	/* selog(DEBUG1, "the key length is %d", oIndex->tDesc->attrs->attlen); */
	if (indexOid == F_HASHHANDLER)
	{
		oIndex->tDesc->isnbtree = false;
		/* selog(DEBUG1, "Going to init hash metapage"); */
		_hash_init_s(oIndex, 0);
	}
	else if (indexOid == F_BTHANDLER)
	{
		oIndex->tDesc->isnbtree = true;
		/* selog(DEBUG1, "Going to init nbtree metapage"); */
		_bt_initmetapage_s(oIndex, P_NONE, 0);
	}
	else
	{
		selog(ERROR, "unsupported index");
	}
	scan = NULL;
    mode = DYNAMIC;
}

void
initFSOE(const char *tName, const char *iName, int tNBlocks, int *fanouts, int nlevels, unsigned int tOid, unsigned int iOid, char *attrDesc, unsigned int attrDescLength)
{


	selog(DEBUG1, "Initializing FSOE for relation %s and index %s", tName, iName);
	stateTable = initORAMState(tName, tNBlocks, &heap_ofileCreate, true);
	oTable = InitVRelation(stateTable, tOid, tNBlocks, &heap_pageInit);

	/* Handle the initialization of the tree index. */

	ostTable = initOSTreeProtocol(iName, iOid, fanouts, nlevels, &ost_ofileCreate);


	/* By default a single attribute is used to compare elements in the tree. */
	ostIndex = InitOSTRelation(ostTable, iOid, attrDesc, attrDescLength);

	scan = NULL;
    mode = OST;
}

ORAMState
initORAMState(const char *name, int nBlocks, AMOFile * (*ofile) (), bool isHeap)
{


	size_t		fileSize = nBlocks * BLCKSZ;
	Amgr	   *amgr;
	ORAMState	state;

	amgr = (Amgr *) malloc(sizeof(Amgr));
	amgr->am_stash = stashCreate();
	amgr->am_pmap = pmapCreate();
	amgr->am_ofile = ofile();

	if (isHeap)
	{
		tamgr = amgr;
	}
	else
	{
		iamgr = amgr;
	}

	state = init_oram(name, fileSize, BLCKSZ, BKCAP, amgr, NULL);
	return state;
}


OSTreeState
initOSTreeProtocol(const char *name, unsigned int iOid, int *fanouts, int nlevels, AMOFile * (*ofile) ())
{

	int			i;
	int			namelen;

	OSTreeState ost = (OSTreeState) malloc(sizeof(struct OSTreeState));

	ost->fanouts = (int *) malloc(sizeof(int) * nlevels);
	memcpy(ost->fanouts, fanouts, sizeof(int) * nlevels);
	ost->nlevels = nlevels;
	ost->iOid = iOid;
	namelen = strlen(name) + 1;
	ost->iname = (char *) malloc(namelen);
	memcpy(ost->iname, name, namelen);


	ost->orams = (ORAMState *) malloc(sizeof(ORAMState) * nlevels);

	ost_status(ost);

	for (i = 0; i < nlevels; i++)
	{
		size_t		fileSize = BLCKSZ * fanouts[i];
		Amgr	   *amgr;

		amgr = (Amgr *) malloc(sizeof(Amgr));
		amgr->am_stash = stashCreate();
		amgr->am_pmap = pmapCreate();
		amgr->am_ofile = ofile();
		/* setclevel(i); */

		/*
		 * selog(DEBUG1, "Initiating ORAM on level %d with filesize %d", i,
		 * fileSize);
		 */
		ost->orams[i] = init_oram(name, fileSize, BLCKSZ, BKCAP, amgr, &i);
	}

	return ost;
}

void
insert(const char *heapTuple, unsigned int tupleSize, char *datum, unsigned int datumSize)
{

	HeapTuple	hTuple = (HeapTuple) malloc(sizeof(HeapTupleData));
	int			trimmedSize = (datumSize + 1) * sizeof(char);
	char	   *trimedDatum = (char *) malloc(trimmedSize);

	memcpy(trimedDatum, datum, datumSize);
	trimedDatum[datumSize] = '\0';

	/*
	 * selog(DEBUG1, "Going to insert key %s with size %d and original %d",
	 * trimedDatum, datumSize+1, datumSize);
	 */
	Item		tuple = (Item) heapTuple;

	if (tupleSize <= MAX_TUPLE_SIZE)
	{
		/* selog(DEBUG1, "Going to insert tuple of size %d", tupleSize); */
		heap_insert_s(oTable, tuple, (uint32) tupleSize, hTuple);
		if (oIndex->indexOid == F_HASHHANDLER)
		{
			/* selog(DEBUG1, "Going to insert tuple in hash index"); */
			hashinsert_s(oIndex, &(hTuple->t_self), trimedDatum, datumSize + 1);
		}
		else if (oIndex->indexOid == F_BTHANDLER)
		{
			/* selog(DEBUG1, "Going to insert tuple in btree index"); */
			btinsert_s(oIndex, oTable, &(hTuple->t_self), trimedDatum, datumSize + 1);
		}

	}
	else
	{
		selog(WARNING, "Can't insert tuple of size %d", tupleSize);
	}

	free(hTuple);
	free(trimedDatum);
}



void
addIndexBlock(char *block, unsigned int blocksize, unsigned int offset, unsigned int level)
{

	insert_ost(ostIndex, block, level, offset);
}

void
addHeapBlock(char *block, unsigned int blockSize, unsigned int blkno)
{
	heap_insert_block_s(oTable, block);
}

int
getTuple(unsigned int opmode, unsigned int opoid, const char *key, int scanKeySize, char *tuple, unsigned int tupleLen, char *tupleData, unsigned int tupleDataLen)
{

	HeapTuple	heapTuple;
	ItemPointerData tid;
    ItemPointer dtid;
	int			hasNext;
	char	   *trimedKey;
    bool        matchFound  = false;

    heapTuple = (HeapTuple) malloc(sizeof(HeapTupleData));
	hasNext = 0;

    trimedKey = (char *) malloc(scanKeySize + 1);
    memcpy(trimedKey, key, scanKeySize);
	trimedKey[scanKeySize] = '\0';
    

    //Stop everything. Resources have to be freed correctly.
    if(strcmp(key, "HALT")==0){
        selog(DEBUG1, "Received Halt signal from client");
        free(trimedKey);
        return 1;
    }

    if(scan == NULL){
        /*Old request is complete. Start new input request*/
        if(mode == DYNAMIC){
            scan = btbeginscan_s(oIndex, trimedKey, scanKeySize + 1);
        }else{
		    scan = btbeginscan_ost(ostIndex, trimedKey, scanKeySize + 1);
        }
        scan->opoid = opoid;
    }

    matchFound = mode == DYNAMIC? btgettuple_s(scan): btgettuple_ost(scan);

    if(matchFound){
        tid = scan->xs_ctup.t_self;
        heap_gettuple_s(oTable, &tid, heapTuple);
    }else{
        mode == DYNAMIC ? btendscan_s(scan) : btendscan_ost(scan);
        scan = NULL;

        #ifdef DUMMYS
            dtid = (ItemPointer) malloc(sizeof(struct ItemPointerData));
            ItemPointerSet_s(dtid, 0, 1);
            heap_gettuple_s(oTable, dtid, heapTuple);
            free(dtid);
        #else
            free(heapTuple);
            free(trimedKey);
            return 1;
        #endif
    }


    if (heapTuple->t_len > MAX_TUPLE_SIZE){
		    selog(ERROR, "Tuple len does not match %d != %d", tupleDataLen, heapTuple->t_len);
	}else{
		memcpy(tuple, (char *) heapTuple, sizeof(HeapTupleData));
		memcpy(tupleData, (char *) (heapTuple->t_data), (heapTuple->t_len));
	}
    
    free(trimedKey);
    free(heapTuple->t_data);
    free(heapTuple);
    return 0;
}


void
insertHeap(const char *heapTuple, unsigned int tupleSize)
{

	HeapTuple	hTuple = (HeapTuple) malloc(sizeof(HeapTupleData));


	Item		tuple = (Item) heapTuple;

	if (tupleSize <= MAX_TUPLE_SIZE)
	{
		heap_insert_s(oTable, tuple, (uint32) tupleSize, hTuple);

	}
	else
	{
		selog(WARNING, "Can't insert tuple of size %d", tupleSize);
	}

	free(hTuple);

}


void
closeSoe()
{
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
AMOFile *
ofileCreate(void)
{
	return NULL;
}
