/*-------------------------------------------------------------------------
 *
 * nbtsearch.c
 *	  Search code for postgres btrees.
 *
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/nbtree/nbtsearch.c
 *
 *-------------------------------------------------------------------------
 */

#include "access/soe_ost.h"
#include "storage/soe_ost_ofile.h"
#include "logger/logger.h"

static bool _bt_readpage_ost(IndexScanDesc scan,
							 OffsetNumber offnum);
static void _bt_saveitem_ost(BTScanOpaqueOST so, int itemIndex,
							 OffsetNumber offnum, IndexTuple itup);
static bool _bt_steppage_ost(IndexScanDesc scan);
static bool _bt_readnextpage_ost(IndexScanDesc scan, BlockNumber blkno);
static inline void _bt_initialize_more_data_ost(BTScanOpaqueOST so);



void bt_dummy_search_ost(OSTRelation rel, int maxHeight){
    #ifdef DUMMYS
    int height = 0;
    
    while(height < maxHeight){
        ReadDummyBuffer_ost(rel, height,  0);
        height+=1;
    }
    #endif
}

/*
 *	_bt_search() -- Search the tree for a particular scankey,
 *		or more precisely for the first leaf page it could be on.
 *
 * The passed scankey must be an insertion-type scankey (see nbtree/README),
 * but it can omit the rightmost column(s) of the index.
 *
 * When nextkey is false (the usual case), we are looking for the first
 * item >= scankey.  When nextkey is true, we are looking for the first
 * item strictly greater than scankey.
 *
 * Return value is a stack of parent-page pointers.  *bufP is set to the
 * address of the leaf-page buffer, which is read-locked and pinned.
 * No locks are held on the parent pages, however!
 *
 * If the snapshot parameter is not NULL, "old snapshot" checking will take
 * place during the descent through the tree.  This is not needed when
 * positioning for an insert or delete, so NULL is used for those cases.
 *
 * NOTE that the returned buffer is read-locked regardless of the access
 * parameter.  However, access = BT_WRITE will allow an empty root page
 * to be created and returned.  When access = BT_READ, an empty index
 * will result in *bufP being set to InvalidBuffer.  Also, in BT_WRITE mode,
 * any incomplete splits encountered during the search will be finished.
 */
BTStackOST
_bt_search_ost(OSTRelation rel, int keysz, ScanKey scankey, bool nextkey,
			   Buffer * bufP, int access, bool doDummy)
{
	BTStackOST	stack_in = NULL;
	unsigned int height = 0;
    Buffer dummy;

	rel->level = height;

	/* Get the root page to start with */
	*bufP = _bt_getroot_ost(rel, access);


	/* Loop iterates once per level descended in the tree */
	for (;;)
	{
		Page		page;
		BTPageOpaqueOST opaque;
		OffsetNumber offnum;
		ItemId		itemid;
		IndexTuple	itup;
		BlockNumber blkno;
		BlockNumber par_blkno;
		BTStackOST	new_stack;

		/*
		 * Race -- the page we just grabbed may have split since we read its
		 * pointer in the parent (or metapage).  If it has, we may need to
		 * move right to its new sibling.  Do that.
		 *
		 * In write-mode, allow _bt_moveright to finish any incomplete splits
		 * along the way.  Strictly speaking, we'd only need to finish an
		 * incomplete split on the leaf page we're about to insert to, not on
		 * any of the upper levels (they are taken care of in _bt_getstackbuf,
		 * if the leaf page is split and we insert to the parent page).  But
		 * this is a good opportunity to finish splits of internal pages too.
		 */
		/* Concurrent splits are not supported on the prototype. */
		/**bufP = _bt_moveright(rel, *bufP, keysz, scankey, nextkey,
							  (access == BT_WRITE), stack_in,
							  BT_READ, snapshot);*/


		/* if this is a leaf page, we're done */
		page = BufferGetPage_ost(rel, *bufP);
		opaque = (BTPageOpaqueOST) PageGetSpecialPointer_s(page);
		if (P_ISLEAF_OST(opaque))
		{
            #ifdef DUMMYS
            while(doDummy && height < rel->osts->nlevels){
                dummy = ReadDummyBuffer_ost(rel, height, 0);
                height += 1;
                rel->level += height;
            } 
            #endif
			break;
		}

		/*
		 * Find the appropriate item on the internal page, and get the child
		 * page that it points to.
		 */

		offnum = _bt_binsrch_ost(rel, *bufP, keysz, scankey, nextkey);
		itemid = PageGetItemId_s(page, offnum);
		itup = (IndexTuple) PageGetItem_s(page, itemid);
		blkno = BTreeInnerTupleGetDownLink_OST(itup);

		par_blkno = BufferGetBlockNumber_ost(*bufP);

		/*
		 * We need to save the location of the index entry we chose in the
		 * parent page on a stack. In case we split the tree, we'll use the
		 * stack to work back up to the parent page.  We also save the actual
		 * downlink (block) to uniquely identify the index entry, in case it
		 * moves right while we're working lower in the tree.  See the paper
		 * by Lehman and Yao for how this is detected and handled. (We use the
		 * child link to disambiguate duplicate keys in the index -- Lehman
		 * and Yao disallow duplicate keys.)
		 */
		new_stack = (BTStackOST) malloc(sizeof(BTStackDataOST));
		new_stack->bts_blkno = par_blkno;
		new_stack->bts_offset = offnum;
		new_stack->bts_btentry = blkno;
		new_stack->bts_parent = stack_in;

		ReleaseBuffer_ost(rel, *bufP);
		height += 1;
		rel->level = height;

		*bufP = ReadBuffer_ost(rel, blkno);

		/* okay, all set to move down a level */
		stack_in = new_stack;
	}

	return stack_in;
}

