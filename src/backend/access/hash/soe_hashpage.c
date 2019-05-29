/*-------------------------------------------------------------------------
 *
 * hashpage.c
 *	  Hash table page management code for the Postgres hash access method
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/hash/hashpage.c
 *
 * NOTES
 *	  Postgres hash pages look like ordinary relation pages.  The opaque
 *	  data at high addresses includes information about the page including
 *	  whether a page is an overflow page or a true bucket, the bucket
 *	  number, and the block numbers of the preceding and following pages
 *	  in the same bucket.
 *
 *	  The first page in a hash relation, page zero, is special -- it stores
 *	  information describing the hash table; it is referred to as the
 *	  "meta page." Pages one and higher store the actual data.
 *
 *	  There are also bitmap pages, which are not manipulated here;
 *	  see hashovfl.c.
 *
 *-------------------------------------------------------------------------
 */

#include "access/soe_hash.h"
#include "storage/soe_bufmgr.h"
#include "logger/logger.h"
#include "access/soe_hash.h"


static bool _hash_alloc_buckets_s(VRelation rel, BlockNumber firstblock,
					uint32 nblocks);
static void _hash_splitbucket_s(VRelation rel, Buffer metabuf,
				  Bucket obucket, Bucket nbucket,
				  Buffer obuf,
				  Buffer nbuf,
				  uint32 maxbucket,
				  uint32 highmask, uint32 lowmask);


/*
 * We use high-concurrency locking on hash indexes (see README for an overview
 * of the locking rules).  However, we can skip taking lmgr locks when the
 * index is local to the current backend (ie, either temp or new in the
 * current transaction).  No one else can see it, so there's no reason to
 * take locks.  We still take buffer-level locks, but not lmgr locks.
 */
#define USELOCKING_s(rel)		(!RELATION_IS_LOCAL(rel))




/*
 *	_hash_init() -- Initialize the metadata page of a hash index,
 *				the initial buckets, and the initial bitmap page.
 *
 * The initial number of buckets is dependent on num_tuples, an estimate
 * of the number of tuples to be loaded into the index initially.  The
 * chosen number of buckets is returned.
 *
 * We are fairly cavalier about locking here, since we know that no one else
 * could be accessing this index.  In particular the rule about not holding
 * multiple buffer locks is ignored.
 */
uint32
_hash_init_s(VRelation rel, double num_tuples) 
{
	Buffer		metabuf;
	Buffer		buf;
	Buffer		bitmapbuf;
	Page		pg;
	HashMetaPage metap;
	int32		data_width;
	int32		item_width;
	int32		ffactor;
	uint32		num_buckets;
	uint32		i;



	/*
	 * Determine the target fill factor (in tuples per bucket) for this index.
	 * The idea is to make the fill factor correspond to pages about as full
	 * as the user-settable fillfactor parameter says.  We can compute it
	 * exactly since the index datatype (i.e. uint32 hash key) is fixed-width.
	 */
	data_width = sizeof(uint32);
	item_width = MAXALIGN_s(sizeof(IndexTupleData)) + MAXALIGN_s(data_width) +
		sizeof(ItemIdData);		/* include the line pointer */
	ffactor =  ((BLCKSZ*HASH_DEFAULT_FILLFACTOR)/100) / item_width;
	//selog(DEBUG1, "Fill factor is %d", ffactor);

	/* keep to a sane range */
	if (ffactor < 10)
		ffactor = 10;
	//Not used, even in the   original postgres code
	//procid = index_getprocid(rel, 1, HASHSTANDARD_PROC);

	/*
	 * We initialize the metapage, the first N bucket pages, and the first
	 * bitmap page in sequence, using _hash_getnewbuf to cause smgrextend()
	 * calls to occur.  This ensures that the smgr level has the right idea of
	 * the physical index length.
	 *
	 * Critical section not required, because on error the creation of the
	 * whole relation will be rolled back.
	 */
	metabuf = _hash_getnewbuf_s(rel, HASH_METAPAGE);
	_hash_init_metabuffer_s(rel, metabuf, num_tuples, ffactor);
	MarkBufferDirty_s(rel, metabuf);

	pg = BufferGetPage_s(rel, metabuf);
	metap = HashPageGetMeta_s(pg);


	num_buckets = metap->hashm_maxbucket + 1;

	/*
	 * Initialize and WAL Log the first N buckets
	 */
	for (i = 0; i < num_buckets; i++)
	{
		BlockNumber blkno;

		/* Allow interrupts, in case N is huge */

		blkno = BUCKET_TO_BLKNO_s(metap, i);
		//selog(DEBUG1, "Going to initialize block %d", blkno);
		buf = _hash_getnewbuf_s(rel, blkno);
		_hash_initbuf_s(rel, buf, metap->hashm_maxbucket, i, LH_BUCKET_PAGE, false);

		MarkBufferDirty_s(rel, buf);
		ReleaseBuffer_s(rel, buf);
	}


	/*
	 * Initialize bitmap page
	 */
	//selog(DEBUG1, "Going to get bitmap page %d", num_buckets+1);
	bitmapbuf = _hash_getnewbuf_s(rel, num_buckets + 1);
	_hash_initbitmapbuffer_s(rel, bitmapbuf, metap->hashm_bmsize, false);
	MarkBufferDirty_s(rel, bitmapbuf);

	/* add the new bitmap page to the metapage's list of bitmaps */
	/* metapage already has a write lock */
	if (metap->hashm_nmaps >= HASH_MAX_BITMAPS)
		selog(DEBUG1, "out of overflow pages in hash index");

	metap->hashm_mapp[metap->hashm_nmaps] = num_buckets + 1;

	metap->hashm_nmaps++;

	//selog(DEBUG1, "Going to update metabuffer");
	MarkBufferDirty_s(rel, metabuf);


	/* all done */
	ReleaseBuffer_s(rel, bitmapbuf);
	ReleaseBuffer_s(rel, metabuf);

	return num_buckets;
}


