/*-------------------------------------------------------------------------
 *
 * soe_nbtinsert.c
 * Bare bones copy of tem insertion in Lehman and Yao btrees for Postgres for
 * enclave execution.
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/nbtree/nbtinsert.c
 *
 *-------------------------------------------------------------------------
 */

#include "access/soe_nbtree.h"
#include "logger/logger.h"



typedef struct
{
	/* context data for _bt_checksplitloc */
	Size		newitemsz;		/* size of new item to be inserted */
	int			fillfactor;		/* needed when splitting rightmost page */
	bool		is_leaf;		/* T if splitting a leaf page */
	bool		is_rightmost;	/* T if splitting a rightmost page */
	OffsetNumber newitemoff;	/* where the new item is to be inserted */
	int			leftspace;		/* space available for items on left page */
	int			rightspace;		/* space available for items on right page */
	int			olddataitemstotal;	/* space taken by old items */

	bool		have_split;		/* found a valid split? */

	/* these fields valid only if have_split is true */
	bool		newitemonleft;	/* new item on left or right of best split */
	OffsetNumber firstright;	/* best split point */
	int			best_delta;		/* best size delta so far */
}			FindSplitData;


static Buffer _bt_newroot_s(VRelation rel, Buffer lbuf, Buffer rbuf);

static void _bt_findinsertloc_s(VRelation rel,
								Buffer * bufptr,
								OffsetNumber * offsetptr,
								int keysz,
								ScanKey scankey,
								IndexTuple newtup,
								BTStack stack,
								VRelation heapRel);
static void _bt_insertonpg_s(VRelation rel, Buffer buf, Buffer cbuf,
							 BTStack stack,
							 IndexTuple itup,
							 OffsetNumber newitemoff,
							 bool split_only_page);
static Buffer _bt_split_s(VRelation rel, Buffer buf, Buffer cbuf,
						  OffsetNumber firstright, OffsetNumber newitemoff, Size newitemsz,
						  IndexTuple newitem, bool newitemonleft);
static void _bt_insert_parent_s(VRelation rel, Buffer buf, Buffer rbuf,
								BTStack stack, bool is_root, bool is_only);
static OffsetNumber _bt_findsplitloc_s(VRelation rel, Page page,
									   OffsetNumber newitemoff,
									   Size newitemsz,
									   bool *newitemonleft);
static void _bt_checksplitloc_s(FindSplitData * state,
								OffsetNumber firstoldonright, bool newitemonleft,
								int dataitemstoleft, Size firstoldonrightsz);
static bool _bt_pgaddtup_s(Page page, Size itemsize, IndexTuple itup,
						   OffsetNumber itup_off);

/*
 *	_bt_doinsert() -- Handle insertion of a single index tuple in the tree.
 *
 *		This routine is called by the public interface routine, btinsert.
 *		By here, itup is filled in, including the TID.
 *
 *		If checkUnique is UNIQUE_CHECK_NO or UNIQUE_CHECK_PARTIAL, this
 *		will allow duplicates.  Otherwise (UNIQUE_CHECK_YES or
 *		UNIQUE_CHECK_EXISTING) it will throw error for a duplicate.
 *		For UNIQUE_CHECK_EXISTING we merely run the duplicate check, and
 *		don't actually insert.
 *
 *		The result value is only significant for UNIQUE_CHECK_PARTIAL:
 *		it must be true if the entry is known unique, else false.
 *		(In the current implementation we'll also return true after a
 *		successful UNIQUE_CHECK_YES or UNIQUE_CHECK_EXISTING call, but
 *		that's just a coding artifact.)
 */
bool
_bt_doinsert_s(VRelation rel, IndexTuple itup, char *datum, int size, VRelation heapRel)
{
	bool		is_unique = false;
	int			indnkeyatts;
	ScanKey		itup_scankey;
	BTStack		stack = NULL;
	Buffer		buf;
	OffsetNumber offset;

	/* bool		fastpath; */

	/* We currently only support indexing a single column. */
	indnkeyatts = 1;

	/* we need an insertion scan key to do our search, so build one */
	itup_scankey = _bt_mkscankey_s(rel, itup, datum, size);

	/*
	 * It's very common to have an index on an auto-incremented or
	 * monotonically increasing value. In such cases, every insertion happens
	 * towards the end of the index. We try to optimize that case by caching
	 * the right-most leaf of the index. If our cached block is still the
	 * rightmost leaf, has enough free space to accommodate a new entry and
	 * the insertion key is strictly greater than the first key in this page,
	 * then we can safely conclude that the new key will be inserted in the
	 * cached block. So we simply search within the cached block and insert
	 * the key at the appropriate location. We call it a fastpath.
	 *
	 * Testing has revealed, though, that the fastpath can result in increased
	 * contention on the exclusive-lock on the rightmost leaf page. So we
	 * conditionally check if the lock is available. If it's not available
	 * then we simply abandon the fastpath and take the regular path. This
	 * makes sense because unavailability of the lock also signals that some
	 * other backend might be concurrently inserting into the page, thus
	 * reducing our chances to finding an insertion place in this page.
	 */
	/* fastpath = false; */
	offset = InvalidOffsetNumber;

	/* find the first page containing this key */
	stack = _bt_search_s(rel, indnkeyatts, itup_scankey, false, &buf, BT_WRITE);

	/*
	 * If the page was split between the time that we surrendered our read
	 * lock and acquired our write lock, then this page may no longer be the
	 * right place for the key we want to insert.  In this case, we need to
	 * move right in the tree.  See Lehman and Yao for an excruciatingly
	 * precise description.
	 */
	/* Concurrent splits are not supported on the prototype. */

	/* Enable duplicate keys. */

	/* do the insertion */
	_bt_findinsertloc_s(rel, &buf, &offset, indnkeyatts, itup_scankey, itup,
						stack, heapRel);

	_bt_insertonpg_s(rel, buf, InvalidBuffer, stack, itup, offset, false);



	/* be tidy */
	if (stack)
		_bt_freestack_s(stack);
	_bt_freeskey_s(itup_scankey);

	return is_unique;
}


/*
 *	_bt_findinsertloc() -- Finds an insert location for a tuple
 *
 *		If the new key is equal to one or more existing keys, we can
 *		legitimately place it anywhere in the series of equal keys --- in fact,
 *		if the new key is equal to the page's "high key" we can place it on
 *		the next page.  If it is equal to the high key, and there's not room
 *		to insert the new tuple on the current page without splitting, then
 *		we can move right hoping to find more free space and avoid a split.
 *		(We should not move right indefinitely, however, since that leads to
 *		O(N^2) insertion behavior in the presence of many equal keys.)
 *		Once we have chosen the page to put the key on, we'll insert it before
 *		any existing equal keys because of the way _bt_binsrch() works.
 *
 *		If there's not enough room in the space, we try to make room by
 *		removing any LP_DEAD tuples.
 *
 *		On entry, *bufptr and *offsetptr point to the first legal position
 *		where the new tuple could be inserted.  The caller should hold an
 *		exclusive lock on *bufptr.  *offsetptr can also be set to
 *		InvalidOffsetNumber, in which case the function will search for the
 *		right location within the page if needed.  On exit, they point to the
 *		chosen insert location.  If _bt_findinsertloc decides to move right,
 *		the lock and pin on the original page will be released and the new
 *		page returned to the caller is exclusively locked instead.
 *
 *		newtup is the new tuple we're inserting, and scankey is an insertion
 *		type scan key for it.
 */
