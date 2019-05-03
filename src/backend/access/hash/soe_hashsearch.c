/*-------------------------------------------------------------------------
 *
 * hashsearch.c
 *	  search code for postgres hash tables
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/hash/hashsearch.c
 *
 *-------------------------------------------------------------------------
 */

#include "access/soe_hash.h"

static bool _hash_readpage(IndexScanDesc scan, Buffer *bufP);
static int _hash_load_qualified_items(IndexScanDesc scan, Page page,
						   OffsetNumber offnum);
static inline void _hash_saveitem(HashScanOpaque so, int itemIndex,
			   OffsetNumber offnum, IndexTuple itup);
static void _hash_readnext(IndexScanDesc scan, Buffer *bufp,
			   Page *pagep, HashPageOpaque *opaquep);

/*
 *	_hash_next() -- Get the next item in a scan.
 *
 *		On entry, so->currPos describes the current page, which may
 *		be pinned but not locked, and so->currPos.itemIndex identifies
 *		which item was previously returned.
 *
 *		On successful exit, scan->xs_ctup.t_self is set to the TID
 *		of the next heap tuple. so->currPos is updated as needed.
 *
 *		On failure exit (no more tuples), we return false with pin
 *		held on bucket page but no pins or locks held on overflow
 *		page.
 */
bool
_hash_next(IndexScanDesc scan)
{
	VRelation	rel = scan->indexRelation;
	HashScanOpaque so = (HashScanOpaque) scan->opaque;
	HashScanPosItem *currItem;
	BlockNumber blkno;
	Buffer		buf;
	bool		end_of_scan = false;

	/*
	 * Advance to the next tuple on the current page; or if done, try to read
	 * data from the next or previous page based on the scan direction. Before
	 * moving to the next or previous page make sure that we deal with all the
	 * killed items.
	 */
	if (++so->currPos.itemIndex > so->currPos.lastItem)
	{

		blkno = so->currPos.nextPage;
		if (BlockNumberIsValid(blkno))
		{
			buf = _hash_getbuf(rel, blkno, HASH_READ, LH_OVERFLOW_PAGE);
			//TestForOldSnapshot(scan->xs_snapshot, rel, BufferGetPage(buf));
			if (!_hash_readpage(scan, &buf))
				end_of_scan = true;
		}
		else
			end_of_scan = true;
	}
	
	

	if (end_of_scan)
	{
		_hash_dropscanbuf(rel, so);
		HashScanPosInvalidate(so->currPos);
		return false;
	}

	/* OK, itemIndex says what to return */
	currItem = &so->currPos.items[so->currPos.itemIndex];
	scan->xs_ctup.t_self = currItem->heapTid;

	return true;
}


/*
 * Advance to next page in a bucket, if any.  If we are scanning the bucket
 * being populated during split operation then this function advances to the
 * bucket being split after the last bucket page of bucket being populated.
 */
static void
_hash_readnext(IndexScanDesc scan,
			   Buffer *bufp, Page *pagep, HashPageOpaque *opaquep)
{
	BlockNumber blkno;
	VRelation	rel = scan->indexRelation;
	HashScanOpaque so = (HashScanOpaque) scan->opaque;
	bool		block_found = false;

	blkno = (*opaquep)->hasho_nextblkno;

	/*
	 * Retain the pin on primary bucket page till the end of scan.  Refer the
	 * comments in _hash_first to know the reason of retaining pin.
	 */
	/*if (*bufp == so->hashso_bucket_buf || *bufp == so->hashso_split_bucket_buf)
		LockBuffer(*bufp, BUFFER_LOCK_UNLOCK);
	else
		_hash_relbuf(rel, *bufp);*/

	*bufp = InvalidBuffer;
	/* check for interrupts while we're not holding any buffer lock */
	if (BlockNumberIsValid(blkno))
	{
		*bufp = _hash_getbuf(rel, blkno, HASH_READ, LH_OVERFLOW_PAGE);
		block_found = true;
	}
	

	if (block_found)
	{
		*pagep = BufferGetPage(rel, *bufp);
		//TestForOldSnapshot(scan->xs_snapshot, rel, *pagep);
		*opaquep = (HashPageOpaque) PageGetSpecialPointer(*pagep);
	}
}


