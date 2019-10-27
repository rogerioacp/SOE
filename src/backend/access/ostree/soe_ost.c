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
bool insert_ost(OSTRelation relstate, char* block, int level, int offset)
{


	if(level == 0){
		//selog(DEBUG1, "Going to write root on file with level %d", level);
		setclevel(level);
		setclevelo(level);
		PLBlock pblock;
		pblock = createBlock(offset, BLCKSZ, block);
		ost_fileWrite(pblock, relstate->osts->iname, offset);
		free(pblock->block);
		free(pblock);
		//selog(DEBUG1, "Root has been written");
	}else{
		//selog(DEBUG1, "Going to write inner node on file with level %d", level);
		setclevel(level);
		setclevelo(level);
		//selog(DEBUG1, "Writing on level %d block %d", level, offset);
		write_oram(block, BLCKSZ, offset, relstate->osts->orams[level-1]);
	}

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


	/*
	 * If we have any array keys, initialize them during first call for a
	 * scan.  We can't do this in btrescan because we don't know the scan
	 * direction at that time.
	 */
	//if (so->numArrayKeys && !BTScanPosIsValid_s(so->currPos))
	//{
		/* punt if we have any unsatisfiable array keys */
	//	if (so->numArrayKeys < 0)
	//		return false;
//
//		_bt_start_array_keys(scan, dir);
//	}

	/* This loop handles advancing to the next array elements, if any */
	//do
	//{
		/*
		 * If we've already initialized this scan, we can just advance it in
		 * the appropriate direction.  If we haven't done so yet, we call
		 * _bt_first() to get the first item in the scan.
		 */
		if (!BTScanPosIsValid_OST(so->currPos)){
			selog(DEBUG1, "Going to first scan");
			res = _bt_first_ost(scan);
		}
		else
		{
			selog(DEBUG1, "Going to continue for next");
			/*
			 * Now continue the scan.
			 */
			res = _bt_next_ost(scan);
		}

		/* If we have a tuple, return it ... */
		//if (res)
		//	break;
		/* ... otherwise see if we have more array keys to deal with */
	//} while (so->numArrayKeys && _bt_advance_array_keys(scan, dir));

	return res;
}

/*
 *	btbeginscan() -- start a scan on a btree index
 */
IndexScanDesc
btbeginscan_ost(OSTRelation rel, const char* key, int keysize)
{
	IndexScanDesc scan;
	BTScanOpaqueOST so;
	ScanKey scanKey;

	scanKey = (ScanKey) malloc(sizeof(ScanKeyData));
	//scanKey->sk_subtype = rel->foid;
	scanKey->sk_argument = (char*) malloc(keysize);
	memcpy(scanKey->sk_argument, key, keysize);
	scanKey->datumSize = keysize;

	/* allocate private workspace */
	so = (BTScanOpaqueOST) malloc(sizeof(BTScanOpaqueDataOST));
	BTScanPosInvalidate_OST(so->currPos);
	BTScanPosInvalidate_OST(so->markPos);
	/*so->keyData = malloc(sizeof(ScanKeyData));
	so->arrayKeyData = NULL;
	so->numArrayKeys = 0;*/
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
	//if (so->keyData != NULL)
	free(scan->keyData->sk_argument);
	free(scan->keyData);
	/* so->markTuples should not be pfree'd, see btrescan */
	free(so);
	free(scan);
}