static void
_bt_findinsertloc_s(VRelation rel,
					Buffer * bufptr,
					OffsetNumber * offsetptr,
					int keysz,
					ScanKey scankey,
					IndexTuple newtup,
					BTStack stack,
					VRelation heapRel)
{
	Buffer		buf = *bufptr;
	Page		page = BufferGetPage_s(rel, buf);
	Size		itemsz;
	BTPageOpaque lpageop;
	bool		movedright,
				vacuumed;
	OffsetNumber newitemoff;
	OffsetNumber firstlegaloff = *offsetptr;

	lpageop = (BTPageOpaque) PageGetSpecialPointer_s(page);

	itemsz = IndexTupleSize_s(newtup);
	itemsz = MAXALIGN_s(itemsz);	/* be safe, PageAddItem will do this but
									 * we need to be consistent */

	/*
	 * Check whether the item can fit on a btree page at all. (Eventually, we
	 * ought to try to apply TOAST methods if not.) We actually need to be
	 * able to fit three items on every page, so restrict any one item to 1/3
	 * the per-page available space. Note that at this point, itemsz doesn't
	 * include the ItemId.
	 *
	 * NOTE: if you change this, see also the similar code in _bt_buildadd().
	 */
	if (itemsz > BTMaxItemSize_s(page))
		selog(DEBUG1, "index row size %zu exceeds maximum %zu for index",
			  itemsz, BTMaxItemSize_s(page));

	/*----------
	 * If we will need to split the page to put the item on this page,
	 * check whether we can put the tuple somewhere to the right,
	 * instead.  Keep scanning right until we
	 *		(a) find a page with enough free space,
	 *		(b) reach the last page where the tuple can legally go, or
	 *		(c) get tired of searching.
	 * (c) is not flippant; it is important because if there are many
	 * pages' worth of equal keys, it's better to split one of the early
	 * pages than to scan all the way to the end of the run of equal keys
	 * on every insert.  We implement "get tired" as a random choice,
	 * since stopping after scanning a fixed number of pages wouldn't work
	 * well (we'd never reach the right-hand side of previously split
	 * pages).  Currently the probability of moving right is set at 0.99,
	 * which may seem too high to change the behavior much, but it does an
	 * excellent job of preventing O(N^2) behavior with many equal keys.
	 *----------
	 */
	movedright = false;
	vacuumed = false;
	while (PageGetFreeSpace_s(page) < itemsz)
	{
		Buffer		rbuf;
		BlockNumber rblkno;

		/* selog(DEBUG1, "Page has no free space, going to find next"); */

		/*
		 * before considering moving right, see if we can obtain enough space
		 * by erasing LP_DEAD items
		 */
		/* if (P_ISLEAF_s(lpageop) && P_HAS_GARBAGE(lpageop)) */

		/*
		 * nope, so check conditions (b) and (c) enumerated above
		 */
		if (P_RIGHTMOST_s(lpageop) ||
			_bt_compare_s(rel, keysz, scankey, page, P_HIKEY) != 0 ||
			getRandomInt_nb() <= (MAX_RANDOM_VALUE / 100))
			break;

		/*
		 * step right to next non-dead page
		 *
		 * must write-lock that page before releasing write lock on current
		 * page; else someone else's _bt_check_unique scan could fail to see
		 * our insertion.  write locks on intermediate dead pages won't do
		 * because we don't know when they will get de-linked from the tree.
		 */
		rbuf = InvalidBuffer;

		rblkno = lpageop->btpo_next;
		for (;;)
		{

			ReleaseBuffer_s(rel, buf);
			rbuf = ReadBuffer_s(rel, rblkno);
			page = BufferGetPage_s(rel, rbuf);
			lpageop = (BTPageOpaque) PageGetSpecialPointer_s(page);

			/*
			 * If this page was incompletely split, finish the split now. We
			 * do this while holding a lock on the left sibling, which is not
			 * good because finishing the split could be a fairly lengthy
			 * operation.  But this should happen very seldom.
			 */
			/* Prototype assumes splits are always competed until the end. */
			/* if (P_INCOMPLETE_SPLIT(lpageop)) */
			/* { */
			/* _bt_finish_split(rel, rbuf, stack); */
			/* rbuf = InvalidBuffer; */
			/* continue; */
			/* } */

			if (!P_IGNORE_s(lpageop))
				break;
			if (P_RIGHTMOST_s(lpageop))
				selog(ERROR, "fell off the end of index");

			rblkno = lpageop->btpo_next;
		}
		ReleaseBuffer_s(rel, buf);
		buf = rbuf;
		movedright = true;
		vacuumed = false;
	}

	/*
	 * Now we are on the right page, so find the insert position. If we moved
	 * right at all, we know we should insert at the start of the page. If we
	 * didn't move right, we can use the firstlegaloff hint if the caller
	 * supplied one, unless we vacuumed the page which might have moved tuples
	 * around making the hint invalid. If we didn't move right or can't use
	 * the hint, find the position by searching.
	 */
	if (movedright)
	{
		newitemoff = P_FIRSTDATAKEY_s(lpageop);
	}
	else if (firstlegaloff != InvalidOffsetNumber && !vacuumed)
	{
		newitemoff = firstlegaloff;
	}
	else
	{
		/* selog(DEBUG1, "Going to find offset to insert tuple"); */
		newitemoff = _bt_binsrch_s(rel, buf, keysz, scankey, false);
	}

	*bufptr = buf;
	*offsetptr = newitemoff;
}

/*----------
 *	_bt_insertonpg() -- Insert a tuple on a particular page in the index.
 *
 *		This recursive procedure does the following things:
 *
 *			+  if necessary, splits the target page (making sure that the
 *			   split is equitable as far as post-insert free space goes).
 *			+  inserts the tuple.
 *			+  if the page was split, pops the parent stack, and finds the
 *			   right place to insert the new child pointer (by walking
 *			   right using information stored in the parent stack).
 *			+  invokes itself with the appropriate tuple for the right
 *			   child page on the parent.
 *			+  updates the metapage if a true root or fast root is split.
 *
 *		On entry, we must have the correct buffer in which to do the
 *		insertion, and the buffer must be pinned and write-locked.  On return,
 *		we will have dropped both the pin and the lock on the buffer.
 *
 *		This routine only performs retail tuple insertions.  'itup' should
 *		always be either a non-highkey leaf item, or a downlink (new high
 *		key items are created indirectly, when a page is split).  When
 *		inserting to a non-leaf page, 'cbuf' is the left-sibling of the page
 *		we're inserting the downlink for.  This function will clear the
 *		INCOMPLETE_SPLIT flag on it, and release the buffer.
 *
 *		The locking interactions in this code are critical.  You should
 *		grok Lehman and Yao's paper before making any changes.  In addition,
 *		you need to understand how we disambiguate duplicate keys in this
 *		implementation, in order to be able to find our location using
 *		L&Y "move right" operations.
 *----------
 */
