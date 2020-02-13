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
/*
#include "storage/soe_bufpage.h"
#include "access/soe_nbtree.h"
#include "access/soe_itup.h"
#include "access/soe_genam.h"
#include "access/soe_itup.h"
#include "logger/logger.h"
*/
#include <oram/plblock.h>
#include <string.h>
#include <stdlib.h>


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
    Buffer buffer;
    Page    page;


    indexRel->level = level;
   
    buffer = _bt_getbuf_level_s(indexRel, offset); 

    page = BufferGetPage_s(indexRel, buffer);

    memcpy(page, block, BLCKSZ);

    MarkBufferDirty_s(indexRel, buffer);
    ReleaseBuffer_s(indexRel, buffer);
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


    if (!BTScanPosIsValid_s(so->currPos))
    {
        res = _bt_first_s(scan); 
        selog(DEBUG1, "Result of first iteration %d", res);
        ReleaseBuffer_s(scan->indexRelation, so->currPos.buf);
        so->currPos.buf = InvalidBuffer;

    }
    else
    {
		/*
		 * Now continue the scan.
		 */
		res = _bt_next_s(scan);
        selog(DEBUG1, "Next result is %d", res);
    }
    
    // the result returned in res signals if any match was found
    
#ifdef DUMMYS 
    /*I'm almost sure that when one condition is true so is the other. Validate
     * this assumption. No more results*/
    selog(DEBUG1, "what? %d %d %d", res, so->currPos.nextPage, so->currPos.moreRight);
    //if(res == false && (so->currPos.nextPage == P_NONE || !so->currPos.moreRight))
    if(res == false && so->currPos.nextPage == InvalidBlockNumber)
    {
        return false;
    }

    return true;
#else
    return res;
#endif

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
