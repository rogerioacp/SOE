/*-------------------------------------------------------------------------
 *
 * hashinsert.c
 * Barebones copy of Item insertion in hash tables for Postgres for enclave.
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/hash/hashinsert.c
 *
 *-------------------------------------------------------------------------
 */


#include "storage/soe_bufpage.h"
#include "access/soe_hash.h"
#include "access/soe_itup.h"
#include "logger/logger.h"
/*
 *	_hash_doinsert() -- Handle insertion of a single index tuple.
 *
 *		This routine is called by the public interface routines, hashbuild
 *		and hashinsert.  By here, itup is completely filled in.
 */
void
_hash_doinsert_s(VRelation rel, IndexTuple itup)
{
	Buffer		buf = InvalidBuffer;
	Buffer		bucket_buf;
	Buffer		metabuf;
	HashMetaPage metap;
	HashMetaPage usedmetap = NULL;
	Page		metapage;
	Page		page;
	HashPageOpaque pageopaque;
	Size		itemsz;
	bool		do_expand;
	uint32		hashkey;
	//Bucket		bucket;

	/*
	 * Get the hash key for the item (it's stored in the index tuple itself).
	 */
	hashkey = _hash_get_indextuple_hashkey_s(itup);

	/* compute item size too */
	itemsz = IndexTupleSize_s(itup);
	itemsz = MAXALIGN_s(itemsz);	/* be safe, PageAddItem will do this but we
								 * need to be consistent */

	/*
	 * Read the metapage.  We don't lock it yet; HashMaxItemSize() will
	 * examine pd_pagesize_version, but that can't change so we can examine it
	 * without a lock.
	 */
	metabuf = _hash_getbuf_s(rel, HASH_METAPAGE, HASH_NOLOCK, LH_META_PAGE);
	metapage = BufferGetPage_s(rel, metabuf);

	/*
	 * Check whether the item can fit on a hash page at all. (Eventually, we
	 * ought to try to apply TOAST methods if not.)  Note that at this point,
	 * itemsz doesn't include the ItemId.
	 *
	 * XXX this is useless code if we are only storing hash keys.
	 */
	if (itemsz > HashMaxItemSize_s(metapage))
		//log error
		/*ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("index row size %zu exceeds hash maximum %zu",
						itemsz, HashMaxItemSize(metapage)),
				 errhint("Values larger than a buffer page cannot be indexed.")));*/

	/* Lock the primary bucket page for the target bucket. */
	buf = _hash_getbucketbuf_from_hashkey_s(rel, hashkey, HASH_WRITE,
										  &usedmetap);
	//Assert(usedmetap != NULL);


	/* remember the primary bucket buffer to release the pin on it at end. */
	bucket_buf = buf;

	page = BufferGetPage_s(rel, buf);
	pageopaque = (HashPageOpaque) PageGetSpecialPointer_s(page);
	//bucket = pageopaque->hasho_bucket;

	/* Do the insertion */
	while (PageGetFreeSpace_s(page) < itemsz)
	{
		BlockNumber nextblkno;


		/*
		 * no space on this page; check for an overflow page
		 */
		nextblkno = pageopaque->hasho_nextblkno;

		if (BlockNumberIsValid_s(nextblkno))
		{

			buf = _hash_getbuf_s(rel, nextblkno, HASH_WRITE, LH_OVERFLOW_PAGE);
			page = BufferGetPage_s(rel, buf);
		}
		else
		{
			/*
			 * we're at the end of the bucket chain and we haven't found a
			 * page with enough room.  allocate a new overflow page.
			 */

			/* chain to a new overflow page */
			buf = _hash_addovflpage_s(rel, metabuf, buf, (buf == bucket_buf) ? true : false);
			page = BufferGetPage_s(rel, buf);

			/* should fit now, given test above */
			//Assert(PageGetFreeSpace(page) >= itemsz);
		}
		pageopaque = (HashPageOpaque) PageGetSpecialPointer_s(page);
		//Assert((pageopaque->hasho_flag & LH_PAGE_TYPE) == LH_OVERFLOW_PAGE);
		//Assert(pageopaque->hasho_bucket == bucket);
	}

	/*
	 * Write-lock the metapage so we can increment the tuple count. After
	 * incrementing it, check to see if it's time for a split.
	 */

	/* found page with enough space, so add the item here */
	//itup_off = _hash_pgaddtup(rel, buf, itemsz, itup);

	MarkBufferDirty_s(rel, buf);

	/* metapage operations */
	metap = HashPageGetMeta_s(metapage);
	metap->hashm_ntuples += 1;

	/* Make sure this stays in sync with _hash_expandtable() */
	do_expand = metap->hashm_ntuples >
		(double) metap->hashm_ffactor * (metap->hashm_maxbucket + 1);

	MarkBufferDirty_s(rel, metabuf);


	/* Attempt to split if a split is needed */
	if (do_expand)
		_hash_expandtable_s(rel, metabuf);

}