static void
_bt_insertonpg_s(VRelation rel,
				 Buffer buf,
				 Buffer cbuf,
				 BTStack stack,
				 IndexTuple itup,
				 OffsetNumber newitemoff,
				 bool split_only_page)
{
	Page		page;
	BTPageOpaque lpageop;
	OffsetNumber firstright = InvalidOffsetNumber;
	Size		itemsz;

	page = BufferGetPage_s(rel, buf);
	lpageop = (BTPageOpaque) PageGetSpecialPointer_s(page);


	/* The caller should've finished any incomplete splits already. */
	if (P_INCOMPLETE_SPLIT_s(lpageop))
		selog(ERROR, "cannot insert to incompletely split page %u", buf);

	itemsz = IndexTupleSize_s(itup);
	itemsz = MAXALIGN_s(itemsz);	/* be safe, PageAddItem will do this but
									 * we need to be consistent */

	/*
	 * Do we need to split the page to fit the item on it?
	 *
	 * Note: PageGetFreeSpace() subtracts sizeof(ItemIdData) from its result,
	 * so this comparison is correct even though we appear to be accounting
	 * only for the item and not for its line pointer.
	 */
	if (PageGetFreeSpace_s(page) < itemsz)
	{
		bool		is_root = P_ISROOT_s(lpageop);
		bool		is_only = P_LEFTMOST_s(lpageop) && P_RIGHTMOST_s(lpageop);
		bool		newitemonleft;
		Buffer		rbuf;

		/*
		 * If we're here then a pagesplit is needed. We should never reach
		 * here if we're using the fastpath since we should have checked for
		 * all the required conditions, including the fact that this page has
		 * enough freespace. Note that this routine can in theory deal with
		 * the situation where a NULL stack pointer is passed (that's what
		 * would happen if the fastpath is taken), like it does during crash
		 * recovery. But that path is much slower, defeating the very purpose
		 * of the optimization.  The following assertion should protect us
		 * from any future code changes that invalidate those assumptions.
		 *
		 * Note that whenever we fail to take the fastpath, we clear the
		 * cached block. Checking for a valid cached block at this point is
		 * enough to decide whether we're in a fastpath or not.
		 */

		/* Choose the split point */
		firstright = _bt_findsplitloc_s(rel, page,
										newitemoff, itemsz,
										&newitemonleft);


		/* split the buffer into left and right halves */
		rbuf = _bt_split_s(rel, buf, cbuf, firstright,
						   newitemoff, itemsz, itup, newitemonleft);
		/* selog(DEBUG1, "Page %d was split. Right buf is %d", buf, rbuf); */

		/*----------
		 * By here,
		 *
		 *		+  our target page has been split;
		 *		+  the original tuple has been inserted;
		 *		+  we have write locks on both the old (left half)
		 *		   and new (right half) buffers, after the split; and
		 *		+  we know the key we want to insert into the parent
		 *		   (it's the "high key" on the left child page).
		 *
		 * We're ready to do the parent insertion.  We need to hold onto the
		 * locks for the child pages until we locate the parent, but we can
		 * release them before doing the actual insertion (see Lehman and Yao
		 * for the reasoning).
		 *----------
		 */
		_bt_insert_parent_s(rel, buf, rbuf, stack, is_root, is_only);
	}
	else
	{
		Buffer		metabuf = InvalidBuffer;
		Page		metapg = NULL;
		BTMetaPageData *metad = NULL;

		/* OffsetNumber itup_off; */
		BlockNumber itup_blkno;

		/* itup_off = newitemoff; */
		itup_blkno = BufferGetBlockNumber_s(buf);

		/*
		 * If we are doing this insert because we split a page that was the
		 * only one on its tree level, but was not the root, it may have been
		 * the "fast root".  We need to ensure that the fast root link points
		 * at or above the current page.  We can safely acquire a lock on the
		 * metapage here --- see comments for _bt_newroot().
		 */
		/* if (split_only_page) */

		/*
		 * The code for the above condition has been removed because the
		 * prototype does not support fast paths.
		 */

		/*
		 * Every internal page should have exactly one negative infinity item
		 * at all times.  Only _bt_split() and _bt_newroot() should add items
		 * that become negative infinity items through truncation, since
		 * they're the only routines that allocate new internal pages.  Do not
		 * allow a retail insertion of a new item at the negative infinity
		 * offset.
		 */
		if (!P_ISLEAF_s(lpageop) && newitemoff == P_FIRSTDATAKEY_s(lpageop))
			selog(ERROR, "cannot insert second negative infinity item in block %u of index", itup_blkno);

		if (!_bt_pgaddtup_s(page, itemsz, itup, newitemoff))
			selog(ERROR, "failed to add new item to block %u in index",
				  itup_blkno);

		MarkBufferDirty_s(rel, buf);

		if (BufferIsValid_s(rel, metabuf))
		{
			/* upgrade meta-page if needed */
			if (metad->btm_version < BTREE_VERSION)
				_bt_upgrademetapage_s(metapg);
			metad->btm_fastroot = itup_blkno;
			metad->btm_fastlevel = lpageop->btpo.level;
			MarkBufferDirty_s(rel, metabuf);
		}

		/* clear INCOMPLETE_SPLIT flag on child if inserting a downlink */
		if (BufferIsValid_s(rel, cbuf))
		{
			Page		cpage = BufferGetPage_s(rel, cbuf);
			BTPageOpaque cpageop = (BTPageOpaque) PageGetSpecialPointer_s(cpage);

			cpageop->btpo_flags &= ~BTP_INCOMPLETE_SPLIT;
			MarkBufferDirty_s(rel, cbuf);
		}


		/* release buffers */
		if (BufferIsValid_s(rel, metabuf))
			ReleaseBuffer_s(rel, metabuf);
		if (BufferIsValid_s(rel, cbuf))
			ReleaseBuffer_s(rel, cbuf);
		ReleaseBuffer_s(rel, buf);
	}
}

/*
 *	_bt_split() -- split a page in the btree.
 *
 *		On entry, buf is the page to split, and is pinned and write-locked.
 *		firstright is the item index of the first item to be moved to the
 *		new right page.  newitemoff etc. tell us about the new item that
 *		must be inserted along with the data from the old page.
 *
 *		When splitting a non-leaf page, 'cbuf' is the left-sibling of the
 *		page we're inserting the downlink for.  This function will clear the
 *		INCOMPLETE_SPLIT flag on it, and release the buffer.
 *
 *		Returns the new right sibling of buf, pinned and write-locked.
 *		The pin and lock on buf are maintained.
 */