/*
 *	_bt_binsrch() -- Do a binary search for a key on a particular page.
 *
 * The passed scankey must be an insertion-type scankey (see nbtree/README),
 * but it can omit the rightmost column(s) of the index.
 *
 * When nextkey is false (the usual case), we are looking for the first
 * item >= scankey.  When nextkey is true, we are looking for the first
 * item strictly greater than scankey.
 *
 * On a leaf page, _bt_binsrch() returns the OffsetNumber of the first
 * key >= given scankey, or > scankey if nextkey is true.  (NOTE: in
 * particular, this means it is possible to return a value 1 greater than the
 * number of keys on the page, if the scankey is > all keys on the page.)
 *
 * On an internal (non-leaf) page, _bt_binsrch() returns the OffsetNumber
 * of the last key < given scankey, or last key <= given scankey if nextkey
 * is true.  (Since _bt_compare treats the first data key of such a page as
 * minus infinity, there will be at least one key < scankey, so the result
 * always points at one of the keys on the page.)  This key indicates the
 * right place to descend to be sure we find all leaf keys >= given scankey
 * (or leaf keys > given scankey when nextkey is true).
 *
 * This procedure is not responsible for walking right, it just examines
 * the given page.  _bt_binsrch() has no lock or refcount side effects
 * on the buffer.
 */
OffsetNumber
_bt_binsrch_ost(OSTRelation rel,
				Buffer buf,
				int keysz,
				ScanKey scankey,
				bool nextkey)
{
	Page		page;
	BTPageOpaqueOST opaque;
	OffsetNumber low,
				high;
	int32		result,
				cmpval;

	page = BufferGetPage_ost(rel, buf);
	opaque = (BTPageOpaqueOST) PageGetSpecialPointer_s(page);

	low = P_FIRSTDATAKEY_OST(opaque);
	high = PageGetMaxOffsetNumber_s(page);

	/*
	 * If there are no keys on the page, return the first available slot. Note
	 * this covers two cases: the page is really empty (no keys), or it
	 * contains only a high key.  The latter case is possible after vacuuming.
	 * This can never happen on an internal page, however, since they are
	 * never empty (an internal page must have children).
	 */
	if (high < low)
	{
		/* selog(DEBUG1, "No keys on page, returing first slot"); */
		return low;
	}

	/*
	 * Binary search to find the first key on the page >= scan key, or first
	 * key > scankey when nextkey is true.
	 *
	 * For nextkey=false (cmpval=1), the loop invariant is: all slots before
	 * 'low' are < scan key, all slots at or after 'high' are >= scan key.
	 *
	 * For nextkey=true (cmpval=0), the loop invariant is: all slots before
	 * 'low' are <= scan key, all slots at or after 'high' are > scan key.
	 *
	 * We can fall out when high == low.
	 */
	high++;						/* establish the loop invariant for high */

	cmpval = nextkey ? 0 : 1;	/* select comparison value */

	while (high > low)
	{
		OffsetNumber mid = low + ((high - low) / 2);

		/* We have low <= mid < high, so mid points at a real slot */
		result = _bt_compare_ost(rel, keysz, scankey, page, mid);

		if (result >= cmpval)
			low = mid + 1;
		else
			high = mid;
	}

	/*
	 * At this point we have high == low, but be careful: they could point
	 * past the last slot on the page.
	 *
	 * On a leaf page, we always return the first key >= scan key (resp. >
	 * scan key), which could be the last slot + 1.
	 */
	if (P_ISLEAF_OST(opaque))
		return low;

	/*
	 * On a non-leaf page, return the last key < scan key (resp. <= scan key).
	 * There must be one if _bt_compare() is playing by the rules.
	 */

	return OffsetNumberPrev_s(low);
}



