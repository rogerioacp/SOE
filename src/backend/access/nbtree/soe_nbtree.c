/*-------------------------------------------------------------------------
 *
 * soe_nbtree.c
 * Bare bones copy of the Implementation of Lehman and Yao's btree management
 * algorithm for Postgres. This implementation iis developed to be executed
 * inside a secure enclave.
 *
 * NOTES
 *	  This file contains only the public interface routines.
 *
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/nbtree/nbtree.c
 *
 *-------------------------------------------------------------------------
 */

#ifdef UNSAFE
#include "Enclave_dt.h"
#else
#include "sgx_trts.h"
#include "Enclave_t.h"
#endif

#include "access/soe_nbtree.h"
#include "logger/logger.h"
#include "storage/soe_nbtree_ofile.h"
#include "storage/soe_bufpage.h"

#include <oram/plblock.h>
#include <string.h>
#include <stdlib.h>


#include "common/soe_prf.h"

/*
 * BTPARALLEL_NOT_INITIALIZED indicates that the scan has not started.
 *
 * BTPARALLEL_ADVANCING indicates that some process is advancing the scan to
 * a new page; others must wait.
 *
 * BTPARALLEL_IDLE indicates that no backend is currently advancing the scan
 * to a new page; some process can start doing that.
 *
 * BTPARALLEL_DONE indicates that the scan is complete (including error exit).
 * We reach this state once for every distinct combination of array keys.
 */


void btree_load_s(VRelation indexRel, char* block, unsigned int level, unsigned int offset)
{

    Buffer          buffer;
    Page            page;
    BTPageOpaque    oopaque;
    unsigned int    token[8];
   


    memset(&token, 0, sizeof(unsigned int)*4);
    oopaque = (BTPageOpaque) PageGetSpecialPointer_s((Page) block);
    memset(oopaque->counters, 0, sizeof(uint32)*300);

    prf(level, offset, 0, (unsigned char*) &token);
    
    //selog(DEBUG1, "size of btree opaque data is %d\n", sizeof(BTPageOpaqueData));
    //selog(DEBUG1, "Counters are %d %d %d %d\n", token[0], token[1], token[2], token[3]);
    //selog(DEBUG1, "btree block at level %d and offset %d has o_blkno %d\n", level, offset, oopaque->o_blkno);

    indexRel->level = level;
    indexRel->token = token;

    buffer = _bt_getbuf_level_s(indexRel, offset); 

    page = BufferGetPage_s(indexRel, buffer);

    memcpy(page, block, BLCKSZ);

   // oopaque = (BTPageOpaque) PageGetSpecialPointer_s((Page) block);

    //selog(DEBUG1, "btree block after initialization at level %d and offset %d has o_blkno %d\n", level, offset, oopaque->o_blkno);
    
    prf(level, offset, 1, (unsigned char*) &token);

    //selog(DEBUG1, "Counters are %d %d %d %d\n", token[0], token[1], token[2], token[3]);
    //indexRel->token = token;

    MarkBufferDirty_s(indexRel, buffer);
    ReleaseBuffer_s(indexRel, buffer);
    //indexRel->rCounter +=2;
    //free(token);

}

/*
 *	btinsert() -- insert an index tuple into a btree.
 *
 *		Descend the tree recursively, find the appropriate location for our
 *		new tuple, and put it there.
 */
bool
btinsert_s(VRelation indexRel, VRelation heapRel, ItemPointer ht_ctid, char *datum, unsigned int datumSize)
{
	bool		result;
	IndexTuple	itup;

	Datum		index_values[1];
	bool		index_isnull[1];

	index_values[0] = PointerGetDatum_s(datum);
	index_isnull[0] = false;

	/* enable  */
	/* bool checkUnique = UNIQUE_CHECK_NO; //enable duplicate? */
	/* generate an index tuple */
	itup = index_form_tuple_s(indexRel->tDesc, index_values, index_isnull);
	itup->t_tid = *ht_ctid;

	result = _bt_doinsert_s(indexRel, itup, datum, datumSize, heapRel);

	free(itup);

	return result;
}

/*
 *	btgettuple() -- Get the next tuple in the scan.
 */
bool
btgettuple_s(IndexScanDesc scan)
{
	BTScanOpaque so = (BTScanOpaque) scan->opaque;
	bool		res;

    res = _bt_first_s(scan); 
    ReleaseBuffer_s(scan->indexRelation, so->currPos.buf);
    so->currPos.buf = InvalidBuffer;
    return res;

}

/*
 *	btbeginscan() -- start a scan on a btree index
 */
IndexScanDesc
btbeginscan_s(VRelation rel, const char *key, int keysize)
{
	IndexScanDesc scan;
	BTScanOpaque so;
	ScanKey		scanKey;

	scanKey = (ScanKey) malloc(sizeof(ScanKeyData));
	scanKey->sk_subtype = rel->foid;
	scanKey->sk_argument = (char *) malloc(keysize);
	memcpy(scanKey->sk_argument, key, keysize);
	scanKey->datumSize = keysize;

	/* allocate private workspace */
	so = (BTScanOpaque) malloc(sizeof(BTScanOpaqueData));
	BTScanPosInvalidate_s(so->currPos);
	BTScanPosInvalidate_s(so->markPos);

	/*
	 * so->keyData = malloc(sizeof(ScanKeyData)); so->arrayKeyData = NULL;
	 * so->numArrayKeys = 0;
	 */

	/*
	 * We don't know yet whether the scan will be index-only, so we do not
	 * allocate the tuple workspace arrays until btrescan.  However, we set up
	 * scan->xs_itupdesc whether we'll need it or not, since that's so cheap.
	 */
    so->currTuples = so->markTuples = NULL;
    so->currPos.firstItem = 0;
    so->currPos.lastItem = 0;

	/* get the scan */
	scan = (IndexScanDesc) malloc(sizeof(IndexScanDescData));
	scan->indexRelation = rel;
    scan->ost = NULL;
	scan->keyData = scanKey;
	scan->opaque = so;

	ItemPointerSetInvalid_s(&scan->xs_ctup.t_self);
	scan->xs_ctup.t_data = NULL;
	scan->xs_cbuf = InvalidBuffer;
	scan->xs_continue_hot = false;

	return scan;
}


/*
 *	btendscan() -- close down a scan
 */
void
btendscan_s(IndexScanDesc scan)
{
	BTScanOpaque so = (BTScanOpaque) scan->opaque;

	so->markItemIndex = -1;

	/* No need to invalidate positions, the RAM is about to be freed. */

	/* Release storage */
	/* if (so->keyData != NULL) */
	free(scan->keyData->sk_argument);
	free(scan->keyData);
	/* so->markTuples should not be pfree'd, see btrescan */
	free(so);
	free(scan);
}