static Buffer
_bt_split_s(VRelation rel, Buffer buf, Buffer cbuf, OffsetNumber firstright,
			OffsetNumber newitemoff, Size newitemsz, IndexTuple newitem,
			bool newitemonleft)
{
	Buffer		rbuf;
	Page		origpage;
	Page		leftpage,
				rightpage;
	BlockNumber origpagenumber,
				rightpagenumber;
	BTPageOpaque ropaque,
				lopaque,
				oopaque;
	Buffer		sbuf = InvalidBuffer;
	Page		spage = NULL;
	BTPageOpaque sopaque = NULL;
	Size		itemsz;
	ItemId		itemid;
	IndexTuple	item;
	OffsetNumber leftoff,
				rightoff;
	OffsetNumber maxoff;
	OffsetNumber i;
	bool		isleaf;
	IndexTuple	lefthikey;

	/* int			indnatts = 1;//IndexRelationGetNumberOfAttributes(rel); */

	/*
	 * int			indnkeyatts =
	 * 1;//IndexRelationGetNumberOfKeyAttributes(rel);
	 */

	/* Acquire a new page to split into */
	rbuf = _bt_getbuf_s(rel, P_NEW, BT_WRITE);

	/*
	 * origpage is the original page to be split.  leftpage is a temporary
	 * buffer that receives the left-sibling data, which will be copied back
	 * into origpage on success.  rightpage is the new page that receives the
	 * right-sibling data.  If we fail before reaching the critical section,
	 * origpage hasn't been modified and leftpage is only workspace. In
	 * principle we shouldn't need to worry about rightpage either, because it
	 * hasn't been linked into the btree page structure; but to avoid leaving
	 * possibly-confusing junk behind, we are careful to rewrite rightpage as
	 * zeroes before throwing any error.
	 */
	origpage = BufferGetPage_s(rel, buf);
	leftpage = PageGetTempPage_s(origpage);
	rightpage = BufferGetPage_s(rel, rbuf);


	origpagenumber = BufferGetBlockNumber_s(buf);
	rightpagenumber = BufferGetBlockNumber_s(rbuf);

	_bt_pageinit_s(leftpage, BufferGetPageSize_s(rel, buf));
	/* rightpage was already initialized by _bt_getbuf */

	/*
	 * Copy the original page's LSN into leftpage, which will become the
	 * updated version of the page.  We need this because XLogInsert will
	 * examine the LSN and possibly dump it in a page image.
	 */
	/* PageSetLSN(leftpage, PageGetLSN(origpage)); */

	/* init btree private data */
	oopaque = (BTPageOpaque) PageGetSpecialPointer_s(origpage);
	lopaque = (BTPageOpaque) PageGetSpecialPointer_s(leftpage);
	ropaque = (BTPageOpaque) PageGetSpecialPointer_s(rightpage);

	isleaf = P_ISLEAF_s(oopaque);

	/* if we're splitting this page, it won't be the root when we're done */
	/* also, clear the SPLIT_END and HAS_GARBAGE flags in both pages */
	lopaque->btpo_flags = oopaque->btpo_flags;
	lopaque->btpo_flags &= ~(BTP_ROOT | BTP_SPLIT_END | BTP_HAS_GARBAGE);
	ropaque->btpo_flags = lopaque->btpo_flags;
	/* set flag in left page indicating that the right page has no downlink */
	lopaque->btpo_flags |= BTP_INCOMPLETE_SPLIT;
	lopaque->btpo_prev = oopaque->btpo_prev;
	lopaque->btpo_next = rightpagenumber;
	ropaque->btpo_prev = origpagenumber;
	ropaque->btpo_next = oopaque->btpo_next;
	lopaque->btpo.level = ropaque->btpo.level = oopaque->btpo.level;
	lopaque->o_blkno = oopaque->o_blkno;
	/* Since we already have write-lock on both pages, ok to read cycleid */
	/* lopaque->btpo_cycleid = _bt_vacuum_cycleid(rel); */
	/* ropaque->btpo_cycleid = lopaque->btpo_cycleid; */

	/*
	 * If the page we're splitting is not the rightmost page at its level in
	 * the tree, then the first entry on the page is the high key for the
	 * page.  We need to copy that to the right half.  Otherwise (meaning the
	 * rightmost page case), all the items on the right half will be user
	 * data.
	 */
	rightoff = P_HIKEY;

	if (!P_RIGHTMOST_s(oopaque))
	{
		itemid = PageGetItemId_s(origpage, P_HIKEY);
		itemsz = ItemIdGetLength_s(itemid);
		item = (IndexTuple) PageGetItem_s(origpage, itemid);
		if (PageAddItem_s(rightpage, (Item) item, itemsz, rightoff,
						  false, false) == InvalidOffsetNumber)
		{
			memset(rightpage, 0, BufferGetPageSize_s(rel, rbuf));
			selog(ERROR, "failed to add hikey to the right sibling"
				  " while splitting block %u of index",
				  origpagenumber);
		}
		rightoff = OffsetNumberNext_s(rightoff);
	}

	/*
	 * The "high key" for the new left page will be the first key that's going
	 * to go into the new right page.  This might be either the existing data
	 * item at position firstright, or the incoming tuple.
	 */
	leftoff = P_HIKEY;
	if (!newitemonleft && newitemoff == firstright)
	{
		/* incoming tuple will become first on right page */
		itemsz = newitemsz;
		item = newitem;
	}
	else
	{
		/* existing item at firstright will become first on right page */
		itemid = PageGetItemId_s(origpage, firstright);
		itemsz = ItemIdGetLength_s(itemid);
		item = (IndexTuple) PageGetItem_s(origpage, itemid);
	}


	lefthikey = item;

	if (PageAddItem_s(leftpage, (Item) lefthikey, itemsz, leftoff,
					  false, false) == InvalidOffsetNumber)
	{
		memset(rightpage, 0, BufferGetPageSize_s(rel, rbuf));
		selog(ERROR, "failed to add hikey to the left sibling"
			  " while splitting block %u of index", origpagenumber);
	}
	leftoff = OffsetNumberNext_s(leftoff);
	/* be tidy */
	if (lefthikey != item)
		free(lefthikey);

	/*
	 * Now transfer all the data items to the appropriate page.
	 *
	 * Note: we *must* insert at least the right page's items in item-number
	 * order, for the benefit of _bt_restore_page().
	 */
	maxoff = PageGetMaxOffsetNumber_s(origpage);

	for (i = P_FIRSTDATAKEY_s(oopaque); i <= maxoff; i = OffsetNumberNext_s(i))
	{
		itemid = PageGetItemId_s(origpage, i);
		itemsz = ItemIdGetLength_s(itemid);
		item = (IndexTuple) PageGetItem_s(origpage, itemid);

		/* does new item belong before this one? */
		if (i == newitemoff)
		{
			if (newitemonleft)
			{

				if (!_bt_pgaddtup_s(leftpage, newitemsz, newitem, leftoff))
				{
					memset(rightpage, 0, BufferGetPageSize_s(rel, rbuf));
					selog(ERROR, "failed to add new item to the left sibling"
						  " while splitting block %u", origpagenumber);
				}
				leftoff = OffsetNumberNext_s(leftoff);
			}
			else
			{
				if (!_bt_pgaddtup_s(rightpage, newitemsz, newitem, rightoff))
				{
					memset(rightpage, 0, BufferGetPageSize_s(rel, rbuf));
					selog(ERROR, "failed to add new item to the right sibling"
						  " while splitting block %u",
						  origpagenumber);
				}
				rightoff = OffsetNumberNext_s(rightoff);
			}
		}

		/* decide which page to put it on */
		if (i < firstright)
		{
			if (!_bt_pgaddtup_s(leftpage, itemsz, item, leftoff))
			{
				memset(rightpage, 0, BufferGetPageSize_s(rel, rbuf));
				selog(ERROR, "failed to add old item to the left sibling"
					  " while splitting block %u",
					  origpagenumber);
			}
			leftoff = OffsetNumberNext_s(leftoff);
		}
		else
		{
			if (!_bt_pgaddtup_s(rightpage, itemsz, item, rightoff))
			{
				memset(rightpage, 0, BufferGetPageSize_s(rel, rbuf));
				selog(ERROR, "failed to add old item to the right sibling"
					  " while splitting block %u",
					  origpagenumber);
			}
			rightoff = OffsetNumberNext_s(rightoff);
		}
	}

	/* cope with possibility that newitem goes at the end */
	if (i <= newitemoff)
	{
		/*
		 * Can't have newitemonleft here; that would imply we were told to put
		 * *everything* on the left page, which cannot fit (if it could, we'd
		 * not be splitting the page).
		 */
		/* Assert(!newitemonleft); */
		if (!_bt_pgaddtup_s(rightpage, newitemsz, newitem, rightoff))
		{
			memset(rightpage, 0, BufferGetPageSize_s(rel, rbuf));
			selog(ERROR, "failed to add new item to the right sibling"
				  " while splitting block %u",
				  origpagenumber);
		}
		rightoff = OffsetNumberNext_s(rightoff);
	}

	/*
	 * We have to grab the right sibling (if any) and fix the prev pointer
	 * there. We are guaranteed that this is deadlock-free since no other
	 * writer will be holding a lock on that page and trying to move left, and
	 * all readers release locks on a page before trying to fetch its
	 * neighbors.
	 */

	if (!P_RIGHTMOST_s(oopaque))
	{
		sbuf = _bt_getbuf_s(rel, oopaque->btpo_next, BT_WRITE);
		spage = BufferGetPage_s(rel, sbuf);
		sopaque = (BTPageOpaque) PageGetSpecialPointer_s(spage);
		if (sopaque->btpo_prev != origpagenumber)
		{
			memset(rightpage, 0, BufferGetPageSize_s(rel, rbuf));
			selog(ERROR, "right sibling's left-link doesn't match: "
				  "block %u links to %u instead of expected %u",
				  oopaque->btpo_next, sopaque->btpo_prev, origpagenumber);
		}

		/*
		 * Check to see if we can set the SPLIT_END flag in the right-hand
		 * split page; this can save some I/O for vacuum since it need not
		 * proceed to the right sibling.  We can set the flag if the right
		 * sibling has a different cycleid: that means it could not be part of
		 * a group of pages that were all split off from the same ancestor
		 * page.  If you're confused, imagine that page A splits to A B and
		 * then again, yielding A C B, while vacuum is in progress.  Tuples
		 * originally in A could now be in either B or C, hence vacuum must
		 * examine both pages.  But if D, our right sibling, has a different
		 * cycleid then it could not contain any tuples that were in A when
		 * the vacuum started.
		 */
		ropaque->btpo_flags |= BTP_SPLIT_END;
	}

	/*
	 * Right sibling is locked, new siblings are prepared, but original page
	 * is not updated yet.
	 *
	 * NO EREPORT(ERROR) till right sibling is updated.  We can get away with
	 * not starting the critical section till here because we haven't been
	 * scribbling on the original page yet; see comments above.
	 */
	/* START_CRIT_SECTION(); */

	/*
	 * By here, the original data page has been split into two new halves, and
	 * these are correct.  The algorithm requires that the left page never
	 * move during a split, so we copy the new left page back on top of the
	 * original.  Note that this is not a waste of time, since we also require
	 * (in the page management code) that the center of a page always be
	 * clean, and the most efficient way to guarantee this is just to compact
	 * the data by reinserting it into a new left page.  (XXX the latter
	 * comment is probably obsolete; but in any case it's good to not scribble
	 * on the original page until we enter the critical section.)
	 *
	 * We need to do this before writing the WAL record, so that XLogInsert
	 * can WAL log an image of the page if necessary.
	 */
	PageRestoreTempPage_s(leftpage, origpage);
	/* leftpage, lopaque must not be used below here */

	MarkBufferDirty_s(rel, buf);
	MarkBufferDirty_s(rel, rbuf);
	if (!P_RIGHTMOST_s(ropaque))
	{
		sopaque->btpo_prev = rightpagenumber;
		MarkBufferDirty_s(rel, sbuf);
	}

	/*
	 * Clear INCOMPLETE_SPLIT flag on child if inserting the new item finishes
	 * a split.
	 */
	if (!isleaf)
	{
		Page		cpage = BufferGetPage_s(rel, cbuf);
		BTPageOpaque cpageop = (BTPageOpaque) PageGetSpecialPointer_s(cpage);

		cpageop->btpo_flags &= ~BTP_INCOMPLETE_SPLIT;
		MarkBufferDirty_s(rel, cbuf);
	}

	/* release the old right sibling */
	if (!P_RIGHTMOST_s(ropaque))
		ReleaseBuffer_s(rel, sbuf);

	/* release the child */
	if (!isleaf)
		ReleaseBuffer_s(rel, cbuf);

	/* split's done */
	return rbuf;
}