/*----------
 *	_bt_compare() -- Compare scankey to a particular tuple on the page.
 *
 * The passed scankey must be an insertion-type scankey (see nbtree/README),
 * but it can omit the rightmost column(s) of the index.
 *
 *	keysz: number of key conditions to be checked (might be less than the
 *		number of index columns!)
 *	page/offnum: location of btree item to be compared to.
 *
 *		This routine returns:
 *			<0 if scankey < tuple at offnum;
 *			 0 if scankey == tuple at offnum;
 *			>0 if scankey > tuple at offnum.
 *		NULLs in the keys are treated as sortable values.  Therefore
 *		"equality" does not necessarily mean that the item should be
 *		returned to the caller as a matching key!
 *
 * CRUCIAL NOTE: on a non-leaf page, the first data key is assumed to be
 * "minus infinity": this routine will always claim it is less than the
 * scankey.  The actual key value stored (if any, which there probably isn't)
 * does not matter.  This convention allows us to implement the Lehman and
 * Yao convention that the first down-link pointer is before the first key.
 * See backend/access/nbtree/README for details.
 *----------
 */
int32
_bt_compare_ost(OSTRelation rel,
				int keysz,
				ScanKey scankey,
				Page page,
				OffsetNumber offnum)
{
	char	   *datum;
	IndexTuple	itup;
	int32		result;

	result = 0;


	BTPageOpaqueOST opaque = (BTPageOpaqueOST) PageGetSpecialPointer_s(page);

	/*
	 * Force result ">" if target item is first data item on an internal page
	 * --- see NOTE above.
	 */
	if (!P_ISLEAF_OST(opaque) && offnum == P_FIRSTDATAKEY_OST(opaque))
		return 1;

	itup = (IndexTuple) PageGetItem_s(page, PageGetItemId_s(page, offnum));



	/* datum = NameStr_s(*DatumGetName_s(index_getattr_s(itup))); */
	datum = VARDATA_ANY_S(DatumGetBpCharPP_S(index_getattr_s(itup)));

	/* We assume we are comparing strings(varchars) */
    result = (int32) strncmp(scankey->sk_argument, datum, strlen(datum)-1);

	/* if the keys are unequal, return the difference */
	if (result != 0)
		return result;

	/* if we get here, the keys are equal */
	return 0;
}

/*
 *	_bt_first() -- Find the first item in a scan.
 *
 *		We need to be clever about the direction of scan, the search
 *		conditions, and the tree ordering.  We find the first item (or,
 *		if backwards scan, the last item) in the tree that satisfies the
 *		qualifications in the scan key.  On success exit, the page containing
 *		the current index tuple is pinned but not locked, and data about
 *		the matching tuple(s) on the page has been loaded into so->currPos.
 *		scan->xs_ctup.t_self is set to the heap TID of the current tuple,
 *		and if requested, scan->xs_itup points to a copy of the index tuple.
 *
 * If there are no matching items in the index, we return false, with no
 * pins or locks held.
 *
 * Note that scan->keyData[], and the so->keyData[] scankey built from it,
 * are both search-type scankeys (see nbtree/README for more about this).
 * Within this routine, we build a temporary insertion-type scankey to use
 * in locating the scan start position.
 */