/*
 *	_hash_first() -- Find the first item in a scan.
 *
 *		We find the first item (or, if backward scan, the last item) in the
 *		index that satisfies the qualification associated with the scan
 *		descriptor.
 *
 *		On successful exit, if the page containing current index tuple is an
 *		overflow page, both pin and lock are released whereas if it is a bucket
 *		page then it is pinned but not locked and data about the matching
 *		tuple(s) on the page has been loaded into so->currPos,
 *		scan->xs_ctup.t_self is set to the heap TID of the current tuple.
 *
 *		On failure exit (no more tuples), we return false, with pin held on
 *		bucket page but no pins or locks held on overflow page.
 */
bool
_hash_first(IndexScanDesc scan)
{
	VRelation	rel = scan->indexRelation;
	HashScanOpaque so = (HashScanOpaque) scan->opaque;
	ScanKey		cur;
	uint32		hashkey;
	Bucket		bucket;
	Buffer		buf;
	Page		page;
	HashPageOpaque opaque;
	HashScanPosItem *currItem;

	/* There may be more than one index qual, but we hash only the first */
	cur = &scan->keyData[0];

	/* We support only single-column hash indexes */
	//Assert(cur->sk_attno == 1);
	/* And there's only one operator strategy, too */
	//Assert(cur->sk_strategy == HTEqualStrategyNumber);

	/*
	 * If the constant in the index qual is NULL, assume it cannot match any
	 * items in the index.
	 */
	//if (cur->sk_flags & SK_ISNULL)
	//	return false;

	/*
	 * Okay to compute the hash key.  We want to do this before acquiring any
	 * locks, in case a user-defined hash function happens to be slow.
	 *
	 * If scankey operator is not a cross-type comparison, we can use the
	 * cached hash function; otherwise gotta look it up in the catalogs.
	 *
	 * We support the convention that sk_subtype == InvalidOid means the
	 * opclass input type; this is a hack to simplify life for ScanKeyInit().
	 */
	//TODO: Define how to create hashkey
	/*if (cur->sk_subtype == rel->rd_opcintype[0] ||
		cur->sk_subtype == InvalidOid)*/
	 hashkey = _hash_datum2hashkey(rel, cur->sk_argument);
	/*else
		hashkey = _hash_datum2hashkey_type(rel, cur->sk_argument,
										   cur->sk_subtype);*/

	so->hashso_sk_hash = hashkey;

	buf = _hash_getbucketbuf_from_hashkey(rel, hashkey, HASH_READ, NULL);
	page = BufferGetPage(rel, buf);
	opaque = (HashPageOpaque) PageGetSpecialPointer(page);
	bucket = opaque->hasho_bucket;

	so->hashso_bucket_buf = buf;


	/* remember which buffer we have pinned, if any */
	//Assert(BufferIsInvalid(so->currPos.buf));
	so->currPos.buf = buf;

	/* Now find all the tuples satisfying the qualification from a page */
	if (!_hash_readpage(scan, &buf))
		return false;

	/* OK, itemIndex says what to return */
	currItem = &so->currPos.items[so->currPos.itemIndex];
	scan->xs_ctup.t_self = currItem->heapTid;

	/* if we're here, _hash_readpage found a valid tuples */
	return true;
}

/*
 *	_hash_readpage() -- Load data from current index page into so->currPos
 *
 *	We scan all the items in the current index page and save them into
 *	so->currPos if it satisfies the qualification. If no matching items
 *	are found in the current page, we move to the next or previous page
 *	in a bucket chain as indicated by the direction.
 *
 *	Return true if any matching items are found else return false.
 */