/*
 *	_hash_init_metabuffer() -- Initialize the metadata page of a hash index.
 */
void
_hash_init_metabuffer_s(VRelation rel, Buffer buf, double num_tuples,
					  uint16 ffactor)
{
	HashMetaPage metap;
	HashPageOpaque pageopaque;
	Page		page;
	double		dnumbuckets;
	uint32		num_buckets;
	uint32		spare_index;
	uint32		i;

	/*
	 * Choose the number of initial bucket pages to match the fill factor
	 * given the estimated number of tuples.  We round up the result to the
	 * total number of buckets which has to be allocated before using its
	 * _hashm_spare element. However always force at least 2 bucket pages. The
	 * upper limit is determined by considerations explained in
	 * _hash_expandtable().
	 */
	dnumbuckets = num_tuples / ffactor;
	if (dnumbuckets <= 2.0)
		num_buckets = 2;
	else if (dnumbuckets >= (double) 0x40000000)
		num_buckets = 0x40000000;
	else
		num_buckets = _hash_get_totalbuckets_s(_hash_spareindex_s(dnumbuckets));

	spare_index = _hash_spareindex_s(num_buckets);

	page = BufferGetPage_s(rel, buf);

	//_hash_pageinit_s(page, BufferGetPageSize_s(rel, buf));

	pageopaque = (HashPageOpaque) PageGetSpecialPointer_s(page);
	pageopaque->hasho_prevblkno = InvalidBlockNumber;
	pageopaque->hasho_nextblkno = InvalidBlockNumber;
	pageopaque->hasho_bucket = -1;
	pageopaque->hasho_flag = LH_META_PAGE;
	pageopaque->hasho_page_id = HASHO_PAGE_ID;

	metap = HashPageGetMeta_s(page);

	metap->hashm_magic = HASH_MAGIC;
	metap->hashm_version = HASH_VERSION;
	metap->hashm_ntuples = 0;
	metap->hashm_nmaps = 0;
	metap->hashm_ffactor = ffactor;
	metap->hashm_bsize = HashGetMaxBitmapSize_s(page);
	/* find largest bitmap array size that will fit in page size */
	for (i = _hash_log2_s(metap->hashm_bsize); i > 0; --i)
	{
		if ((1 << i) <= metap->hashm_bsize)
			break;
	}

	metap->hashm_bmsize = 1 << i;
	metap->hashm_bmshift = i + BYTE_TO_BIT;

	/*
	 * We initialize the index with N buckets, 0 .. N-1, occupying physical
	 * blocks 1 to N.  The first freespace bitmap page is in block N+1.
	 */
	metap->hashm_maxbucket = num_buckets - 1;

	/*
	 * Set highmask as next immediate ((2 ^ x) - 1), which should be
	 * sufficient to cover num_buckets.
	 */
	metap->hashm_highmask = (1 << (_hash_log2_s(num_buckets + 1))) - 1;
	metap->hashm_lowmask = (metap->hashm_highmask >> 1);

	MemSet_s(metap->hashm_spares, 0, sizeof(metap->hashm_spares));
	MemSet_s(metap->hashm_mapp, 0, sizeof(metap->hashm_mapp));

	/* Set up mapping for one spare page after the initial splitpoints */
	metap->hashm_spares[spare_index] = 1;
	metap->hashm_ovflpoint = spare_index;
	metap->hashm_firstfree = 0;

	/*
	 * Set pd_lower just past the end of the metadata.  This is essential,
	 * because without doing so, metadata will be lost if xlog.c compresses
	 * the page.
	 */
	((PageHeader) page)->pd_lower =
		((char *) metap + sizeof(HashMetaPageData)) - (char *) page;
}


/*
 * _hash_getbuf_with_condlock_cleanup() -- Try to get a buffer for cleanup.
 *
 *		We read the page and try to acquire a cleanup lock.  If we get it,
 *		we return the buffer; otherwise, we return InvalidBuffer.
 */
Buffer
_hash_getbuf_with_condlock_cleanup_s(VRelation rel, BlockNumber blkno, int flags)
{
	Buffer		buf = 0;

	if (blkno == P_NEW)
		selog(ERROR, "hash AM does not use P_NEW");
		//log error
		//elog(ERROR, "hash AM does not use P_NEW");

	buf = ReadBuffer_s(rel, blkno);

	/* ref count and lock type are correct */

	_hash_checkpage_s(rel, buf, flags);

	return buf;
}



/*
 *	_hash_getbuf() -- Get a buffer by block number for read or write.
 *
 *		'access' must be HASH_READ, HASH_WRITE, or HASH_NOLOCK.
 *		'flags' is a bitwise OR of the allowed page types.
 *
 *		This must be used only to fetch pages that are expected to be valid
 *		already.  _hash_checkpage() is applied using the given flags.
 *
 *		When this routine returns, the appropriate lock is set on the
 *		requested buffer and its reference count has been incremented
 *		(ie, the buffer is "locked and pinned").
 *
 *		P_NEW is disallowed because this routine can only be used
 *		to access pages that are known to be before the filesystem EOF.
 *		Extending the index should be done with _hash_getnewbuf.
 */
Buffer
_hash_getbuf_s(VRelation rel, BlockNumber blkno, int access, int flags)
{
	Buffer		buf = 0;

	if (blkno == P_NEW)
		selog(ERROR, "hash AM does not use P_NEW");
		//log error
		//elog(ERROR, "hash AM does not use P_NEW");

	buf = ReadBuffer_s(rel, blkno);

	_hash_checkpage_s(rel, buf, flags);

	return buf;
}


