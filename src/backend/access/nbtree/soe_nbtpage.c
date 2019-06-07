/*-------------------------------------------------------------------------
 *
 * nbtpage.c
 *	  BTree-specific page management code for the Postgres btree access
 *	  method.
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

#include "access/soe_nbtree.h"
#include "logger/logger.h"

/*
 *	_bt_initmetapage() -- Fill a page buffer with a correct metapage image
 */
void
_bt_initmetapage_s(VRelation rel, BlockNumber rootbknum, uint32 level)
{

	Buffer metabuf;
	Page page;
	BTMetaPageData *metad;
	BTPageOpaque metaopaque;

	//First time creating the meta page
	metabuf = _bt_getbuf_s(rel, P_NEW, BT_WRITE);
	page = BufferGetPage_s(rel, metabuf);

	//selog(DEBUG1, "Metabuf buffer is %d and rootbknum is %d", metabuf, rootbknum);
	_bt_pageinit_s(page, BLCKSZ);

	metad = BTPageGetMeta_s(page);
	metad->btm_magic = BTREE_MAGIC;
	metad->btm_version = BTREE_VERSION;
	metad->btm_root = rootbknum;
	metad->btm_level = level;
	metad->btm_fastroot = rootbknum;
	metad->btm_fastlevel = level;
	metad->btm_last_cleanup_num_heap_tuples = -1.0;

	metaopaque = (BTPageOpaque) PageGetSpecialPointer_s(page);
	metaopaque->btpo_flags = BTP_META;

	/*
	 * Set pd_lower just past the end of the metadata.  This is essential,
	 * because without doing so, metadata will be lost if xlog.c compresses
	 * the page.
	 */
	((PageHeader) page)->pd_lower =
		((char *) metad + sizeof(BTMetaPageData)) - (char *) page;

	MarkBufferDirty_s(rel, metabuf);
	ReleaseBuffer_s(rel, metabuf);
}

/*
 *	_bt_upgrademetapage() -- Upgrade a meta-page from an old format to the new.
 *
 *		This routine does purely in-memory image upgrade.  Caller is
 *		responsible for locking, WAL-logging etc.
 */
void
_bt_upgrademetapage_s(Page page)
{
	BTMetaPageData *metad;
	//BTPageOpaque metaopaque;

	metad = BTPageGetMeta_s(page);
	//metaopaque = (BTPageOpaque) PageGetSpecialPointer_s(page);

	/* It must be really a meta page of upgradable version */

	/* Set version number and fill extra fields added into version 3 */
	metad->btm_version = BTREE_VERSION;
	metad->btm_last_cleanup_num_heap_tuples = -1.0;

	/* Adjust pd_lower (see _bt_initmetapage() for details) */
	((PageHeader) page)->pd_lower =
		((char *) metad + sizeof(BTMetaPageData)) - (char *) page;
}


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
_bt_getroot_s(VRelation rel, int access)
{
	Buffer		metabuf;
	Page		metapg;
	BTPageOpaque metaopaque;
	Buffer		rootbuf;
	Page		rootpage;
	BTPageOpaque rootopaque;
	BlockNumber rootblkno;
	uint32		rootlevel;
	BTMetaPageData *metad;


	metabuf = _bt_getbuf_s(rel, BTREE_METAPAGE, BT_READ);
	metapg = BufferGetPage_s(rel, metabuf);
	metaopaque = (BTPageOpaque) PageGetSpecialPointer_s(metapg);
	metad = BTPageGetMeta_s(metapg);
	//selog(DEBUG1, "Metabuffer page is %d", metabuf);
	/* sanity-check the metapage */
	if (!P_ISMETA_s(metaopaque) ||
		metad->btm_magic != BTREE_MAGIC)
		selog(ERROR, "index is not a btree");

	if (metad->btm_version < BTREE_MIN_VERSION ||
		metad->btm_version > BTREE_VERSION)
		selog(ERROR, "version mismatch in index: file version %d, "
						"current version %d, minimal supported version %d",
						metad->btm_version, BTREE_VERSION, BTREE_MIN_VERSION
						 );

	/* if no root page initialized yet, do it */
	if (metad->btm_root == P_NONE)
	{
		//selog(DEBUG1, "going to initialize root page");
		/* If access = BT_READ, caller doesn't want us to create root yet */
		if (access == BT_READ)
		{
			ReleaseBuffer_s(rel, metabuf);
			return InvalidBuffer;
		}

		/*
		 * Get, initialize, write, and leave a lock of the appropriate type on
		 * the new root page.  Since this is the first page in the tree, it's
		 * a leaf as well as the root.
		 */
		rootbuf = _bt_getbuf_s(rel, P_NEW, BT_WRITE);
		rootblkno = BufferGetBlockNumber_s(rootbuf);
		rootpage = BufferGetPage_s(rel,rootbuf);
		rootopaque = (BTPageOpaque) PageGetSpecialPointer_s(rootpage);
		rootopaque->btpo_prev = rootopaque->btpo_next = P_NONE;
		rootopaque->btpo_flags = (BTP_LEAF | BTP_ROOT);
		rootopaque->btpo.level = 0;

		/* NO ELOG(ERROR) till meta is updated */

		/* upgrade metapage if needed */
		metad->btm_root = rootblkno;
		metad->btm_level = 0;
		metad->btm_fastroot = rootblkno;
		metad->btm_fastlevel = 0;
		//metad->btm_last_cleanup_num_heap_tuples = -1.0;
		MarkBufferDirty_s(rel, rootbuf);
		MarkBufferDirty_s(rel, metabuf);

		/* okay, metadata is correct, release lock on it */
		ReleaseBuffer_s(rel, metabuf);
	}
	else
	{

		rootblkno = metad->btm_fastroot;
		rootlevel = metad->btm_fastlevel;

		/*
		 * We are done with the metapage; arrange to release it via first
		 * _bt_relandgetbuf call
		 */
		rootbuf = metabuf;
		ReleaseBuffer_s(rel, rootbuf);
		rootbuf = ReadBuffer_s(rel, rootblkno);
		rootpage = BufferGetPage_s(rel, rootbuf);
		rootopaque = (BTPageOpaque) PageGetSpecialPointer_s(rootpage);

		/* Note: can't check btpo.level on deleted pages */
		if (rootopaque->btpo.level != rootlevel)
			selog(ERROR, "root page %u of index has level %u, expected %u",
				 rootblkno, rootopaque->btpo.level, rootlevel);
	}

	/*
	 * By here, we have a pin and read lock on the root page, and no lock set
	 * on the metadata page.  Return the root page's buffer.
	 */
	return rootbuf;
}