bool
_bt_first_ost(IndexScanDesc scan)
{
	OSTRelation rel = scan->ost;
	BTScanOpaqueOST so = (BTScanOpaqueOST) scan->opaque;
	Buffer		buf;
	BTStackOST	stack;
	OffsetNumber offnum;

	/* StrategyNumber strat; */
	bool		nextkey;
	bool		goback;

/* 	ScanKey		startKeys[INDEX_MAX_KEYS]; */
/* 	ScanKeyData scankeys[INDEX_MAX_KEYS]; */
/* 	ScanKeyData notnullkeys[INDEX_MAX_KEYS]; */
	int			keysCount = 0;

/* 	int			i; */
/* 	bool		status = true; */
	/* StrategyNumber strat_total; */
	BTScanPosItemOST *currItem;

/* 	BlockNumber blkno; */


	ScanKey		cur = scan->keyData;

	/**
	 * By debugging postgres, a search on a btree with a single leaf and no
	 * nodes had always the strat_total = BTEqualStrategyNumber despite the
	 * comparison operator on the where clause.
	 *
	 */

	/* strat_total = BTEqualStrategyNumber; */

	/*----------
	 * Examine the selected initial-positioning strategy to determine exactly
	 * where we need to start the scan, and set flag variables to control the
	 * code below.
	 *
	 * If nextkey = false, _bt_search and _bt_binsrch will locate the first
	 * item >= scan key.  If nextkey = true, they will locate the first
	 * item > scan key.
	 *
	 * If goback = true, we will then step back one item, while if
	 * goback = false, we will start the scan on the located item.
	 *----------
	 */
	/* selog(DEBUG1, "bt_first scan opoid is %d", scan->opoid); */
	/* The protoype currently does not support backward scans. */
	switch (scan->opoid)
	{
		case 1058:
			/* BTLessStrategyNumber: */

			/*
			 * Find first item >= scankey, then back up one to arrive at last
			 * item < scankey.  (Note: this positioning strategy is only used
			 * for a backward scan, so that is always the correct starting
			 * position.)
			 */
			nextkey = false;
			goback = true;
			selog(ERROR, "Less or equal strategy requires backward scan no supported");
			break;

		case 1059:
			/* BTLessEqualStrategyNumber: */

			/*
			 * Find first item > scankey, then back up one to arrive at last
			 * item <= scankey.  (Note: this positioning strategy is only used
			 * for a backward scan, so that is always the correct starting
			 * position.)
			 */
			nextkey = true;
			goback = true;
			selog(ERROR, "Less than strategy requires backward scan no supported");
			break;

		case 1054:
			/* BTEqualStrategyNumber: */



			/*
			 * This is the same as the <= strategy.  We will check at the end
			 * whether the found item is actually =.
			 */
			nextkey = false;
			goback = false;
			break;

		case 1061:
			/* BTGreaterEqualStrategyNumber: */

			/*
			 * Find first item >= scankey.  (This is only used for forward
			 * scans.)
			 */
			nextkey = false;
			goback = false;
			break;

		case 1060:
			/* BTGreaterStrategyNumber: */

			/*
			 * Find first item > scankey.  (This is only used for forward
			 * scans.)
			 */
			nextkey = true;
			goback = false;
			break;

		default:
			/* can't get here, but keep compiler quiet */
			selog(ERROR, "unrecognized strat_total: %d", (int) scan->opoid);
			return false;
	}

	/*
	 * Use the manufactured insertion scan key to descend the tree and
	 * position ourselves on the target leaf page.
	 */
	stack = _bt_search_ost(rel, 1, cur, nextkey, &buf, BT_READ_OST, true);
	/* don't need to keep the stack around... */
	_bt_freestack_ost(stack);
	/* selog(DEBUG1, "GOING to initialize more data"); */

	_bt_initialize_more_data_ost(so);
	/* position to the precise item on the page */
	offnum = _bt_binsrch_ost(rel, buf, keysCount, cur, nextkey);
	
    /*
	 * If nextkey = false, we are positioned at the first item >= scan key, or
	 * possibly at the end of a page on which all the existing items are less
	 * than the scan key and we know that everything on later pages is greater
	 * than or equal to scan key.
	 *
	 * If nextkey = true, we are positioned at the first item > scan key, or
	 * possibly at the end of a page on which all the existing items are less
	 * than or equal to the scan key and we know that everything on later
	 * pages is greater than scan key.
	 *
	 * The actually desired starting point is either this item or the prior
	 * one, or in the end-of-page case it's the first item on the next page or
	 * the last item on this page.  Adjust the starting offset if needed. (If
	 * this results in an offset before the first item or after the last one,
	 * _bt_readpage will report no items found, and then we'll step to the
	 * next page as needed.)
	 */
	if (goback)
	{
		offnum = OffsetNumberPrev_s(offnum);
		/* selog(DEBUG1, "Found match on offset prev %d", offnum); */
	}


	/* remember which buffer we have pinned, if any */
	/* Assert(!BTScanPosIsValid(so->currPos)); */
	so->currPos.buf = buf;

	/*
	 * Now load data from the first page of the scan.
	 */
	if (!_bt_readpage_ost(scan, offnum))
	{
		 selog(DEBUG1, "Page has no match, move to next page!"); 
		/*
		 * There's no actually-matching data on this page.  Try to advance to
		 * the next page.  Return false if there's no matching data at all.
		 */
		/* LockBuffer(so->currPos.buf, BUFFER_LOCK_UNLOCK); */
#ifdef DUMMYS
        selog(DEBUG1, "Not found");
        return false;
#else
        selog(DEBUG1, "On dummys not found");
        if (!_bt_steppage_ost(scan))
			return false;
#endif	
    }

	/* else */
	/* { */
	/* selog(DEBUG1, "Match found! Maybe something needs to happen"); */
	/* selog(DEBUG1, "current pos is %d", so->currPos.itemIndex); */
	/* Drop the lock, and maybe the pin, on the current page */
	/* /_bt_drop_lock_and_maybe_pin(scan, &so->currPos); */
	/* } */

	/* readcomplete: */
	/* OK, itemIndex says what to return */
	currItem = &so->currPos.items[so->currPos.itemIndex];
	scan->xs_ctup.t_self = currItem->heapTid;

	return true;
}