/*
 *	_hash_getinitbuf() -- Get and initialize a buffer by block number.
 *
 *		This must be used only to fetch pages that are known to be before
 *		the index's filesystem EOF, but are to be filled from scratch.
 *		_hash_pageinit() is applied automatically.  Otherwise it has
 *		effects similar to _hash_getbuf() with access = HASH_WRITE.
 *
 *		When this routine returns, a write lock is set on the
 *		requested buffer and its reference count has been incremented
 *		(ie, the buffer is "locked and pinned").
 *
 *		P_NEW is disallowed because this routine can only be used
 *		to access pages that are known to be before the filesystem EOF.
 *		Extending the index should be done with _hash_getnewbuf.
 */
Buffer
_hash_getinitbuf_s(VRelation rel, BlockNumber blkno)
{
	Buffer		buf = 0;

	if (blkno == P_NEW)
		selog(ERROR, "hash AM does not use P_NEW");
		//elog(ERROR, "hash AM does not use P_NEW");
	buf =  ReadBuffer_s(rel, blkno);
	//buf = ReadBufferExtended(rel, MAIN_FORKNUM, blkno, RBM_ZERO_AND_LOCK,
	//						 NULL);

	/* ref count and lock type are correct */

	/* initialize the page */
	_hash_pageinit_s(BufferGetPage_s(rel, buf), BufferGetPageSize_s(vrel, buf));

	return buf;
}

/*
 *	_hash_initbuf() -- Get and initialize a buffer by bucket number.
 */
void
_hash_initbuf_s(VRelation rel, Buffer buf, uint32 max_bucket, uint32 num_bucket, uint32 flag,
			  bool initpage)
{
	HashPageOpaque pageopaque;
	Page		page;

	page = BufferGetPage_s(rel, buf);

	/* initialize the page */
	if (initpage)
		_hash_pageinit_s(page, BufferGetPageSize_s(rel, buf));

	pageopaque = (HashPageOpaque) PageGetSpecialPointer_s(page);

	/*
	 * Set hasho_prevblkno with current hashm_maxbucket. This value will be
	 * used to validate cached HashMetaPageData. See
	 * _hash_getbucketbuf_from_hashkey().
	 */
	pageopaque->hasho_prevblkno = max_bucket;
	pageopaque->hasho_nextblkno = InvalidBlockNumber;
	pageopaque->hasho_bucket = num_bucket;
	pageopaque->hasho_flag = flag;
	pageopaque->hasho_page_id = HASHO_PAGE_ID;
}

/*
 *	_hash_getnewbuf() -- Get a new page at the end of the index.
 *
 *		This has the same API as _hash_getinitbuf, except that we are adding
 *		a page to the index, and hence expect the page to be past the
 *		logical EOF.  (However, we have to support the case where it isn't,
 *		since a prior try might have crashed after extending the filesystem
 *		EOF but before updating the metapage to reflect the added page.)
 *
 *		It is caller's responsibility to ensure that only one process can
 *		extend the index at a time.  In practice, this function is called
 *		only while holding write lock on the metapage, because adding a page
 *		is always associated with an update of metapage data.
 */
Buffer
_hash_getnewbuf_s(VRelation rel, BlockNumber blkno)
{
	BlockNumber nblocks = NumberOfBlocks_s(rel);
	Buffer		buf;

	if (blkno == nblocks){
		//selog(DEBUG1, "Requesting for a new block %d", blkno);
		buf = ReadBuffer_s(rel, P_NEW);
	}else{

		//selog(DEBUG1, "Requesting an existing block %d ", blkno);
		buf = ReadBuffer_s(rel, blkno);
	}

	/* ref count and lock type are correct */

	/* initialize the page */
	//Every time a new page is read, the ReadBuffer initializes the page.
	//_hash_pageinit_s(BufferGetPage_s(rel, buf), BufferGetPageSize_s(rel, buf));

	return buf;
}

/*
 *	_hash_getbuf_with_strategy() -- Get a buffer with nondefault strategy.
 *
 *		This is identical to _hash_getbuf() but also allows a buffer access
 *		strategy to be specified.  We use this for VACUUM operations.
 */
Buffer
_hash_getbuf_with_strategy_s(VRelation rel, BlockNumber blkno,  int flags)
{
	Buffer		buf;

	if (blkno == P_NEW)
		selog(ERROR, "hash AM does not use P_NEW");

	buf = ReadBuffer_s(rel, blkno);
//	buf = ReadBufferExtended(rel, MAIN_FORKNUM, blkno, RBM_NORMAL);

	//if (access != HASH_NOLOCK)
	//	LockBuffer(buf, access);

	/* ref count and lock type are correct */

	_hash_checkpage_s(rel, buf, flags);

	return buf;
}


/*
 *	_hash_dropscanbuf() -- release buffers used in scan.
 *
 * This routine unpins the buffers used during scan on which we
 * hold no lock.
 */
void
_hash_dropscanbuf_s(VRelation rel, HashScanOpaque so)
{
	/* release pin we hold on primary bucket page */
	if (BufferIsValid_s(rel, so->hashso_bucket_buf) &&
		so->hashso_bucket_buf != so->currPos.buf)
		ReleaseBuffer_s(rel, so->hashso_bucket_buf);
	so->hashso_bucket_buf = InvalidBuffer;

	/* release pin we hold on primary bucket page  of bucket being split */
	if (BufferIsValid_s(rel, so->hashso_split_bucket_buf) &&
		so->hashso_split_bucket_buf != so->currPos.buf)
		ReleaseBuffer_s(rel, so->hashso_split_bucket_buf);
	so->hashso_split_bucket_buf = InvalidBuffer;

	/* release any pin we still hold */
	if (BufferIsValid_s(rel, so->currPos.buf))
		ReleaseBuffer_s(rel, so->currPos.buf);
	so->currPos.buf = InvalidBuffer;

	/* reset split scan */
	so->hashso_buc_populated = false;
	so->hashso_buc_split = false;
}

/*
 *	_hash_relbuf() -- release a locked buffer.
 *
 * Lock and pin (refcount) are both dropped.
 */