/*
 *	_bt_findsplitloc() -- find an appropriate place to split a page.
 *
 * The idea here is to equalize the free space that will be on each split
 * page, *after accounting for the inserted tuple*.  (If we fail to account
 * for it, we might find ourselves with too little room on the page that
 * it needs to go into!)
 *
 * If the page is the rightmost page on its level, we instead try to arrange
 * to leave the left split page fillfactor% full.  In this way, when we are
 * inserting successively increasing keys (consider sequences, timestamps,
 * etc) we will end up with a tree whose pages are about fillfactor% full,
 * instead of the 50% full result that we'd get without this special case.
 * This is the same as nbtsort.c produces for a newly-created tree.  Note
 * that leaf and nonleaf pages use different fillfactors.
 *
 * We are passed the intended insert position of the new tuple, expressed as
 * the offsetnumber of the tuple it must go in front of.  (This could be
 * maxoff+1 if the tuple is to go at the end.)
 *
 * We return the index of the first existing tuple that should go on the
 * righthand page, plus a boolean indicating whether the new tuple goes on
 * the left or right page.  The bool is necessary to disambiguate the case
 * where firstright == newitemoff.
 */
static OffsetNumber
_bt_findsplitloc_s(VRelation rel,
				   Page page,
				   OffsetNumber newitemoff,
				   Size newitemsz,
				   bool *newitemonleft)
{
	BTPageOpaque opaque;
	OffsetNumber offnum;
	OffsetNumber maxoff;
	ItemId		itemid;
	FindSplitData state;
	int			leftspace,
				rightspace,
				goodenough,
				olddataitemstotal,
				olddataitemstoleft;
	bool		goodenoughfound;

	opaque = (BTPageOpaque) PageGetSpecialPointer_s(page);

	/* Passed-in newitemsz is MAXALIGNED but does not include line pointer */
	newitemsz += sizeof(ItemIdData);

	/* Total free space available on a btree page, after fixed overhead */
	leftspace = rightspace =
		PageGetPageSize_s(page) - SizeOfPageHeaderData -
		MAXALIGN_s(sizeof(BTPageOpaqueData));

	/* The right page will have the same high key as the old page */
	if (!P_RIGHTMOST_s(opaque))
	{
		itemid = PageGetItemId_s(page, P_HIKEY);
		rightspace -= (int) (MAXALIGN_s(ItemIdGetLength_s(itemid)) +
							 sizeof(ItemIdData));
	}

	/* Count up total space in data items without actually scanning 'em */
	olddataitemstotal = rightspace - (int) PageGetExactFreeSpace_s(page);

	state.newitemsz = newitemsz;
	state.is_leaf = P_ISLEAF_s(opaque);
	state.is_rightmost = P_RIGHTMOST_s(opaque);
	state.have_split = false;
	if (state.is_leaf)
		state.fillfactor = BTREE_DEFAULT_FILLFACTOR;
	else
		state.fillfactor = BTREE_NONLEAF_FILLFACTOR;
	state.newitemonleft = false;	/* these just to keep compiler quiet */
	state.firstright = 0;
	state.best_delta = 0;
	state.leftspace = leftspace;
	state.rightspace = rightspace;
	state.olddataitemstotal = olddataitemstotal;
	state.newitemoff = newitemoff;

	/*
	 * Finding the best possible split would require checking all the possible
	 * split points, because of the high-key and left-key special cases.
	 * That's probably more work than it's worth; instead, stop as soon as we
	 * find a "good-enough" split, where good-enough is defined as an
	 * imbalance in free space of no more than pagesize/16 (arbitrary...) This
	 * should let us stop near the middle on most pages, instead of plowing to
	 * the end.
	 */
	goodenough = leftspace / 16;

	/*
	 * Scan through the data items and calculate space usage for a split at
	 * each possible position.
	 */
	olddataitemstoleft = 0;
	goodenoughfound = false;
	maxoff = PageGetMaxOffsetNumber_s(page);

	for (offnum = P_FIRSTDATAKEY_s(opaque);
		 offnum <= maxoff;
		 offnum = OffsetNumberNext_s(offnum))
	{
		Size		itemsz;

		itemid = PageGetItemId_s(page, offnum);
		itemsz = MAXALIGN_s(ItemIdGetLength_s(itemid)) + sizeof(ItemIdData);

		/*
		 * Will the new item go to left or right of split?
		 */
		if (offnum > newitemoff)
			_bt_checksplitloc_s(&state, offnum, true,
								olddataitemstoleft, itemsz);

		else if (offnum < newitemoff)
			_bt_checksplitloc_s(&state, offnum, false,
								olddataitemstoleft, itemsz);
		else
		{
			/* need to try it both ways! */
			_bt_checksplitloc_s(&state, offnum, true,
								olddataitemstoleft, itemsz);

			_bt_checksplitloc_s(&state, offnum, false,
								olddataitemstoleft, itemsz);
		}

		/* Abort scan once we find a good-enough choice */
		if (state.have_split && state.best_delta <= goodenough)
		{
			goodenoughfound = true;
			break;
		}

		olddataitemstoleft += itemsz;
	}

	/*
	 * If the new item goes as the last item, check for splitting so that all
	 * the old items go to the left page and the new item goes to the right
	 * page.
	 */
	if (newitemoff > maxoff && !goodenoughfound)
		_bt_checksplitloc_s(&state, newitemoff, false, olddataitemstotal, 0);

	/*
	 * I believe it is not possible to fail to find a feasible split, but just
	 * in case ...
	 */
	if (!state.have_split)
		selog(ERROR, "could not find a feasible split point for index");

	*newitemonleft = state.newitemonleft;
	return state.firstright;
}