/*
 *	_bt_next() -- Get the next item in a scan.
 *
 *		On entry, so->currPos describes the current page, which may be pinned
 *		but is not locked, and so->currPos.itemIndex identifies which item was
 *		previously returned.
 *
 *		On successful exit, scan->xs_ctup.t_self is set to the TID of the
 *		next heap tuple, and if requested, scan->xs_itup points to a copy of
 *		the index tuple.  so->currPos is updated as needed.
 *
 *		On failure exit (no more tuples), we release pin and set
 *		so->currPos.buf to InvalidBuffer.
 */
bool
_bt_next_ost(IndexScanDesc scan)
{
	BTScanOpaqueOST so = (BTScanOpaqueOST) scan->opaque;
	BTScanPosItemOST *currItem;

	/*
	 * Advance to next tuple on current page; or if there's no more, try to
	 * step to the next page with data.
	 */
	/* selog(DEBUG1, "lastItem is %d", so->currPos.lastItem); */
	if (++so->currPos.itemIndex > so->currPos.lastItem)
	{   
        bt_dummy_search_ost(scan->ost, scan->ost->osts->nlevels-1);
		if (!_bt_steppage_ost(scan)){

            /*
             * This case only happens when a scan has iterated over every
             * page that could statisfy a request and there are no more pages.
             * Thus the scan was at the last page and an oblivious request must 
             * be made to simullate an access to the leaf level of the tree.
             **/
            ReadDummyBuffer_ost(scan->ost, scan->ost->osts->nlevels,  0);
			return false;
        }
	}else{
      bt_dummy_search_ost(scan->ost, scan->ost->osts->nlevels);
    }


	/* OK, itemIndex says what to return */
	currItem = &so->currPos.items[so->currPos.itemIndex];
	scan->xs_ctup.t_self = currItem->heapTid;
/* 	if (scan->xs_want_itup) */
/* 		scan->xs_itup = (IndexTuple) (so->currTuples + currItem->tupleOffset); */

	return true;
}