void
_hash_relbuf_s(VRelation rel, Buffer buf)
{
	selog(ERROR, "_hash_relbuf not defined");
	//ReleaseBuffer(rel, buf);
}

/*
 *	_hash_dropbuf() -- release an unlocked buffer.
 *
 * This is used to unpin a buffer on which we hold no lock.
 */
void
_hash_dropbuf_s(VRelation rel, Buffer buf)
{

	selog(ERROR, "_hash_dropbuf not defined");
	//ReleaseBuffer(rel, buf);
}


/*
 *	_hash_pageinit() -- Initialize a new hash index page.
 */
void
_hash_pageinit_s(Page page, Size size)
{
	PageInit_s(page, size, sizeof(HashPageOpaqueData));
}

/*
 * Attempt to expand the hash table by creating one new bucket.
 *
 * This will silently do nothing if we don't get cleanup lock on old or
 * new bucket.
 *
 * Complete the pending splits and remove the tuples from old bucket,
 * if there are any left over from the previous split.
 *
 * The caller must hold a pin, but no lock, on the metapage buffer.
 * The buffer is returned in the same state.
 */
void
_hash_expandtable_s(VRelation rel, Buffer metabuf)
{
	HashMetaPage metap;
	Bucket		old_bucket;
	Bucket		new_bucket;
	uint32		spare_ndx;
	BlockNumber start_oblkno;
	BlockNumber start_nblkno;
	Buffer		buf_nblkno;
	Buffer		buf_oblkno;
	Page		opage;
	Page		npage;
	HashPageOpaque oopaque;
	HashPageOpaque nopaque;
	uint32		maxbucket;
	uint32		highmask;
	uint32		lowmask;
//	bool		metap_update_masks = false;
//	bool		metap_update_splitpoint = false;


	_hash_checkpage_s(rel, metabuf, LH_META_PAGE);
	metap = HashPageGetMeta_s(BufferGetPage_s(rel, metabuf));

	/*
	 * Check to see if split is still needed; someone else might have already
	 * done one while we waited for the lock.
	 *
	 * Make sure this stays in sync with _hash_doinsert()
	 */
	if (metap->hashm_ntuples <=
		(double) metap->hashm_ffactor * (metap->hashm_maxbucket + 1))
		goto fail;

	/*
	 * Can't split anymore if maxbucket has reached its maximum possible
	 * value.
	 *
	 * Ideally we'd allow bucket numbers up to UINT_MAX-1 (no higher because
	 * the calculation maxbucket+1 mustn't overflow).  Currently we restrict
	 * to half that because of overflow looping in _hash_log2() and
	 * insufficient space in hashm_spares[].  It's moot anyway because an
	 * index with 2^32 buckets would certainly overflow BlockNumber and hence
	 * _hash_alloc_buckets() would fail, but if we supported buckets smaller
	 * than a disk block then this would be an independent constraint.
	 *
	 * If you change this, see also the maximum initial number of buckets in
	 * _hash_init().
	 */
	if (metap->hashm_maxbucket >= (uint32) 0x7FFFFFFE)
		goto fail;

	/*
	 * Determine which bucket is to be split, and attempt to take cleanup lock
	 * on the old bucket.  If we can't get the lock, give up.
	 *
	 * The cleanup lock protects us not only against other backends, but
	 * against our own backend as well.
	 *
	 * The cleanup lock is mainly to protect the split from concurrent
	 * inserts. See src/backend/access/hash/README, Lock Definitions for
	 * further details.  Due to this locking restriction, if there is any
	 * pending scan, the split will give up which is not good, but harmless.
	 */
	new_bucket = metap->hashm_maxbucket + 1;

	old_bucket = (new_bucket & metap->hashm_lowmask);
	//selog(DEBUG1, "Old bucket to split is %d", old_bucket);
	start_oblkno = BUCKET_TO_BLKNO_s(metap, old_bucket);
	//selog(DEBUG1, "Start block number is %d", start_oblkno);
	buf_oblkno = _hash_getbuf_with_condlock_cleanup_s(rel, start_oblkno, LH_BUCKET_PAGE);
	if (!buf_oblkno)
		goto fail;

	opage = BufferGetPage_s(rel, buf_oblkno);
	oopaque = (HashPageOpaque) PageGetSpecialPointer_s(opage);


	/*
	 * There shouldn't be any active scan on new bucket.
	 *
	 * Note: it is safe to compute the new bucket's blkno here, even though we
	 * may still need to update the BUCKET_TO_BLKNO mapping.  This is because
	 * the current value of hashm_spares[hashm_ovflpoint] correctly shows
	 * where we are going to put a new splitpoint's worth of buckets.
	 */
	start_nblkno = BUCKET_TO_BLKNO_s(metap, new_bucket);
	//selog(DEBUG1, "new bucket block number is %d", start_nblkno);
	/*
	 * If the split point is increasing we need to allocate a new batch of
	 * bucket pages.
	 */
	spare_ndx = _hash_spareindex_s(new_bucket + 1);
	//selog(DEBUG1, "spare_ndx is %d", spare_ndx);
	if (spare_ndx > metap->hashm_ovflpoint)
	{
		uint32		buckets_to_add;

		//Assert(spare_ndx == metap->hashm_ovflpoint + 1);
		//selog(DEBUG1, "New buckets are needed");
		/*
		 * We treat allocation of buckets as a separate WAL-logged action.
		 * Even if we fail after this operation, won't leak bucket pages;
		 * rather, the next split will consume this space. In any case, even
		 * without failure we don't use all the space in one split operation.
		 */
		buckets_to_add = _hash_get_totalbuckets_s(spare_ndx) - new_bucket;
		//selog(DEBUG1, "Going to allocate %d buckets", buckets_to_add);
		if (!_hash_alloc_buckets_s(rel, start_nblkno, buckets_to_add))
		{
			//selog(DEBUG1, "Cant't split due to Block number overflow");
			/* can't split due to BlockNumber overflow */
			ReleaseBuffer_s(rel, buf_oblkno);
			goto fail;
		}
	}

	/*
	 * Physically allocate the new bucket's primary page.  We want to do this
	 * before changing the metapage's mapping info, in case we can't get the
	 * disk space.  Ideally, we don't need to check for cleanup lock on new
	 * bucket as no other backend could find this bucket unless meta page is
	 * updated.  However, it is good to be consistent with old bucket locking.
	 */
	//selog(DEBUG1, "Going to initiate new bucket %d", start_nblkno);
	buf_nblkno = _hash_getnewbuf_s(rel, start_nblkno);
	/*if (!IsBufferCleanupOK(buf_nblkno))
	{
		_hash_relbuf(rel, buf_oblkno);
		_hash_relbuf(rel, buf_nblkno);
		goto fail;
	}*/
	//selog(DEBUG1, "New bucket %d initiated", buf_nblkno);
	/*
	 * Okay to proceed with split.  Update the metapage bucket mapping info.
	 */
	metap->hashm_maxbucket = new_bucket;
	//selog(DEBUG1, "Increased metapage hashm_maxbucke to %d", new_bucket);

	if (new_bucket > metap->hashm_highmask)
	{
		/* Starting a new doubling */
		metap->hashm_lowmask = metap->hashm_highmask;
		metap->hashm_highmask = new_bucket | metap->hashm_lowmask;
		//metap_update_masks = true;
	}

	/*
	 * If the split point is increasing we need to adjust the hashm_spares[]
	 * array and hashm_ovflpoint so that future overflow pages will be created
	 * beyond this new batch of bucket pages.
	 */
	if (spare_ndx > metap->hashm_ovflpoint)
	{
		metap->hashm_spares[spare_ndx] = metap->hashm_spares[metap->hashm_ovflpoint];
		metap->hashm_ovflpoint = spare_ndx;
		//metap_update_splitpoint = true;
	}
	//selog(DEBUG1, "Going to update meta page which has maxbucket set to %d", metap->hashm_maxbucket);
	MarkBufferDirty_s(rel, metabuf);

	/*
	 * Copy bucket mapping info now; this saves re-accessing the meta page
	 * inside _hash_splitbucket's inner loop.  Note that once we drop the
	 * split lock, other splits could begin, so these values might be out of
	 * date before _hash_splitbucket finishes.  That's okay, since all it
	 * needs is to tell which of these two buckets to map hashkeys into.
	 */
	maxbucket = metap->hashm_maxbucket;
	highmask = metap->hashm_highmask;
	lowmask = metap->hashm_lowmask;

	opage = BufferGetPage_s(rel, buf_oblkno);
	oopaque = (HashPageOpaque) PageGetSpecialPointer_s(opage);

	/*
	 * Mark the old bucket to indicate that split is in progress.  (At
	 * operation end, we will clear the split-in-progress flag.)  Also, for a
	 * primary bucket page, hasho_prevblkno stores the number of buckets that
	 * existed as of the last split, so we must update that value here.
	 */
	oopaque->hasho_flag |= LH_BUCKET_BEING_SPLIT;
	oopaque->hasho_prevblkno = maxbucket;
	//selog(DEBUG1, "Going to update old bucket %d", buf_oblkno);
	MarkBufferDirty_s(rel, buf_oblkno);

	npage = BufferGetPage_s(rel, buf_nblkno);

	/*
	 * initialize the new bucket's primary page and mark it to indicate that
	 * split is in progress.
	 */
	nopaque = (HashPageOpaque) PageGetSpecialPointer_s(npage);
	nopaque->hasho_prevblkno = maxbucket;
	nopaque->hasho_nextblkno = InvalidBlockNumber;
	nopaque->hasho_bucket = new_bucket;
	nopaque->hasho_flag = LH_BUCKET_PAGE | LH_BUCKET_BEING_POPULATED;
	nopaque->hasho_page_id = HASHO_PAGE_ID;

	//selog(DEBUG1, "Going to update new bucket %d", buf_nblkno);
	MarkBufferDirty_s(rel, buf_nblkno);

	//selog(DEBUG1, "Going to split buckets");
	/* Relocate records to the new bucket */
	_hash_splitbucket_s(rel, metabuf,
					  old_bucket, new_bucket,
					  buf_oblkno, buf_nblkno,
					  maxbucket, highmask, lowmask);

	//selog(DEBUG1, "Buckets have been split");
	/* all done, now release the pins on primary buckets. */
	ReleaseBuffer_s(rel, buf_oblkno);
	ReleaseBuffer_s(rel, buf_nblkno);

	return;

	/* Here if decide not to split or fail to acquire old bucket lock */
fail:
	selog(ERROR, "failed on _hash_expandtable");
	/* We didn't write the metapage, so just drop lock */
	//LockBuffer(metabuf, BUFFER_LOCK_UNLOCK);
}