/*
 * Subroutine to analyze a particular possible split choice (ie, firstright
 * and newitemonleft settings), and record the best split so far in *state.
 *
 * firstoldonright is the offset of the first item on the original page
 * that goes to the right page, and firstoldonrightsz is the size of that
 * tuple. firstoldonright can be > max offset, which means that all the old
 * items go to the left page and only the new item goes to the right page.
 * In that case, firstoldonrightsz is not used.
 *
 * olddataitemstoleft is the total size of all old items to the left of
 * firstoldonright.
 */
static void
_bt_checksplitloc_s(FindSplitData * state,
					OffsetNumber firstoldonright,
					bool newitemonleft,
					int olddataitemstoleft,
					Size firstoldonrightsz)
{
	int			leftfree,
				rightfree;
	Size		firstrightitemsz;
	bool		newitemisfirstonright;

	/* Is the new item going to be the first item on the right page? */
	newitemisfirstonright = (firstoldonright == state->newitemoff
							 && !newitemonleft);

	if (newitemisfirstonright)
		firstrightitemsz = state->newitemsz;
	else
		firstrightitemsz = firstoldonrightsz;

	/* Account for all the old tuples */
	leftfree = state->leftspace - olddataitemstoleft;
	rightfree = state->rightspace -
		(state->olddataitemstotal - olddataitemstoleft);

	/*
	 * The first item on the right page becomes the high key of the left page;
	 * therefore it counts against left space as well as right space. When
	 * index has included attributes, then those attributes of left page high
	 * key will be truncated leaving that page with slightly more free space.
	 * However, that shouldn't affect our ability to find valid split
	 * location, because anyway split location should exists even without high
	 * key truncation.
	 */
	leftfree -= firstrightitemsz;

	/* account for the new item */
	if (newitemonleft)
		leftfree -= (int) state->newitemsz;
	else
		rightfree -= (int) state->newitemsz;

	/*
	 * If we are not on the leaf level, we will be able to discard the key
	 * data from the first item that winds up on the right page.
	 */
	if (!state->is_leaf)
		rightfree += (int) firstrightitemsz -
			(int) (MAXALIGN_s(sizeof(IndexTupleData)) + sizeof(ItemIdData));

	/*
	 * If feasible split point, remember best delta.
	 */
	if (leftfree >= 0 && rightfree >= 0)
	{
		int			delta;

		if (state->is_rightmost)
		{
			/*
			 * If splitting a rightmost page, try to put (100-fillfactor)% of
			 * free space on left page. See comments for _bt_findsplitloc.
			 */
			delta = (state->fillfactor * leftfree)
				- ((100 - state->fillfactor) * rightfree);
		}
		else
		{
			/* Otherwise, aim for equal free space on both sides */
			delta = leftfree - rightfree;
		}

		if (delta < 0)
			delta = -delta;
		if (!state->have_split || delta < state->best_delta)
		{
			state->have_split = true;
			state->newitemonleft = newitemonleft;
			state->firstright = firstoldonright;
			state->best_delta = delta;
		}
	}
}

/*
 * _bt_insert_parent() -- Insert downlink into parent after a page split.
 *
 * On entry, buf and rbuf are the left and right split pages, which we
 * still hold write locks on per the L&Y algorithm.  We release the
 * write locks once we have write lock on the parent page.  (Any sooner,
 * and it'd be possible for some other process to try to split or delete
 * one of these pages, and get confused because it cannot find the downlink.)
 *
 * stack - stack showing how we got here.  May be NULL in cases that don't
 *			have to be efficient (concurrent ROOT split, WAL recovery)
 * is_root - we split the true root
 * is_only - we split a page alone on its level (might have been fast root)
 */