/*
 *	_hash_pgaddtup() -- add a tuple to a particular page in the index.
 *
 * This routine adds the tuple to the page as requested; it does not write out
 * the page.  It is an error to call pgaddtup() without pin and write lock on
 * the target buffer.
 *
 * Returns the offset number at which the tuple was inserted.  This function
 * is responsible for preserving the condition that tuples in a hash index
 * page are sorted by hashkey value.
 */
OffsetNumber
_hash_pgaddtup_s(VRelation rel, Buffer buf, Size itemsize, IndexTuple itup)
{
	OffsetNumber itup_off;
	Page		page;
	uint32		hashkey;

	_hash_checkpage_s(rel, buf, LH_BUCKET_PAGE | LH_OVERFLOW_PAGE);
	page = BufferGetPage_s(rel, buf);

	/* Find where to insert the tuple (preserving page's hashkey ordering) */
	hashkey = _hash_get_indextuple_hashkey_s(itup);
	itup_off = _hash_binsearch_s(page, hashkey);
	//Page add item extended. already have an example of a function to add a page.
	PageAddItem_s(page, (Item) itup, itemsize, itup_off, false, false);
	//if (PageAddItem(page, (Item) itup, itemsize, itup_off, false, false)
	//	== InvalidOffsetNumber)
		//log error
		/*elog(ERROR, "failed to add index item to \"%s\"",
			 RelationGetRelationName(rel));*/

	return itup_off;
}

/*
 *	_hash_pgaddmultitup() -- add a tuple vector to a particular page in the
 *							 index.
 *
 * This routine has same requirements for locking and tuple ordering as
 * _hash_pgaddtup().
 *
 * Returns the offset number array at which the tuples were inserted.
 */
void
_hash_pgaddmultitup_s(VRelation rel, Buffer buf, IndexTuple *itups,
					OffsetNumber *itup_offsets, uint16 nitups)
{
	OffsetNumber itup_off;
	Page		page;
	uint32		hashkey;
	int			i;

	_hash_checkpage_s(rel, buf, LH_BUCKET_PAGE | LH_OVERFLOW_PAGE);
	page = BufferGetPage_s(rel, buf);

	for (i = 0; i < nitups; i++)
	{
		Size		itemsize;

		itemsize = IndexTupleSize_s(itups[i]);
		itemsize = MAXALIGN_s(itemsize);

		/* Find where to insert the tuple (preserving page's hashkey ordering) */
		hashkey = _hash_get_indextuple_hashkey_s(itups[i]);
		itup_off = _hash_binsearch_s(page, hashkey);

		itup_offsets[i] = itup_off;

		if (PageAddItem_s(page, (Item) itups[i], itemsize, itup_off, false, false)
			== InvalidOffsetNumber)
			selog(ERROR,"failed to add index item to relation");
				// RelationGetRelationName(rel));
			//Log error
			/*elog(ERROR, "failed to add index item to \"%s\"",
				 RelationGetRelationName(rel));*/
	}
}