/*
 * _hash_alloc_buckets -- allocate a new splitpoint's worth of bucket pages
 *
 * This does not need to initialize the new bucket pages; we'll do that as
 * each one is used by _hash_expandtable().  But we have to extend the logical
 * EOF to the end of the splitpoint; this keeps smgr's idea of the EOF in
 * sync with ours, so that we don't get complaints from smgr.
 *
 * We do this by writing a page of zeroes at the end of the splitpoint range.
 * We expect that the filesystem will ensure that the intervening pages read
 * as zeroes too.  On many filesystems this "hole" will not be allocated
 * immediately, which means that the index file may end up more fragmented
 * than if we forced it all to be allocated now; but since we don't scan
 * hash indexes sequentially anyway, that probably doesn't matter.
 *
 * XXX It's annoying that this code is executed with the metapage lock held.
 * We need to interlock against _hash_addovflpage() adding a new overflow page
 * concurrently, but it'd likely be better to use LockRelationForExtension
 * for the purpose.  OTOH, adding a splitpoint is a very infrequent operation,
 * so it may not be worth worrying about.
 *
 * Returns true if successful, or false if allocation failed due to
 * BlockNumber overflow.
 */
static bool
_hash_alloc_buckets_s(VRelation rel, BlockNumber firstblock, uint32 nblocks)
{
	//BlockNumber lastblock;
	//PGAlignedBlock zerobuf;
	//Page		page;
	//HashPageOpaque ovflopaque;

	//lastblock = firstblock + nblocks - 1;


	/**
	 * This function deviates from the original postgres function logic a it 
	 * does not initialize the new pages here or uses the postgres storage api
	 * to expand the index relation buckets.
	 * Instead it relies on the fact that the underlying oblivious files have
	 * a fixed set of initialized pages and the soe_bufmgr manages virtual
	 * pages to enable the index to still grow. As such, it by using the 
	 * function _hash_getnewbuf_s it requests the soe_bufmgr to allocate
	 * new virtual pages which can be used by the index.
	 */
	int index;

	for(index=0; index < nblocks; index++){
		_hash_getnewbuf_s(rel, firstblock+index);
	}
	/*
	 * Check for overflow in block number calculation; if so, we cannot extend
	 * the index anymore.
	 */
	//if (lastblock < firstblock || lastblock == InvalidBlockNumber)
	//	return false;

	//page = (Page) zerobuf.data;

	/*
	 * Initialize the page.  Just zeroing the page won't work; see
	 * _hash_freeovflpage for similar usage.  We take care to make the special
	 * space valid for the benefit of tools such as pageinspect.
	 */
	/*_hash_pageinit_s(page, BLCKSZ);

	ovflopaque = (HashPageOpaque) PageGetSpecialPointer_s(page);

	ovflopaque->hasho_prevblkno = InvalidBlockNumber;
	ovflopaque->hasho_nextblkno = InvalidBlockNumber;
	ovflopaque->hasho_bucket = -1;
	ovflopaque->hasho_flag = LH_UNUSED_PAGE;
	ovflopaque->hasho_page_id = HASHO_PAGE_ID;
	ReadBuffer_s(rel, f)*/
	/*
		Storage extension is already made by the ocall when initializing the
		oram files.

	 	RelationOpenSmgr(rel);
		PageSetChecksumInplace(page, lastblock);
	  	smgrextend(rel->rd_smgr, MAIN_FORKNUM, lastblock, zerobuf.data, false);
	*/

	return true;
}