static void
_bt_insert_parent_s(VRelation rel,
					Buffer buf,
					Buffer rbuf,
					BTStack stack,
					bool is_root,
					bool is_only)
{
	/*
	 * Here we have to do something Lehman and Yao don't talk about: deal with
	 * a root split and construction of a new root.  If our stack is empty
	 * then we have just split a node on what had been the root level when we
	 * descended the tree.  If it was still the root then we perform a
	 * new-root construction.  If it *wasn't* the root anymore, search to find
	 * the next higher level that someone constructed meanwhile, and find the
	 * right place to insert as for the normal case.
	 *
	 * If we have to search for the parent level, we do so by re-descending
	 * from the root.  This is not super-efficient, but it's rare enough not
	 * to matter.
	 */
	if (is_root)
	{
		Buffer		rootbuf;

		/* create a new root node and update the metapage */
		rootbuf = _bt_newroot_s(rel, buf, rbuf);
		selog(DEBUG1, "Root split. New root is %d", rootbuf);
		/* release the split buffers */
		ReleaseBuffer_s(rel, rootbuf);
		ReleaseBuffer_s(rel, rbuf);
		ReleaseBuffer_s(rel, buf);
	}
	else
	{
		BlockNumber bknum = BufferGetBlockNumber_s(buf);
		BlockNumber rbknum = BufferGetBlockNumber_s(rbuf);
		Page		page = BufferGetPage_s(rel, buf);
		IndexTuple	new_item;

/* 		BTStackData fakestack; */
		IndexTuple	ritem;
		Buffer		pbuf;

		/* get high key from left page == lower bound for new right page */
		ritem = (IndexTuple) PageGetItem_s(page,
										   PageGetItemId_s(page, P_HIKEY));

		/* form an index tuple that points at the new right page */
		new_item = CopyIndexTuple_s(ritem);
		BTreeInnerTupleSetDownLink_s(new_item, rbknum);

		/*
		 * Find the parent buffer and get the parent page.
		 *
		 * Oops - if we were moved right then we need to change stack item! We
		 * want to find parent pointing to where we are, right ?	- vadim
		 * 05/27/97
		 */
		stack->bts_btentry = bknum;
		pbuf = _bt_getstackbuf_s(rel, stack, BT_WRITE);

		/*
		 * Now we can unlock the right child. The left child will be unlocked
		 * by _bt_insertonpg().
		 */
		ReleaseBuffer_s(rel, rbuf);

		/* Check for error only after writing children */
		if (pbuf == InvalidBuffer)
			selog(ERROR, "failed to re-find parent key in index for split pages %u/%u", bknum, rbknum);

		/* Recursively update the parent */
		_bt_insertonpg_s(rel, pbuf, buf, stack->bts_parent,
						 new_item, stack->bts_offset + 1,
						 is_only);

		/* be tidy */
		free(new_item);
	}
}


/*
 *	_bt_getstackbuf() -- Walk back up the tree one step, and find the item
 *						 we last looked at in the parent.
 *
 *		This is possible because we save the downlink from the parent item,
 *		which is enough to uniquely identify it.  Insertions into the parent
 *		level could cause the item to move right; deletions could cause it
 *		to move left, but not left of the page we previously found it in.
 *
 *		Adjusts bts_blkno & bts_offset if changed.
 *
 *		Returns InvalidBuffer if item not found (should not happen).
 */
Buffer
_bt_getstackbuf_s(VRelation rel, BTStack stack, int access)
{
	BlockNumber blkno;
	OffsetNumber start;

	blkno = stack->bts_blkno;
	start = stack->bts_offset;

	for (;;)
	{
		Buffer		buf;
		Page		page;
		BTPageOpaque opaque;

		buf = _bt_getbuf_s(rel, blkno, access);
		page = BufferGetPage_s(rel, buf);
		opaque = (BTPageOpaque) PageGetSpecialPointer_s(page);

		if (access == BT_WRITE && P_INCOMPLETE_SPLIT_s(opaque))
		{
			selog(ERROR, "Concurrent splits are not supported");
			/* _bt_finish_split(rel, buf, stack->bts_parent); */
			/* continue; */
		}

		if (!P_IGNORE_s(opaque))
		{
			OffsetNumber offnum,
						minoff,
						maxoff;
			ItemId		itemid;
			IndexTuple	item;

			minoff = P_FIRSTDATAKEY_s(opaque);
			maxoff = PageGetMaxOffsetNumber_s(page);

			/*
			 * start = InvalidOffsetNumber means "search the whole page". We
			 * need this test anyway due to possibility that page has a high
			 * key now when it didn't before.
			 */
			if (start < minoff)
				start = minoff;

			/*
			 * Need this check too, to guard against possibility that page
			 * split since we visited it originally.
			 */
			if (start > maxoff)
				start = OffsetNumberNext_s(maxoff);

			/*
			 * These loops will check every item on the page --- but in an
			 * order that's attuned to the probability of where it actually
			 * is.  Scan to the right first, then to the left.
			 */
			for (offnum = start;
				 offnum <= maxoff;
				 offnum = OffsetNumberNext_s(offnum))
			{
				itemid = PageGetItemId_s(page, offnum);
				item = (IndexTuple) PageGetItem_s(page, itemid);

				if (BTreeInnerTupleGetDownLink_s(item) == stack->bts_btentry)
				{
					/* Return accurate pointer to where link is now */
					stack->bts_blkno = blkno;
					stack->bts_offset = offnum;
					return buf;
				}
			}

			for (offnum = OffsetNumberPrev_s(start);
				 offnum >= minoff;
				 offnum = OffsetNumberPrev_s(offnum))
			{
				itemid = PageGetItemId_s(page, offnum);
				item = (IndexTuple) PageGetItem_s(page, itemid);

				if (BTreeInnerTupleGetDownLink_s(item) == stack->bts_btentry)
				{
					/* Return accurate pointer to where link is now */
					stack->bts_blkno = blkno;
					stack->bts_offset = offnum;
					return buf;
				}
			}
		}

		/*
		 * The item we're looking for moved right at least one page.
		 */
		if (P_RIGHTMOST_s(opaque))
		{
			ReleaseBuffer_s(rel, buf);
			return InvalidBuffer;
		}
		blkno = opaque->btpo_next;
		start = InvalidOffsetNumber;
		ReleaseBuffer_s(rel, buf);
	}
}

/*
 *	_bt_newroot() -- Create a new root page for the index.
 *
 *		We've just split the old root page and need to create a new one.
 *		In order to do this, we add a new root page to the file, then lock
 *		the metadata page and update it.  This is guaranteed to be deadlock-
 *		free, because all readers release their locks on the metadata page
 *		before trying to lock the root, and all writers lock the root before
 *		trying to lock the metadata page.  We have a write lock on the old
 *		root page, so we have not introduced any cycles into the waits-for
 *		graph.
 *
 *		On entry, lbuf (the old root) and rbuf (its new peer) are write-
 *		locked. On exit, a new root page exists with entries for the
 *		two new children, metapage is updated and unlocked/unpinned.
 *		The new root buffer is returned to caller which has to unlock/unpin
 *		lbuf, rbuf & rootbuf.
 */
