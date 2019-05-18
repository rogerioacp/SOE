/*-------------------------------------------------------------------------
 *
 * hash.c
 *	  Implementation of Margo Seltzer's Hashing package for postgres.
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/hash/hash.c
 *
 * NOTES
 *	  This file contains only the public interface routines.
 *
 *-------------------------------------------------------------------------
 */

#include "storage/soe_bufpage.h"
#include "access/soe_hash.h"
#include "access/soe_itup.h"
#include "access/soe_genam.h"

/*
 *	hashinsert() -- insert an index tuple into a hash table.
 *
 *	Hash on the heap tuple's key, form an index tuple with hash code.
 *	Find the appropriate location for the new tuple, and put it there.
 */
bool
hashinsert_s(VRelation rel, IndexTuple itup)
{

	/*
	 *The logic to create an index tuple will be made outside of the enclave on
	 * the foreign data wrapper. However, it will have an ecall to generate
	 * an encrypted hash key which can be later decrypted and used by the 
	 * enclave. This process simplifies the process of creating an index 
	 * tuple.
     */

	//Datum		index_values[1];
	//bool		index_isnull[1];
	//IndexTuple	itup;

	/* convert data to a hash key; on failure, do not insert anything */
	// Is going to be hardocode for prototype. hasutil.c
	//if (!_hash_convert_tuple(rel,
	//						 values, isnull,
	//						 index_values, index_isnull))
	//	return false;

	/* form an index tuple and point it at the heap tuple */
	/**
	 * TODO: index_form_tuple can be done outside which does not impact the
	 * system security and does not need to import the index_form_tuple 
	 * function to inside of the enclave.
	 */
	//itup = index_form_tuple(RelationGetDescr(rel), index_values, index_isnull);
	//itup->t_tid = *ht_ctid;

	_hash_doinsert_s(rel, itup);
	return false;
}


/*
 *	hashgettuple() -- Get the next tuple in the scan.
 */
bool
hashgettuple_s(IndexScanDesc scan)
{
	HashScanOpaque so = (HashScanOpaque) scan->opaque;
	bool		res;

	/* Hash indexes are always lossy since we store only the hash code */
	//scan->xs_recheck = true;

	/*
	 * If we've already initialized this scan, we can just advance it in the
	 * appropriate direction.  If we haven't done so yet, we call a routine to
	 * get the first item in the scan.
	 */
	if (!HashScanPosIsValid_s(so->currPos))
		res = _hash_first_s(scan);
	else
	{
		/*
		 * Now continue the scan.
		 */
		res = _hash_next_s(scan);
	}

	return res;
}


/*
 *	hashbeginscan() -- start a scan on a hash index
 */
IndexScanDesc
hashbeginscan_s(VRelation rel, int nkeys, int norderbys)
{
	IndexScanDesc scan;
	HashScanOpaque so;

	scan = NULL;

	/* no order by operators allowed */
	//Assert(norderbys == 0);

	/*TODO: RelationGetIndexScan has be placed inside the enclave*/
	//scan = RelationGetIndexScan(rel, nkeys, norderbys);

	/*TODO: replace palloc calls for mallocs*/
	so = (HashScanOpaque) malloc(sizeof(HashScanOpaqueData));
	HashScanPosInvalidate_s(so->currPos);
	so->hashso_bucket_buf = InvalidBuffer;
	so->hashso_split_bucket_buf = InvalidBuffer;

	so->hashso_buc_populated = false;
	so->hashso_buc_split = false;

	so->killedItems = NULL;
	so->numKilled = 0;

	scan->opaque = so;

	return scan;
}

/*
 *	hashendscan() -- close down a scan
 */
void
hashendscan_s(IndexScanDesc scan)
{
	HashScanOpaque so = (HashScanOpaque) scan->opaque;
	VRelation	rel = scan->indexRelation;

	_hash_dropscanbuf_s(rel, so);

	if (so->killedItems != NULL)
		free(so->killedItems);
	free(so);
	scan->opaque = NULL;
}


/*
 * On the prototype this functions used for cleaning an old page after
 * a split point.
 * 
 * Helper function to perform deletion of index entries from a bucket.
 *
 * This function expects that the caller has acquired a cleanup lock on the
 * primary bucket page, and will return with a write lock again held on the
 * primary bucket page.  The lock won't necessarily be held continuously,
 * though, because we'll release it when visiting overflow pages.
 *
 * There can't be any concurrent scans in progress when we first enter this
 * function because of the cleanup lock we hold on the primary bucket page,
 * but as soon as we release that lock, there might be.  If those scans got
 * ahead of our cleanup scan, they might see a tuple before we kill it and
 * wake up only after VACUUM has completed and the TID has been recycled for
 * an unrelated tuple.  To avoid that calamity, we prevent scans from passing
 * our cleanup scan by locking the next page in the bucket chain before
 * releasing the lock on the previous page.  (This type of lock chaining is not
 * ideal, so we might want to look for a better solution at some point.)
 *
 * We need to retain a pin on the primary bucket to ensure that no concurrent
 * split can start.
 */
