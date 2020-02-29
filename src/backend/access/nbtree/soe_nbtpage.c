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

int* sfanouts;
unsigned int snlevels;

extern void btree_fanout_setup(int* fanouts,unsigned int fanout_size, 
                               unsigned int nlevels){
    
    sfanouts = (int*)malloc(fanout_size);
    memcpy(sfanouts, fanouts, fanout_size);
    snlevels = nlevels;
}

extern void free_btree_fanout(){
    free(sfanouts);    
}



/*
 *	_bt_initmetapage() -- Fill a page buffer with a correct metapage image
 */
void
_bt_initmetapage_s(VRelation rel, BlockNumber rootbknum, uint32 level)
{

	Buffer		metabuf;
	Page		page;
	BTMetaPageData *metad;
	BTPageOpaque metaopaque;

	/* First time creating the meta page */
	metabuf = _bt_getbuf_s(rel, P_NEW, BT_WRITE);
	page = BufferGetPage_s(rel, metabuf);

	/*
	 * selog(DEBUG1, "Metabuf buffer is %d and rootbknum is %d", metabuf,
	 * rootbknum);
	 */
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

	/* BTPageOpaque metaopaque; */

	metad = BTPageGetMeta_s(page);
	/* metaopaque = (BTPageOpaque) PageGetSpecialPointer_s(page); */

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
    Buffer rootbuf;

    rootbuf = ReadBuffer_s(rel,0);

    return rootbuf;
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
		buf = ReadBuffer_s(rel, P_NEW);
	}

	/* ref count and lock type are correct */
	return buf;
}

Buffer
_bt_getbuf_level_s(VRelation rel, BlockNumber blkno)
{

    unsigned int clevel = rel->level;
    unsigned int l_offset;
    unsigned int l_ob_blkno;
    Buffer  buf;

    if(clevel == 0){
        l_ob_blkno = blkno;

    }else{

        l_offset = 1;

        for(int i=0; i < clevel-1; i++){
            l_offset += sfanouts[i];
        }
    
        l_ob_blkno = l_offset + blkno;
    }
    
    buf = ReadBuffer_s(rel, l_ob_blkno);

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