static bool
_hash_readpage(IndexScanDesc scan, Buffer *bufP)
{
	VRelation	rel = scan->indexRelation;
	HashScanOpaque so = (HashScanOpaque) scan->opaque;
	Buffer		buf;
	Page		page;
	HashPageOpaque opaque;
	OffsetNumber offnum;
	uint16		itemIndex;

	buf = *bufP;
	//Assert(BufferIsValid(buf));
	_hash_checkpage(rel, buf, LH_BUCKET_PAGE | LH_OVERFLOW_PAGE);
	page = BufferGetPage(rel, buf);
	opaque = (HashPageOpaque) PageGetSpecialPointer(page);

	so->currPos.buf = buf;
	so->currPos.currPage = BufferGetBlockNumber(buf);


	BlockNumber prev_blkno = InvalidBlockNumber;

	for (;;)
	{
		/* new page, locate starting position by binary search */
		offnum = _hash_binsearch(page, so->hashso_sk_hash);

		itemIndex = _hash_load_qualified_items(scan, page, offnum);

		if (itemIndex != 0)
			break;


		/*
		 * If this is a primary bucket page, hasho_prevblkno is not a real
		 * block number.
		 */
		if (so->currPos.buf == so->hashso_bucket_buf ||
			so->currPos.buf == so->hashso_split_bucket_buf)
			prev_blkno = InvalidBlockNumber;
		else
			prev_blkno = opaque->hasho_prevblkno;

		_hash_readnext(scan, &buf, &page, &opaque);
		if (BufferIsValid(rel, buf))
		{
			so->currPos.buf = buf;
			so->currPos.currPage = BufferGetBlockNumber(buf);
		}
		else
		{
			/*
			 * Remember next and previous block numbers for scrollable
			 * cursors to know the start position and return false
			 * indicating that no more matching tuples were found. Also,
			 * don't reset currPage or lsn, because we expect
			 * _hash_kill_items to be called for the old page after this
			 * function returns.
			 */
			so->currPos.prevPage = prev_blkno;
			so->currPos.nextPage = InvalidBlockNumber;
			so->currPos.buf = buf;
			return false;
		}
	}

	so->currPos.firstItem = 0;
	so->currPos.lastItem = itemIndex - 1;
	so->currPos.itemIndex = 0;
	

	so->currPos.prevPage = opaque->hasho_prevblkno;
	so->currPos.nextPage = opaque->hasho_nextblkno;
	_hash_relbuf(rel, so->currPos.buf);
	so->currPos.buf = InvalidBuffer;

	//Assert(so->currPos.firstItem <= so->currPos.lastItem);
	return true;
}

/*
 * Load all the qualified items from a current index page
 * into so->currPos. Helper function for _hash_readpage.
 */
static int
_hash_load_qualified_items(IndexScanDesc scan, Page page,
						   OffsetNumber offnum)
{
	HashScanOpaque so = (HashScanOpaque) scan->opaque;
	IndexTuple	itup;
	int			itemIndex;
	OffsetNumber maxoff;

	maxoff = PageGetMaxOffsetNumber(page);

	/* load items[] in ascending order */
	itemIndex = 0;

	while (offnum <= maxoff)
	{
		//Assert(offnum >= FirstOffsetNumber);
		itup = (IndexTuple) PageGetItem(page, PageGetItemId(page, offnum));

		if (so->hashso_sk_hash == _hash_get_indextuple_hashkey(itup) &&
			_hash_checkqual(scan, itup))
		{
			/* tuple is qualified, so remember it */
			_hash_saveitem(so, itemIndex, offnum, itup);
			itemIndex++;
		}
		else
		{
			/*
			 * No more matching tuples exist in this page. so, exit while
			 * loop.
			 */
			break;
		}

		offnum = OffsetNumberNext(offnum);
	}

	//Assert(itemIndex <= MaxIndexTuplesPerPage);
	return itemIndex;
}

/* Save an index item into so->currPos.items[itemIndex] */
static inline void
_hash_saveitem(HashScanOpaque so, int itemIndex,
			   OffsetNumber offnum, IndexTuple itup)
{
	HashScanPosItem *currItem = &so->currPos.items[itemIndex];

	currItem->heapTid = itup->t_tid;
	currItem->indexOffset = offnum;
}
