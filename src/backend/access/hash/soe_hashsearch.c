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
#include "logger/logger.h"

static bool _hash_readpage_s(IndexScanDesc scan, Buffer bufP);
static int	_hash_load_qualified_items_s(IndexScanDesc scan, Page page,
										 OffsetNumber offnum);
static inline void _hash_saveitem_s(HashScanOpaque so, int itemIndex,
									OffsetNumber offnum, IndexTuple itup);
static void _hash_readnext_s(IndexScanDesc scan, Buffer * bufp,
							 Page * pagep, HashPageOpaque * opaquep);

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
_hash_next_s(IndexScanDesc scan)
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
		/* Read the next page if every item	is read from the current page. */
					blkno = so->currPos.nextPage;

		if (BlockNumberIsValid_s(blkno))
		{
			buf = _hash_getbuf_s(rel, blkno, HASH_READ, LH_OVERFLOW_PAGE);
			if (!_hash_readpage_s(scan, buf))
				end_of_scan = true;
		}
		else
			end_of_scan = true;
	}


	if (end_of_scan)
	{
		/* selog(DEBUG1, "End of scan"); */
		_hash_dropscanbuf_s(rel, so);
		HashScanPosInvalidate_s(so->currPos);
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
_hash_readnext_s(IndexScanDesc scan,
				 Buffer * bufp, Page * pagep, HashPageOpaque * opaquep)
{
	BlockNumber blkno;
	VRelation	rel = scan->indexRelation;
	HashScanOpaque so = (HashScanOpaque) scan->opaque;
	bool		block_found = false;

	blkno = (*opaquep)->hasho_nextblkno;
	/* selog(DEBUG1, "Next block number is %d", blkno); */

	/*
	 * Retain the pin on primary bucket page till the end of scan.  Refer the
	 * comments in _hash_first to know the reason of retaining pin.
	 */
	if (!(*bufp == so->hashso_bucket_buf || *bufp == so->hashso_split_bucket_buf))
		ReleaseBuffer_s(rel, *bufp);


	*bufp = InvalidBuffer;
	/* check for interrupts while we're not holding any buffer lock */
	if (BlockNumberIsValid_s(blkno))
	{
		/*
		 * selog(DEBUG1, "Block number is valid %d
		 * ",BlockNumberIsValid_s(blkno));
		 */
		*bufp = _hash_getbuf_s(rel, blkno, HASH_READ, LH_OVERFLOW_PAGE);
		block_found = true;
	}


	if (block_found)
	{
		*pagep = BufferGetPage_s(rel, *bufp);
		/* TestForOldSnapshot(scan->xs_snapshot, rel, *pagep); */
		*opaquep = (HashPageOpaque) PageGetSpecialPointer_s(*pagep);
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
_hash_first_s(IndexScanDesc scan)
{
	VRelation	rel = scan->indexRelation;
	HashScanOpaque so = (HashScanOpaque) scan->opaque;
	ScanKey		cur;
	uint32		hashkey;

	/* Bucket		bucket; */
	Buffer		buf;

/* 	Page		page; */
/* 	HashPageOpaque opaque; */
	HashScanPosItem *currItem;

	/* There may be more than one index qual, but we hash only the first */
	cur = &scan->keyData[0];


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

	hashkey = _hash_datum2hashkey_s(rel, cur->sk_argument, cur->datumSize);

	so->hashso_sk_hash = hashkey;

	buf = _hash_getbucketbuf_from_hashkey_s(rel, hashkey, HASH_READ, NULL);

	so->hashso_bucket_buf = buf;

	so->currPos.buf = buf;

	/* selog(DEBUG1, "Going to search qualifying tuples on page"); */
	/* Now find all the tuples satisfying the qualification from a page */
	if (!_hash_readpage_s(scan, buf))
		return false;
	/* selog(DEBUG1, "items were found"); */
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
_hash_readpage_s(IndexScanDesc scan, Buffer buf)
{
	VRelation	rel = scan->indexRelation;
	HashScanOpaque so = (HashScanOpaque) scan->opaque;
	Page		page;
	HashPageOpaque opaque;
	OffsetNumber offnum;
	uint16		itemIndex;

	_hash_checkpage_s(rel, buf, LH_BUCKET_PAGE | LH_OVERFLOW_PAGE);

	page = BufferGetPage_s(rel, buf);
	opaque = (HashPageOpaque) PageGetSpecialPointer_s(page);

	so->currPos.buf = buf;
	so->currPos.currPage = BufferGetBlockNumber_s(buf);


	BlockNumber prev_blkno = InvalidBlockNumber;

	for (;;)
	{
		/* selog(DEBUG1, "Going to do binary search on page"); */
		/* new page, locate starting position by binary search */
		offnum = _hash_binsearch_s(page, so->hashso_sk_hash);
		/* selog(DEBUG1, "Found binary search location start %d", offnum); */
		itemIndex = _hash_load_qualified_items_s(scan, page, offnum);
		/* selog(DEBUG1, "Found %d matches", itemIndex); */

		if (itemIndex != 0)
			break;

		/*
		 * Could not find any matching tuples in the current page, move to the
		 * next page. Before leaving the current page, deal with any killed
		 * items.
		 */

		/* selog(DEBUG1, "Going to move to the next page"); */

		/*
		 * If this is a primary bucket page, hasho_prevblkno is not a real
		 * block number.
		 */
		if (so->currPos.buf == so->hashso_bucket_buf ||
			so->currPos.buf == so->hashso_split_bucket_buf)
			prev_blkno = InvalidBlockNumber;
		else
			prev_blkno = opaque->hasho_prevblkno;
		/* selog(DEBUG1, "Going to read next page"); */
		_hash_readnext_s(scan, &buf, &page, &opaque);

		if (BufferIsValid_s(rel, buf))
		{
			/* selog(DEBUG1, "Next page is valid"); */
			so->currPos.buf = buf;
			so->currPos.currPage = BufferGetBlockNumber_s(buf);
		}
		else
		{
			/* selog(DEBUG1, "Next page is invalid"); */
			/*
			 * Remember next and previous block numbers for scrollable cursors
			 * to know the start position and return false indicating that no
			 * more matching tuples were found. Also, don't reset currPage or
			 * lsn, because we expect _hash_kill_items to be called for the
			 * old page after this function returns.
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
	ReleaseBuffer_s(rel, so->currPos.buf);
	so->currPos.buf = InvalidBuffer;

	return true;
}

/*
 * Load all the qualified items from a current index page
 * into so->currPos. Helper function for _hash_readpage.
 */
static int
_hash_load_qualified_items_s(IndexScanDesc scan, Page page,
							 OffsetNumber offnum)
{
	HashScanOpaque so = (HashScanOpaque) scan->opaque;
	IndexTuple	itup;
	int			itemIndex;
	OffsetNumber maxoff;

	maxoff = PageGetMaxOffsetNumber_s(page);

	/* load items[] in ascending order */
	itemIndex = 0;

	while (offnum <= maxoff)
	{
		/* Assert(offnum >= FirstOffsetNumber); */
		itup = (IndexTuple) PageGetItem_s(page, PageGetItemId_s(page, offnum));

		if (so->hashso_sk_hash == _hash_get_indextuple_hashkey_s(itup)) /* &&
																		 * _hash_checkqual(scan,
																		 * itup)) */
		{
			/* selog(DEBUG1, "Qualified item found"); */
			/* tuple is qualified, so remember it */
			_hash_saveitem_s(so, itemIndex, offnum, itup);
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

		offnum = OffsetNumberNext_s(offnum);
	}

	/* Assert(itemIndex <= MaxIndexTuplesPerPage); */
	return itemIndex;
}

/* Save an index item into so->currPos.items[itemIndex] */
static inline void
_hash_saveitem_s(HashScanOpaque so, int itemIndex,
				 OffsetNumber offnum, IndexTuple itup)
{
	HashScanPosItem *currItem = &so->currPos.items[itemIndex];

	currItem->heapTid = itup->t_tid;
	currItem->indexOffset = offnum;
}
