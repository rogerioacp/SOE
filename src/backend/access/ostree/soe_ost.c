/*-------------------------------------------------------------------------
 *
 * soe_ost.c
 * Bare bones copy of the Implementation of Lehman and Yao's btree management
 * algorithm for Postgres. This implementation is developed to be executed
 * inside a secure enclave with the ost protocol
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

#include "access/soe_ost.h"
#include "logger/logger.h"
#include "storage/soe_ost_ofile.h"
#include "storage/soe_bufpage.h"
#include "storage/soe_ost_bufmgr.h"

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



/*
 *	btinsert() -- insert an index tuple into a btree.
 *
 *		Descend the tree recursively, find the appropriate location for our
 *		new tuple, and put it there.
 */
bool
insert_ost(OSTRelation rel, char *block, unsigned int level, unsigned int offset)
{
	Buffer		buffer;
	Page		page;

	rel->level = level;

	buffer = ReadBuffer_ost(rel, offset);
	page = BufferGetPage_ost(rel, buffer);

	memcpy(page, block, BLCKSZ);

	MarkBufferDirty_ost(rel, buffer);
	ReleaseBuffer_ost(rel, buffer);

	return true;
}

/*
 *	btgettuple() -- Get the next tuple in the scan.
 */
bool
btgettuple_ost(IndexScanDesc scan)
{
	BTScanOpaqueOST so = (BTScanOpaqueOST) scan->opaque;
	bool		res;
    bool        first = false;
	if (!BTScanPosIsValid_OST(so->currPos))
	{
       	res = _bt_first_ost(scan);
        first  = true;
	}
	else
	{
       	/*
		 * Now continue the scan.
		 */
		res = _bt_next_ost(scan);
	}
#ifdef DUMMYS 
    if(res == false && (so->currPos.nextPage == P_NONE || !so->currPos.moreRight))
    {
        //selog(DEBUG1, "No results for now, but next page might have");
        return false;
    }
    if(first){
        selog(DEBUG1, "Not found, first and no more.");
        BTScanOpaqueOST so = (BTScanOpaqueOST) scan->opaque;
        ReleaseBuffer_ost(scan->ost, so->currPos.buf);
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
btbeginscan_ost(OSTRelation rel, const char *key, int keysize)
{
	IndexScanDesc scan;
	BTScanOpaqueOST so;
	ScanKey		scanKey;

	scanKey = (ScanKey) malloc(sizeof(ScanKeyData));
	/* scanKey->sk_subtype = rel->foid; */
	scanKey->sk_argument = (char *) malloc(keysize);
	memcpy(scanKey->sk_argument, key, keysize);
	scanKey->datumSize = keysize;

	/* allocate private workspace */
	so = (BTScanOpaqueOST) malloc(sizeof(BTScanOpaqueDataOST));
	BTScanPosInvalidate_OST(so->currPos);
	BTScanPosInvalidate_OST(so->markPos);

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

	/* get the scan */
	scan = (IndexScanDesc) malloc(sizeof(IndexScanDescData));
	scan->ost = rel;
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
btendscan_ost(IndexScanDesc scan)
{
	BTScanOpaqueOST so = (BTScanOpaqueOST) scan->opaque;

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
