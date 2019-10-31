/*-------------------------------------------------------------------------
 *
 * ost nbtpage.c
 *	  BTree-specific page management code for the Postgres btree access
 *	  method to execute ost protocol.
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/nbtree/nbtpage.c
 *
 *	NOTES
 *	   Postgres btree pages look like ordinary relation pages.  The opaque
 *	   data at high addresses includes pointers to left and right siblings
 *	   and flag data describing page state.  The first page in a btree, page
 *	   zero, is special -- it stores meta-information describing the tree.
 *	   Pages one and higher store the actual tree data.
 *
 *-------------------------------------------------------------------------
 */

#include "access/soe_ost.h"
#include "logger/logger.h"



/*
 *	_bt_getroot() -- Get the root page of the btree.
 *
 *		Since the root page can move around the btree file, we have to read
 *		its location from the metadata page, and then read the root page
 *		itself.  If no root page exists yet, we have to create one.  The
 *		standard class of race conditions exists here; I think I covered
 *		them all in the Hopi Indian rain dance of lock requests below.
 *
 *		The access type parameter (BT_READ or BT_WRITE) controls whether
 *		a new root page will be created or not.  If access = BT_READ,
 *		and no root page exists, we just return InvalidBuffer.  For
 *		BT_WRITE, we try to create the root page if it doesn't exist.
 *		NOTE that the returned root page will have only a read lock set
 *		on it even if access = BT_WRITE!
 *
 *		The returned page is not necessarily the true root --- it could be
 *		a "fast root" (a page that is alone in its level due to deletions).
 *		Also, if the root page is split while we are "in flight" to it,
 *		what we will return is the old root, which is now just the leftmost
 *		page on a probably-not-very-wide level.  For most purposes this is
 *		as good as or better than the true root, so we do not bother to
 *		insist on finding the true root.  We do, however, guarantee to
 *		return a live (not deleted or half-dead) page.
 *
 *		On successful return, the root page is pinned and read-locked.
 *		The metadata page is not locked or pinned on exit.
 */
Buffer
_bt_getroot_ost(OSTRelation rel, int access)
{

	Buffer		rootbuf;
//	Page		rootpage;
	

	/*The OST protocol assumes the root is always the first block of the index
	file.*/
	rootbuf = ReadBuffer_ost(rel, 0);
//	rootpage = BufferGetPage_ost(rel, rootbuf);

	return rootbuf;
}



/*
 *	_bt_checkpage() -- Verify that a freshly-read page looks sane.
 */
void
_bt_checkpage_ost(OSTRelation rel, Buffer buf)
{
	Page		page = BufferGetPage_ost(rel, buf);

	/*
	 * ReadBuffer verifies that every newly-read page passes
	 * PageHeaderIsValid, which means it either contains a reasonably sane
	 * page header or is all-zero.  We have to defend against the all-zero
	 * case, however.
	 */
	if (PageIsNew_s(page))
		selog(DEBUG1, "index contains unexpected zero page at block %d", buf);

	/*
	 * Additionally check that the special area looks sane.
	 */
	if (PageGetSpecialSize_s(page) != MAXALIGN_s(sizeof(BTPageOpaqueDataOST)))
		selog(DEBUG1, "index contains corrupted page at block %d", buf);
	
}


/*
 *	_bt_getbuf() -- Get a buffer by block number for read or write.
 *
 *		blkno == P_NEW means to get an unallocated index page.  The page
 *		will be initialized before returning it.
 *
 *		When this routine returns, the appropriate lock is set on the
 *		requested buffer and its reference count has been incremented
 *		(ie, the buffer is "locked and pinned").  Also, we apply
 *		_bt_checkpage to sanity-check the page (except in P_NEW case).
 */
Buffer
_bt_getbuf_ost(OSTRelation rel, BlockNumber blkno, int access)
{
	Buffer		buf;

	if (blkno != P_NEW)
	{
		/* Read an existing block of the relation */
		buf = ReadBuffer_ost(rel, blkno);
		_bt_checkpage_ost(rel, buf);
	}
	else
	{
//		bool		needLock;
//		Page		page;
		buf = ReadBuffer_ost(rel, P_NEW);
		//Initalization is done by the ReadBuffer
		/* Initialize the new page before returning it */
		//page = BufferGetPage(buf);
	}

	/* ref count and lock type are correct */
	return buf;
}


/*
 *	_bt_relbuf() -- release a locked buffer.
 *
 * Lock and pin (refcount) are both dropped.
 */
void
_bt_relbuf_ost(OSTRelation rel, Buffer buf)
{
	ReleaseBuffer_ost(rel, buf);
}