static Buffer
_bt_newroot_s(VRelation rel, Buffer lbuf, Buffer rbuf)
{
	Buffer		rootbuf;
	Page		lpage,
				rootpage;
	BlockNumber lbkno,
				rbkno;
	BlockNumber rootblknum;
	BTPageOpaque rootopaque;
	BTPageOpaque lopaque;
	ItemId		itemid;
	IndexTuple	item;
	IndexTuple	left_item;
	Size		left_item_sz;
	IndexTuple	right_item;
	Size		right_item_sz;
	Buffer		metabuf;
	Page		metapg;
	BTMetaPageData *metad;

	/* BTPageOpaque ropaquea, */
	/* lopaquea, */
	/* mopaquea; */
	/* selog(DEBUG1, "creating new root"); */
	lbkno = BufferGetBlockNumber_s(lbuf);
	rbkno = BufferGetBlockNumber_s(rbuf);
	lpage = BufferGetPage_s(rel, lbuf);
	lopaque = (BTPageOpaque) PageGetSpecialPointer_s(lpage);

	/* get a new root page */
	rootbuf = _bt_getbuf_s(rel, P_NEW, BT_WRITE);
	rootpage = BufferGetPage_s(rel, rootbuf);
	rootblknum = BufferGetBlockNumber_s(rootbuf);
	selog(DEBUG1, "New root buf is %d and has rootblknum %d", rootbuf, rootblknum);

	/* acquire lock on the metapage */
	metabuf = _bt_getbuf_s(rel, BTREE_METAPAGE, BT_WRITE);
	metapg = BufferGetPage_s(rel, metabuf);
	metad = BTPageGetMeta_s(metapg);
/* 	selog(DEBUG1, "Acquired metabuf %d", metabuf); */

	/*
	 * Create downlink item for left page (old root).  Since this will be the
	 * first item in a non-leaf page, it implicitly has minus-infinity key
	 * value, so we need not store any actual key in it.
	 */
	left_item_sz = sizeof(IndexTupleData);
	left_item = (IndexTuple) malloc(left_item_sz);
	left_item->t_info = left_item_sz;
	BTreeInnerTupleSetDownLink_s(left_item, lbkno);
	BTreeTupleSetNAtts_s(left_item, 0);

	/*
	 * Create downlink item for right page.  The key for it is obtained from
	 * the "high key" position in the left page.
	 */
	itemid = PageGetItemId_s(lpage, P_HIKEY);
	right_item_sz = ItemIdGetLength_s(itemid);
	item = (IndexTuple) PageGetItem_s(lpage, itemid);
	right_item = CopyIndexTuple_s(item);
	BTreeInnerTupleSetDownLink_s(right_item, rbkno);
	/* selog(DEBUG1, "Right item is going to point to %d", rbkno); */

	/* NO EREPORT(ERROR) from here till newroot op is logged */
	/* START_CRIT_SECTION(); */

	/* upgrade metapage if needed */
	/* if (metad->btm_version < BTREE_VERSION){ */
/* 		selog(DEBUG1, "Update meta page"); */
/* 		_bt_upgrademetapage_s(metapg); */
/* 	} */

	/* set btree special data */
	rootopaque = (BTPageOpaque) PageGetSpecialPointer_s(rootpage);
	/* selog(DEBUG1, "rootpage has special pointer %d", rootopaque->o_blkno); */
	rootopaque->btpo_prev = rootopaque->btpo_next = P_NONE;
	rootopaque->btpo_flags = BTP_ROOT;
	rootopaque->btpo.level =
		((BTPageOpaque) PageGetSpecialPointer_s(lpage))->btpo.level + 1;
	/* rootopaque->o_blkno = rootbuf; */
	/* selog(DEBUG1, "rootpage level is %d", rootopaque->btpo.level); */
	/* update metapage data */
	metad->btm_root = rootblknum;
	metad->btm_level = rootopaque->btpo.level;
	metad->btm_fastroot = rootblknum;
	metad->btm_fastlevel = rootopaque->btpo.level;

	/*
	 * selog(DEBUG1, "Metapage current root is %d and level is
	 * %d",metad->btm_root,metad->btm_level);
	 */

	/*
	 * Insert the left page pointer into the new root page.  The root page is
	 * the rightmost page on its level so there is no "high key" in it; the
	 * two items will go into positions P_HIKEY and P_FIRSTKEY.
	 *
	 * Note: we *must* insert the two items in item-number order, for the
	 * benefit of _bt_restore_page().
	 */
	/* Assert(BTreeTupleGetNAtts(left_item, rel) == 0); */
	if (PageAddItem_s(rootpage, (Item) left_item, left_item_sz, P_HIKEY,
					  false, false) == InvalidOffsetNumber)
		selog(ERROR, "failed to add leftkey to new root page"
			  " while splitting block %u",
			  BufferGetBlockNumber_s(lbuf));

	/*
	 * insert the right page pointer into the new root page.
	 */
	/* Assert(BTreeTupleGetNAtts(right_item, rel) == */
	/* IndexRelationGetNumberOfKeyAttributes(rel)); */
	if (PageAddItem_s(rootpage, (Item) right_item, right_item_sz, P_FIRSTKEY,
					  false, false) == InvalidOffsetNumber)
		selog(ERROR, "failed to add rightkey to new root page"
			  " while splitting block %u",
			  BufferGetBlockNumber_s(lbuf));

	/*
	 * selog(DEBUG1, "Going to write buffers to disk %d, %d, %d", lbuf,
	 * rootbuf, metabuf);
	 */
	/* lopaquea =  (BTPageOpaque) PageGetSpecialPointer_s(lpage); */
	/* ropaquea =  (BTPageOpaque) PageGetSpecialPointer_s(rootpage); */
	/* mopaquea =  (BTPageOpaque) PageGetSpecialPointer_s(metapg); */

	/*
	 * selog(DEBUG1, "buffer special pointers are %d, %d, %d",
	 * lopaquea->o_blkno, ropaquea->o_blkno, mopaquea->o_blkno);
	 */

	/* Clear the incomplete-split flag in the left child */
	/* Assert(P_INCOMPLETE_SPLIT(lopaque)); */
	lopaque->btpo_flags &= ~BTP_INCOMPLETE_SPLIT;
	MarkBufferDirty_s(rel, lbuf);
	MarkBufferDirty_s(rel, rootbuf);
	MarkBufferDirty_s(rel, metabuf);

	/* done with metapage */
	ReleaseBuffer_s(rel, metabuf);

	free(left_item);
	free(right_item);

	return rootbuf;
}

/*
 *	_bt_pgaddtup() -- add a tuple to a particular page in the index.
 *
 *		This routine adds the tuple to the page as requested.  It does
 *		not affect pin/lock status, but you'd better have a write lock
 *		and pin on the target buffer!  Don't forget to write and release
 *		the buffer afterwards, either.
 *
 *		The main difference between this routine and a bare PageAddItem call
 *		is that this code knows that the leftmost index tuple on a non-leaf
 *		btree page doesn't need to have a key.  Therefore, it strips such
 *		tuples down to just the tuple header.  CAUTION: this works ONLY if
 *		we insert the tuples in order, so that the given itup_off does
 *		represent the final position of the tuple!
 */
static bool
_bt_pgaddtup_s(Page page,
			   Size itemsize,
			   IndexTuple itup,
			   OffsetNumber itup_off)
{
	BTPageOpaque opaque = (BTPageOpaque) PageGetSpecialPointer_s(page);
	IndexTupleData trunctuple;

	if (!P_ISLEAF_s(opaque) && itup_off == P_FIRSTDATAKEY_s(opaque))
	{
		trunctuple = *itup;
		trunctuple.t_info = sizeof(IndexTupleData);
		BTreeTupleSetNAtts_s(&trunctuple, 0);
		itup = &trunctuple;
		itemsize = sizeof(IndexTupleData);
	}

	if (PageAddItem_s(page, (Item) itup, itemsize, itup_off,
					  false, false) == InvalidOffsetNumber)
		return false;

	return true;
}