/*
 *	_bt_getrootheight() -- Get the height of the btree search tree.
 *
 *		We return the level (counting from zero) of the current fast root.
 *		This represents the number of tree levels we'd have to descend through
 *		to start any btree index search.
 *
 *		This is used by the planner for cost-estimation purposes.  Since it's
 *		only an estimate, slightly-stale data is fine, hence we don't worry
 *		about updating previously cached data.
 */
int
_bt_getrootheight_s(VRelation rel)
{
	BTMetaPageData *metad;

	/*
	 * We can get what we need from the cached metapage data.  If it's not
	 * cached yet, load it.  Sanity checks here must match _bt_getroot().
	 */
	if (rel->rd_amcache == NULL)
	{
		Buffer		metabuf;
		Page		metapg;
		BTPageOpaque metaopaque;

		metabuf = _bt_getbuf_s(rel, BTREE_METAPAGE, BT_READ);
		metapg = BufferGetPage_s(rel, metabuf);
		metaopaque = (BTPageOpaque) PageGetSpecialPointer_s(metapg);
		metad = BTPageGetMeta_s(metapg);

		/* sanity-check the metapage */
		if (!P_ISMETA_s(metaopaque) ||
			metad->btm_magic != BTREE_MAGIC)
			selog(ERROR,"indexis not a btree" );

		if (metad->btm_version < BTREE_MIN_VERSION ||
			metad->btm_version > BTREE_VERSION)
			selog(ERROR, "version mismatch in index");
		/*
		 * If there's no root page yet, _bt_getroot() doesn't expect a cache
		 * to be made, so just stop here and report the index height is zero.
		 * (XXX perhaps _bt_getroot() should be changed to allow this case.)
		 */
		if (metad->btm_root == P_NONE)
		{
			ReleaseBuffer_s(rel, metabuf);
			return 0;
		}

		ReleaseBuffer_s(rel, metabuf);
	}

	metad = (BTMetaPageData *) rel->rd_amcache;
	/* We shouldn't have cached it if any of these fail */
	return metad->btm_fastlevel;
}

/*
 *	_bt_checkpage() -- Verify that a freshly-read page looks sane.
 */
void
_bt_checkpage_s(VRelation rel, Buffer buf)
{
	Page		page = BufferGetPage_s(rel, buf);

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
	if (PageGetSpecialSize_s(page) != MAXALIGN_s(sizeof(BTPageOpaqueData)))
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
_bt_getbuf_s(VRelation rel, BlockNumber blkno, int access)
{
	Buffer		buf;

	if (blkno != P_NEW)
	{
		/* Read an existing block of the relation */
		buf = ReadBuffer_s(rel, blkno);
		_bt_checkpage_s(rel, buf);
	}
	else
	{
//		bool		needLock;
//		Page		page;
		buf = ReadBuffer_s(rel, P_NEW);
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
_bt_relbuf_s(VRelation rel, Buffer buf)
{
	ReleaseBuffer_s(rel, buf);
}

/*
 *	_bt_pageinit() -- Initialize a new page.
 *
 * On return, the page header is initialized; data space is empty;
 * special space is zeroed out.
 */
void
_bt_pageinit_s(Page page, Size size)
{
	PageInit_s(page, size, sizeof(BTPageOpaqueData));
}