void
hashbucketcleanup_s(VRelation rel, Bucket cur_bucket, Buffer bucket_buf,
				  BlockNumber bucket_blkno,
				  uint32 maxbucket, uint32 highmask, uint32 lowmask)
{
	BlockNumber blkno;
	Buffer		buf;
	bool		bucket_dirty = false;

	blkno = bucket_blkno;
	buf = bucket_buf;

	/* Scan each page in bucket */
	for (;;)
	{
		HashPageOpaque opaque;
		OffsetNumber offno;
		OffsetNumber maxoffno;
		Buffer		next_buf;
		Page		page;
		OffsetNumber deletable[MaxOffsetNumber];
		int			ndeletable = 0;
		//bool		retain_pin = false;
		// bool		clear_dead_marking = false;

		page = BufferGetPage_s(rel, buf);
		opaque = (HashPageOpaque) PageGetSpecialPointer_s(page);

		/* Scan each tuple in page */
		maxoffno = PageGetMaxOffsetNumber_s(page);
		for (offno = FirstOffsetNumber;
			 offno <= maxoffno;
			 offno = OffsetNumberNext_s(offno))
		{
//			ItemPointer htup;
			IndexTuple	itup;
			Bucket		bucket;
			bool		kill_tuple = false;

			itup = (IndexTuple) PageGetItem_s(page,
											PageGetItemId_s(page, offno));
			//htup = &(itup->t_tid);
			
			/* delete the tuples that are moved by split. */
			bucket = _hash_hashkey2bucket_s(_hash_get_indextuple_hashkey_s(itup),
										  maxbucket,
										  highmask,
										  lowmask);
			/* mark the item for deletion */
			if (bucket != cur_bucket)
			{
				/*
				 * We expect tuples to either belong to current bucket or
				 * new_bucket.  This is ensured because we don't allow
				 * further splits from bucket that contains garbage. See
				 * comments in _hash_expandtable.
				 */
				kill_tuple = true;
			}
			

			if (kill_tuple)
			{
				/* mark the item for deletion */
				deletable[ndeletable++] = offno;
			}
		}

		/* retain the pin on primary bucket page till end of bucket scan */
		/*if (blkno == bucket_blkno)
			retain_pin = true;
		else
			retain_pin = false;*/

		blkno = opaque->hasho_nextblkno;

		/*
		 * Apply deletions, advance to next page and write page if needed.
		 */
		if (ndeletable > 0)
		{
			/* No ereport(ERROR) until changes are logged */

			PageIndexMultiDelete_s(page, deletable, ndeletable);
			bucket_dirty = true;

			MarkBufferDirty_s(rel, buf);

		
		}

		/* bail out if there are no more pages to scan. */
		if (!BlockNumberIsValid_s(blkno))
			break;

		next_buf = _hash_getbuf_with_strategy_s(rel, blkno, LH_OVERFLOW_PAGE);

		/*
		 * release the lock on previous page after acquiring the lock on next
		 * page
		 */
		//if (retain_pin)
		//	LockBuffer(buf, BUFFER_LOCK_UNLOCK);
		//else
		//	_hash_relbuf(rel, buf);

		buf = next_buf;
	}

	/*
	 * lock the bucket page to clear the garbage flag and squeeze the bucket.
	 * if the current buffer is same as bucket buffer, then we already have
	 * lock on bucket page.
	 */
	if (buf != bucket_buf)
	{
		_hash_relbuf_s(rel, buf);
		//LockBuffer(bucket_buf, BUFFER_LOCK_EXCLUSIVE);
	}

	/*
	 * Clear the garbage flag from bucket after deleting the tuples that are
	 * moved by split.  We purposefully clear the flag before squeeze bucket,
	 * so that after restart, vacuum shouldn't again try to delete the moved
	 * by split tuples.
	 */
	//if (split_cleanup)
	/*This function is only used for split_cleanup*/
	
	HashPageOpaque bucket_opaque;
	Page		page;

	page = BufferGetPage_s(rel, bucket_buf);
	bucket_opaque = (HashPageOpaque) PageGetSpecialPointer_s(page);

	/* No ereport(ERROR) until changes are logged */

	bucket_opaque->hasho_flag &= ~LH_BUCKET_NEEDS_SPLIT_CLEANUP;
	MarkBufferDirty_s(rel, bucket_buf);

	

	/*
	 * If we have deleted anything, try to compact free space.  For squeezing
	 * the bucket, we must have a cleanup lock, else it can impact the
	 * ordering of tuples for a scan that has started before it.
	 */
	//if (bucket_dirty && IsBufferCleanupOK(bucket_buf))
	//Assuming that its always ok to cleanup.  
	// TODO:Check if buffer locks used by soe always enable cleanup in this point.
	if (bucket_dirty)
		_hash_squeezebucket_s(rel, cur_bucket, bucket_blkno, bucket_buf);
	//else
	//	LockBuffer(bucket_buf, BUFFER_LOCK_UNLOCK);
}