/*
 *	_bt_readpage() -- Load data from current index page into so->currPos
 *
 * Caller must have pinned and read-locked so->currPos.buf; the buffer's state
 * is not changed here.  Also, currPos.moreLeft and moreRight must be valid;
 * they are updated as appropriate.  All other fields of so->currPos are
 * initialized from scratch here.
 *
 * We scan the current page starting at offnum and moving in the indicated
 * direction.  All items matching the scan keys are loaded into currPos.items.
 * moreLeft or moreRight (as appropriate) is cleared if _bt_checkkeys reports
 * that there can be no more matching tuples in the current scan direction.
 *
 * In the case of a parallel scan, caller must have called _bt_parallel_seize
 * prior to calling this function; this function will invoke
 * _bt_parallel_release before returning.
 *
 * Returns true if any matching items found on the page, false if none.
 */
static bool
_bt_readpage_ost(IndexScanDesc scan, OffsetNumber offnum)
{
	BTScanOpaqueOST so = (BTScanOpaqueOST) scan->opaque;
	Page		page;
	BTPageOpaqueOST opaque;
	OffsetNumber minoff;
	OffsetNumber maxoff;
	int			itemIndex;
	IndexTuple	itup;
	bool		continuescan;

	/*
	 * We must have the buffer pinned and locked, but the usual macro can't be
	 * used here; this function is what makes it good for currPos.
	 */
	/* Assert(BufferIsValid(so->currPos.buf)); */

	page = BufferGetPage_ost(scan->ost, so->currPos.buf);
	opaque = (BTPageOpaqueOST) PageGetSpecialPointer_s(page);


	minoff = P_FIRSTDATAKEY_OST(opaque);
	maxoff = PageGetMaxOffsetNumber_s(page);

	/*
	 * We note the buffer's block number so that we can release the pin later.
	 * This allows us to re-read the buffer if it is needed again for hinting.
	 */
	so->currPos.currPage = BufferGetBlockNumber_s(so->currPos.buf);


	/*
	 * we must save the page's right-link while scanning it; this tells us
	 * where to step right to after we're done with these items.  There is no
	 * corresponding need for the left-link, since splits always go right.
	 */
	so->currPos.nextPage = opaque->btpo_next;

	/* initialize tuple workspace to empty */
	so->currPos.nextTupleOffset = 0;

	/*
	 * Now that the current page has been made consistent, the macro should be
	 * good.
	 */
	/* Assert(BTScanPosIsPinned(so->currPos)); */

	/* if (ScanDirectionIsForward(dir)) */
	/* { */
	/* load items[] in ascending order */
	itemIndex = 0;

	offnum = Max_s(offnum, minoff);
	/* Only forward scans are supported on the prototype. */
	while (offnum <= maxoff)
	{
		itup = _bt_checkkeys_ost(scan, page, offnum, &continuescan);
		if (itup != NULL)
		{
			/* tuple passes all scan key conditions, so remember it */
			_bt_saveitem_ost(so, itemIndex, offnum, itup);
			itemIndex++;
		}
		if (!continuescan)
		{
			/* there can't be any more matches, so stop */
			so->currPos.moreRight = false;
			break;
		}

		offnum = OffsetNumberNext_s(offnum);
	}

	/* Assert(itemIndex <= MaxIndexTuplesPerPage); */
	so->currPos.firstItem = 0;
	so->currPos.lastItem = itemIndex - 1;
	so->currPos.itemIndex = 0;
	/* } */

	return (so->currPos.firstItem <= so->currPos.lastItem);
}

/* Save an index item into so->currPos.items[itemIndex] */
static void
_bt_saveitem_ost(BTScanOpaqueOST so, int itemIndex,
				 OffsetNumber offnum, IndexTuple itup)
{
	BTScanPosItemOST *currItem = &so->currPos.items[itemIndex];

	currItem->heapTid = itup->t_tid;
	currItem->indexOffset = offnum;
	if (so->currTuples)
	{
		Size		itupsz = IndexTupleSize_s(itup);

		currItem->tupleOffset = so->currPos.nextTupleOffset;
		memcpy(so->currTuples + so->currPos.nextTupleOffset, itup, itupsz);
		so->currPos.nextTupleOffset += MAXALIGN_s(itupsz);
	}
}