/*
 * _hash_splitbucket -- split 'obucket' into 'obucket' and 'nbucket'
 *
 * This routine is used to partition the tuples between old and new bucket and
 * is used to finish the incomplete split operations.  To finish the previously
 * interrupted split operation, the caller needs to fill htab.  If htab is set,
 * then we skip the movement of tuples that exists in htab, otherwise NULL
 * value of htab indicates movement of all the tuples that belong to the new
 * bucket.
 *
 * We are splitting a bucket that consists of a base bucket page and zero
 * or more overflow (bucket chain) pages.  We must relocate tuples that
 * belong in the new bucket.
 *
 * The caller must hold cleanup locks on both buckets to ensure that
 * no one else is trying to access them (see README).
 *
 * The caller must hold a pin, but no lock, on the metapage buffer.
 * The buffer is returned in the same state.  (The metapage is only
 * touched if it becomes necessary to add or remove overflow pages.)
 *
 * Split needs to retain pin on primary bucket pages of both old and new
 * buckets till end of operation.  This is to prevent vacuum from starting
 * while a split is in progress.
 *
 * In addition, the caller must have created the new bucket's base page,
 * which is passed in buffer nbuf, pinned and write-locked.  The lock will be
 * released here and pin must be released by the caller.  (The API is set up
 * this way because we must do _hash_getnewbuf() before releasing the metapage
 * write lock.  So instead of passing the new bucket's start block number, we
 * pass an actual buffer.)
 */
static void
_hash_splitbucket_s(VRelation rel,
				  Buffer metabuf,
				  Bucket obucket,
				  Bucket nbucket,
				  Buffer obuf,
				  Buffer nbuf,
				  uint32 maxbucket,
				  uint32 highmask,
				  uint32 lowmask)
{
	Buffer		bucket_obuf;
	Buffer		bucket_nbuf;
	Page		opage;
	Page		npage;
	HashPageOpaque oopaque;
	HashPageOpaque nopaque;
	OffsetNumber itup_offsets[MaxIndexTuplesPerPage];
	IndexTuple	itups[MaxIndexTuplesPerPage];
	Size		all_tups_size = 0;
	int			i;
	uint16		nitups = 0;

	bucket_obuf = obuf;
	//selog(DEBUG1, "going to get old page %d", obuf);
	opage = BufferGetPage_s(rel, obuf);
	oopaque = (HashPageOpaque) PageGetSpecialPointer_s(opage);

	bucket_nbuf = nbuf;
	//selog(DEBUG1, "going to get new page %d", nbuf);
	npage = BufferGetPage_s(rel, nbuf);
	nopaque = (HashPageOpaque) PageGetSpecialPointer_s(npage);

	/* Copy the predicate locks from old bucket to new bucket. */
	/*PredicateLockPageSplit(rel,
						   BufferGetBlockNumber(bucket_obuf),
						   BufferGetBlockNumber(bucket_nbuf));*/

	/*
	 * Partition the tuples in the old bucket between the old bucket and the
	 * new bucket, advancing along the old bucket's overflow bucket chain and
	 * adding overflow pages to the new bucket as needed.  Outer loop iterates
	 * once per page in old bucket.
	 */
	for (;;)
	{
		BlockNumber oblkno;
		OffsetNumber ooffnum;
		OffsetNumber omaxoffnum;

		/* Scan each tuple in old page */
		omaxoffnum = PageGetMaxOffsetNumber_s(opage);
		for (ooffnum = FirstOffsetNumber;
			 ooffnum <= omaxoffnum;
			 ooffnum = OffsetNumberNext_s(ooffnum))
		{
			IndexTuple	itup;
			Size		itemsz;
			Bucket		bucket;
			//bool		found = false;

			/* skip dead tuples */
			if (ItemIdIsDead_s(PageGetItemId_s(opage, ooffnum)))
				continue;
			//selog(DEBUG1, "Accessing item %d in bucket %d", ooffnum, bucket_obuf);
			itup = (IndexTuple) PageGetItem_s(opage,
											PageGetItemId_s(opage, ooffnum));


			bucket = _hash_hashkey2bucket_s(_hash_get_indextuple_hashkey_s(itup),
										  maxbucket, highmask, lowmask);
			//selog(DEBUG1, "Item new bucket is %d", bucket);

			if (bucket == nbucket)
			{
				//selog(DEBUG1, "tuple goes to new page");
				IndexTuple	new_itup;
				/**
				 * TODO: CopyIndexTuple has to migrated to inside of the enclave.
				 */
				/*
				 * make a copy of index tuple as we have to scribble on it.
				 */
				new_itup = CopyIndexTuple_s(itup);

				/*
				 * mark the index tuple as moved by split, such tuples are
				 * skipped by scan if there is split in progress for a bucket.
				 */
				new_itup->t_info |= INDEX_MOVED_BY_SPLIT_MASK;

				/*
				 * insert the tuple into the new bucket.  if it doesn't fit on
				 * the current page in the new bucket, we must allocate a new
				 * overflow page and place the tuple on that page instead.
				 */
				itemsz = IndexTupleSize_s(new_itup);
				itemsz = MAXALIGN_s(itemsz);
				//selog(DEBUG1, "Checking if no more tuples fit in the new page");

				if (PageGetFreeSpaceForMultipleTuples_s(npage, nitups + 1) < (all_tups_size + itemsz))
				{
					//selog(DEBUG1, "Going to add new tuples to page");
					_hash_pgaddmultitup_s(rel, nbuf, itups, itup_offsets, nitups);

					MarkBufferDirty_s(rel, nbuf);



					/* be tidy */
					for (i = 0; i < nitups; i++)
						free(itups[i]);
					nitups = 0;
					all_tups_size = 0;
					//selog(DEBUG1, "Going to add new overflow page");
					/* chain to a new overflow page */
					nbuf = _hash_addovflpage_s(rel, metabuf, nbuf, (nbuf == bucket_nbuf) ? true : false);
					npage = BufferGetPage_s(rel, nbuf);
					nopaque = (HashPageOpaque) PageGetSpecialPointer_s(npage);
				}

				itups[nitups++] = new_itup;
				all_tups_size += itemsz;
			}
			else
			{
				//selog(DEBUG1, "Tuple stays in the same page");
				/*
				 * the tuple stays on this page, so nothing to do.
				 */
				//Assert(bucket == obucket);
			}
		}

		oblkno = oopaque->hasho_nextblkno;

		/* retain the pin on the old primary bucket */
		if (obuf != bucket_obuf)
			ReleaseBuffer_s(rel, obuf);

		/* Exit loop if no more overflow pages in old bucket */
		if (!BlockNumberIsValid_s(oblkno))
		{
			/*
			 * Change the shared buffer state in critical section, otherwise
			 * any error could make it unrecoverable.
			 */
			//selog(DEBUG1, "No more overflow pages. Adding tuples to buffer %d", nbuf);
			_hash_pgaddmultitup_s(rel, nbuf, itups, itup_offsets, nitups);
			MarkBufferDirty_s(rel, nbuf);


			if (nbuf != bucket_nbuf){
				//selog(DEBUG1, "1 - Going to release buffer %d", nbuf);
				ReleaseBuffer_s(rel, nbuf);
			}

			/* be tidy */
			for (i = 0; i < nitups; i++)
				free(itups[i]);
			break;
		}

		//selog(DEBUG1, "Advance to the next old page %d", oblkno);
		/* Else, advance to next old page */
		obuf = _hash_getbuf_s(rel, oblkno, HASH_READ, LH_OVERFLOW_PAGE);
		opage = BufferGetPage_s(rel, obuf);
		oopaque = (HashPageOpaque) PageGetSpecialPointer_s(opage);
	}

	/*
	 * We're at the end of the old bucket chain, so we're done partitioning
	 * the tuples.  Mark the old and new buckets to indicate split is
	 * finished.
	 *
	 * To avoid deadlocks due to locking order of buckets, first lock the old
	 * bucket and then the new bucket.
	 */
	//selog(DEBUG1, "Going to update pages flags after split");
	//LockBuffer(bucket_obuf, BUFFER_LOCK_EXCLUSIVE);
	opage = BufferGetPage_s(rel, bucket_obuf);
	oopaque = (HashPageOpaque) PageGetSpecialPointer_s(opage);

	//LockBuffer(bucket_nbuf, BUFFER_LOCK_EXCLUSIVE);
	npage = BufferGetPage_s(rel, bucket_nbuf);
	nopaque = (HashPageOpaque) PageGetSpecialPointer_s(npage);


	oopaque->hasho_flag &= ~LH_BUCKET_BEING_SPLIT;
	nopaque->hasho_flag &= ~LH_BUCKET_BEING_POPULATED;

	/*
	 * After the split is finished, mark the old bucket to indicate that it
	 * contains deletable tuples.  We will clear split-cleanup flag after
	 * deleting such tuples either at the end of split or at the next split
	 * from old bucket or at the time of vacuum.
	 */
	oopaque->hasho_flag |= LH_BUCKET_NEEDS_SPLIT_CLEANUP;

	/*
	 * now write the buffers, here we don't release the locks as caller is
	 * responsible to release locks.
	 */
	MarkBufferDirty_s(rel, bucket_obuf);
	MarkBufferDirty_s(rel, bucket_nbuf);


	/*
	 * If possible, clean up the old bucket.  We might not be able to do this
	 * if someone else has a pin on it, but if not then we can go ahead.  This
	 * isn't absolutely necessary, but it reduces bloat; if we don't do it
	 * now, VACUUM will do it eventually, but maybe not until new overflow
	 * pages have been allocated.  Note that there's no need to clean up the
	 * new bucket.
	 */
	//if (IsBufferCleanupOK(bucket_obuf))
	//{
	//selog(DEBUG1, "Going to cleanup bucket %d with buffer %d", obucket, bucket_obuf);
	hashbucketcleanup_s(rel, obucket, bucket_obuf,
					  BufferGetBlockNumber_s(bucket_obuf),
					  maxbucket, highmask, lowmask);
	//}
}