/*
 *	_bt_steppage() -- Step to next page containing valid data for scan
 *
 * On entry, if so->currPos.buf is valid the buffer is pinned but not locked;
 * if pinned, we'll drop the pin before moving to next page.  The buffer is
 * not locked on entry.
 *
 * For success on a scan using a non-MVCC snapshot we hold a pin, but not a
 * read lock, on that page.  If we do not hold the pin, we set so->currPos.buf
 * to InvalidBuffer.  We return true to indicate success.
 */
static bool
_bt_steppage_ost(IndexScanDesc scan)
{
	BTScanOpaqueOST so = (BTScanOpaqueOST) scan->opaque;
	BlockNumber blkno = InvalidBlockNumber;

/* 	bool		status = true; */

	/* Assert(BTScanPosIsValid(so->currPos)); */

	/*
	 * Before we modify currPos, make a copy of the page data if there was a
	 * mark position that needs it.
	 */
	/* if (so->markItemIndex >= 0) */

	/* Not parallel, so use the previously-saved nextPage link. */

	blkno = so->currPos.nextPage;

	/* Remember we left a page with data */
	so->currPos.moreLeft = true;

	/* release the previous buffer, if pinned */
	//BTScanPosUnpinIfPinned(so->currPos); 
	/* Relase buffer? */
	ReleaseBuffer_ost(scan->ost, so->currPos.buf);
	so->currPos.buf = InvalidBuffer;

	if (!_bt_readnextpage_ost(scan, blkno))
		return false;

	/* Drop the lock, and maybe the pin, on the current page */
	/* Release buffer? */
	/* _bt_drop_lock_and_maybe_pin(scan, &so->currPos); */
	ReleaseBuffer_ost(scan->ost, so->currPos.buf);
	so->currPos.buf= InvalidBuffer;

	return true;
}

/*
 *	_bt_readnextpage() -- Read next page containing valid data for scan
 *
 * On success exit, so->currPos is updated to contain data from the next
 * interesting page.  Caller is responsible to release lock and pin on
 * buffer on success.  We return true to indicate success.
 *
 * If there are no more matching records in the given direction, we drop all
 * locks and pins, set so->currPos.buf to InvalidBuffer, and return false.
 */
static bool
_bt_readnextpage_ost(IndexScanDesc scan, BlockNumber blkno)
{
	BTScanOpaqueOST so = (BTScanOpaqueOST) scan->opaque;
	OSTRelation rel;
	Page		page;
	BTPageOpaqueOST opaque;

/* 	bool		status = true; */

	rel = scan->ost;

	for (;;)
	{
		/*
		 * if we're at end of scan, give up and mark parallel scan as done, so
		 * that all the workers can finish their scan
		 */
		if (blkno == P_NONE_OST || !so->currPos.moreRight)
		{
			/* _bt_parallel_done(scan); */
			BTScanPosInvalidate_OST(so->currPos);
			return false;
		}
		/* check for interrupts while we're not holding any buffer lock */
		/* CHECK_FOR_INTERRUPTS(); */
		/* step right one page */
		so->currPos.buf = _bt_getbuf_ost(rel, blkno, BT_READ_OST);
		page = BufferGetPage_ost(rel, so->currPos.buf);
		/* TestForOldSnapshot(scan->xs_snapshot, rel, page); */
		opaque = (BTPageOpaqueOST) PageGetSpecialPointer_s(page);
		/* check for deleted page */
		if (!P_IGNORE_OST(opaque))
		{
			/* PredicateLockPage(rel, blkno, scan->xs_snapshot); */
			/* see if there are any matches on this page */
			/* note that this will clear moreRight if we can stop */
			if (_bt_readpage_ost(scan, P_FIRSTDATAKEY_OST(opaque)))
				break;
		}

		blkno = opaque->btpo_next;
		_bt_relbuf_ost(rel, so->currPos.buf);

	}


	return true;
}


/*
 * _bt_initialize_more_data() -- initialize moreLeft/moreRight appropriately
 * for scan direction
 */
static inline void
_bt_initialize_more_data_ost(BTScanOpaqueOST so)
{
	/* initialize moreLeft/moreRight appropriately for scan direction */
	so->currPos.moreLeft = false;
	so->currPos.moreRight = true;

	so->markItemIndex = -1;		/* ditto */
}