/*
 *	_hash_getcachedmetap() -- Returns cached metapage data.
 *
 *	If metabuf is not InvalidBuffer, caller must hold a pin, but no lock, on
 *	the metapage.  If not set, we'll set it before returning if we have to
 *	refresh the cache, and return with a pin but no lock on it; caller is
 *	responsible for releasing the pin.
 *
 *	We refresh the cache if it's not initialized yet or force_refresh is true.
 */
HashMetaPage
_hash_getcachedmetap_s(VRelation rel, Buffer *metabuf, bool force_refresh)
{
	Page		page;

	//Assert(metabuf);
	if (force_refresh || rel->rd_amcache == NULL)
	{

		*metabuf = _hash_getbuf_s(rel, HASH_METAPAGE, HASH_READ,
									LH_META_PAGE);
		page = BufferGetPage_s(rel, *metabuf);

		/* Populate the cache. */
		if (rel->rd_amcache == NULL)
			//selog(DEBUG1, "Creating cache for hash page meta");
			rel->rd_amcache = (char*) malloc(sizeof(HashMetaPageData));

		memcpy(rel->rd_amcache, HashPageGetMeta_s(page),
			   sizeof(HashMetaPageData));

	}

	return (HashMetaPage) rel->rd_amcache;
}

/*
 *	_hash_getbucketbuf_from_hashkey() -- Get the bucket's buffer for the given
 *										 hashkey.
 *
 *	Bucket pages do not move or get removed once they are allocated. This give
 *	us an opportunity to use the previously saved metapage contents to reach
 *	the target bucket buffer, instead of reading from the metapage every time.
 *	This saves one buffer access every time we want to reach the target bucket
 *	buffer, which is very helpful savings in bufmgr traffic and contention.
 *
 *	The access type parameter (HASH_READ or HASH_WRITE) indicates whether the
 *	bucket buffer has to be locked for reading or writing.
 *
 *	The out parameter cachedmetap is set with metapage contents used for
 *	hashkey to bucket buffer mapping. Some callers need this info to reach the
 *	old bucket in case of bucket split, see _hash_doinsert().
 */
Buffer
_hash_getbucketbuf_from_hashkey_s(VRelation rel, uint32 hashkey, int access,
								HashMetaPage cachedmetap)
{
	HashMetaPage metap;
	Buffer		buf;
	Buffer		metabuf = InvalidBuffer;
	Page		page;
	Bucket		bucket;
	BlockNumber blkno;
	HashPageOpaque opaque;


	/* We read from target bucket buffer, hence locking is must. */
	//Assert(access == HASH_READ || access == HASH_WRITE);

	if(cachedmetap == NULL){
		metabuf = _hash_getbuf_s(rel, HASH_METAPAGE, HASH_READ,
									LH_META_PAGE);

		metap = HashPageGetMeta_s(BufferGetPage_s(rel,metabuf));
	}else{
		metap = cachedmetap;
	}
	//metap = //_hash_getcachedmetap_s(rel, &metabuf, false);
	//Assert(metap != NULL);

	/*
	 * Loop until we get a lock on the correct target bucket.
	 * In this implementation the loop dosen't do anything, because 
	 * there are no concurrent hash splits.
	 */
	for (;;)
	{
		/*
		 * Compute the target bucket number, and convert to block number.
		 */
		bucket = _hash_hashkey2bucket_s(hashkey,
									  metap->hashm_maxbucket,
									  metap->hashm_highmask,
									  metap->hashm_lowmask);
		//selog(DEBUG1, "Bucket chosen was %d", bucket);
		blkno = BUCKET_TO_BLKNO_s(metap, bucket);
		//selog(DEBUG1, "Block number chosen was %d", blkno);
		/* Fetch the primary bucket page for the bucket */
		buf = _hash_getbuf_s(rel, blkno, access, LH_BUCKET_PAGE);
		//selog(DEBUG1, "Going to get page %d", buf);
		page = BufferGetPage_s(rel, buf);
		opaque = (HashPageOpaque) PageGetSpecialPointer_s(page);
		//Assert(opaque->hasho_bucket == bucket);
		//Assert(opaque->hasho_prevblkno != InvalidBlockNumber);
		//selog(DEBUG1, "Page retrieved %d", opaque->o_blkno);
		/*
		 * If this bucket hasn't been split, we're done.
		 */
		//selog(DEBUG1, "bucket chosen was %d and has prevlkno %d with meta max bucket %d", blkno, opaque->hasho_prevblkno, metap->hashm_maxbucket);
		if (opaque->hasho_prevblkno <= metap->hashm_maxbucket)
			break;

		/* Drop lock on this buffer, update cached metapage, and retry. */
		/*In the prototype there is no need to reload metapage because there
		 * are no concurrent splits.
		 */
		//metap = _hash_getcachedmetap(rel, &metabuf, true);
		//Assert(metap != NULL);
	}

	//No locks to release
	if (BufferIsValid_s(rel, metabuf)){
		ReleaseBuffer_s(rel, metabuf);
	}

	//if (cachedmetap)
	//	*cachedmetap = metap;

	return buf;
}
