/*-------------------------------------------------------------------------
 *
 * soe_nbtree.h
 * Bare bones copy of header file for postgres btree access method implementation for postgres execution.
 *
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/access/nbtree.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef SOE_NBTREE_H
#define SOE_NBTREE_H

#include "access/soe_itup.h"
#include "access/soe_relscan.h"
#include "storage/soe_block.h"
#include "storage/soe_bufmgr.h"
#include "storage/soe_item.h"


/* There's room for a 16-bit vacuum cycle ID in BTPageOpaqueData */
typedef uint16 BTCycleId;

/*
 *	BTPageOpaqueData -- At the end of every page, we store a pointer
 *	to both siblings in the tree.  This is used to do forward/backward
 *	index scans.  The next-page link is also critical for recovery when
 *	a search has navigated to the wrong page due to concurrent page splits
 *	or deletions; see src/backend/access/nbtree/README for more info.
 *
 *	In addition, we store the page's btree level (counting upwards from
 *	zero at a leaf page) as well as some flag bits indicating the page type
 *	and status.  If the page is deleted, we replace the level with the
 *	next-transaction-ID value indicating when it is safe to reclaim the page.
 *
 *	We also store a "vacuum cycle ID".  When a page is split while VACUUM is
 *	processing the index, a nonzero value associated with the VACUUM run is
 *	stored into both halves of the split page.  (If VACUUM is not running,
 *	both pages receive zero cycleids.)	This allows VACUUM to detect whether
 *	a page was split since it started, with a small probability of false match
 *	if the page was last split some exact multiple of MAX_BT_CYCLE_ID VACUUMs
 *	ago.  Also, during a split, the BTP_SPLIT_END flag is cleared in the left
 *	(original) page, and set in the right page, but only if the next page
 *	to its right has a different cycleid.
 *
 *	NOTE: the BTP_LEAF flag bit is redundant since level==0 could be tested
 *	instead.
 */

typedef struct BTPageOpaqueData
{
	BlockNumber btpo_prev;		/* left sibling, or P_NONE if leftmost */
	BlockNumber btpo_next;		/* right sibling, or P_NONE if rightmost */
	union
	{
		uint32		level;		/* tree level --- zero for leaf pages */
	}			btpo;
	uint16		btpo_flags;		/* flag bits, see below */
	/* vacuum cycle ID of latest split */
	/* BTCycleId	btpo_cycleid;*/		
 
    /* block number */
    int o_blkno;
}			BTPageOpaqueData;

typedef BTPageOpaqueData * BTPageOpaque;

/* Bits defined in btpo_flags */
#define BTP_LEAF		(1 << 0)	/* leaf page, i.e. not internal page */
#define BTP_ROOT		(1 << 1)	/* root page (has no parent) */
#define BTP_DELETED		(1 << 2)	/* page has been deleted from tree */
#define BTP_META		(1 << 3)	/* meta-page */
#define BTP_HALF_DEAD	(1 << 4)	/* empty, but still in tree */
#define BTP_SPLIT_END	(1 << 5)	/* rightmost page of split group */
#define BTP_HAS_GARBAGE (1 << 6)	/* page has LP_DEAD tuples */
#define BTP_INCOMPLETE_SPLIT (1 << 7)	/* right sibling's downlink is missing */

/*
 * The max allowed value of a cycle ID is a bit less than 64K.  This is
 * for convenience of pg_filedump and similar utilities: we want to use
 * the last 2 bytes of special space as an index type indicator, and
 * restricting cycle ID lets btree use that space for vacuum cycle IDs
 * while still allowing index type to be identified.
 */
#define MAX_BT_CYCLE_ID		0xFF7F


/*
 * The Meta page is always the first page in the btree index.
 * Its primary purpose is to point to the location of the btree root page.
 * We also point to the "fast" root, which is the current effective root;
 * see README for discussion.
 */

typedef struct BTMetaPageData
{
	uint32		btm_magic;		/* should contain BTREE_MAGIC */
	uint32		btm_version;	/* should contain BTREE_VERSION */
	BlockNumber btm_root;		/* current root location */
	uint32		btm_level;		/* tree level of the root page */
	BlockNumber btm_fastroot;	/* current "fast" root location */
	uint32		btm_fastlevel;	/* tree level of the "fast" root page */
	/* following fields are available since page version 3 */
	float8		btm_last_cleanup_num_heap_tuples;	/* number of heap tuples
													 * during last cleanup */
}			BTMetaPageData;

#define BTPageGetMeta_s(p) \
	((BTMetaPageData *) PageGetContents_s(p))

#define BTREE_METAPAGE	0		/* first page is meta */
#define BTREE_MAGIC		0x053162	/* magic number of btree pages */
#define BTREE_VERSION	3		/* current version number */
#define BTREE_MIN_VERSION	2	/* minimal supported version number */

/*
 * Maximum size of a btree index entry, including its tuple header.
 *
 * We actually need to be able to fit three items on every page,
 * so restrict any one item to 1/3 the per-page available space.
 */
#define BTMaxItemSize_s(page) \
	MAXALIGN_DOWN_s((PageGetPageSize_s(page) - \
				   MAXALIGN_s(SizeOfPageHeaderData + 3*sizeof(ItemIdData)) - \
				   MAXALIGN_s(sizeof(BTPageOpaqueData))) / 3)

/*
 * The leaf-page fillfactor defaults to 90% but is user-adjustable.
 * For pages above the leaf level, we use a fixed 70% fillfactor.
 * The fillfactor is applied during index build and when splitting
 * a rightmost page; when splitting non-rightmost pages we try to
 * divide the data equally.
 */
#define BTREE_MIN_FILLFACTOR		10
#define BTREE_DEFAULT_FILLFACTOR	90
#define BTREE_NONLEAF_FILLFACTOR	70

/*
 *	In general, the btree code tries to localize its knowledge about
 *	page layout to a couple of routines.  However, we need a special
 *	value to indicate "no page number" in those places where we expect
 *	page numbers.  We can use zero for this because we never need to
 *	make a pointer to the metadata page.
 */

#define P_NONE			0

/*
 * Macros to test whether a page is leftmost or rightmost on its tree level,
 * as well as other state info kept in the opaque data.
 */
#define P_LEFTMOST_s(opaque)		((opaque)->btpo_prev == P_NONE)
#define P_RIGHTMOST_s(opaque)		((opaque)->btpo_next == P_NONE)
#define P_ISLEAF_s(opaque)		(((opaque)->btpo_flags & BTP_LEAF) != 0)
#define P_ISROOT_s(opaque)		(((opaque)->btpo_flags & BTP_ROOT) != 0)
#define P_ISMETA_s(opaque)		(((opaque)->btpo_flags & BTP_META) != 0)
#define P_IGNORE_s(opaque)		(((opaque)->btpo_flags & (BTP_DELETED|BTP_HALF_DEAD)) != 0)
#define P_INCOMPLETE_SPLIT_s(opaque)	(((opaque)->btpo_flags & BTP_INCOMPLETE_SPLIT) != 0)

/*
 *	Lehman and Yao's algorithm requires a ``high key'' on every non-rightmost
 *	page.  The high key is not a data key, but gives info about what range of
 *	keys is supposed to be on this page.  The high key on a page is required
 *	to be greater than or equal to any data key that appears on the page.
 *	If we find ourselves trying to insert a key > high key, we know we need
 *	to move right (this should only happen if the page was split since we
 *	examined the parent page).
 *
 *	Our insertion algorithm guarantees that we can use the initial least key
 *	on our right sibling as the high key.  Once a page is created, its high
 *	key changes only if the page is split.
 *
 *	On a non-rightmost page, the high key lives in item 1 and data items
 *	start in item 2.  Rightmost pages have no high key, so we store data
 *	items beginning in item 1.
 */

#define P_HIKEY				((OffsetNumber) 1)
#define P_FIRSTKEY			((OffsetNumber) 2)
#define P_FIRSTDATAKEY_s(opaque)	(P_RIGHTMOST_s(opaque) ? P_HIKEY : P_FIRSTKEY)

/*
 * INCLUDE B-Tree indexes have non-key attributes.  These are extra
 * attributes that may be returned by index-only scans, but do not influence
 * the order of items in the index (formally, non-key attributes are not
 * considered to be part of the key space).  Non-key attributes are only
 * present in leaf index tuples whose item pointers actually point to heap
 * tuples.  All other types of index tuples (collectively, "pivot" tuples)
 * only have key attributes, since pivot tuples only ever need to represent
 * how the key space is separated.  In general, any B-Tree index that has
 * more than one level (i.e. any index that does not just consist of a
 * metapage and a single leaf root page) must have some number of pivot
 * tuples, since pivot tuples are used for traversing the tree.
 *
 * We store the number of attributes present inside pivot tuples by abusing
 * their item pointer offset field, since pivot tuples never need to store a
 * real offset (downlinks only need to store a block number).  The offset
 * field only stores the number of attributes when the INDEX_ALT_TID_MASK
 * bit is set (we never assume that pivot tuples must explicitly store the
 * number of attributes, and currently do not bother storing the number of
 * attributes unless indnkeyatts actually differs from indnatts).
 * INDEX_ALT_TID_MASK is only used for pivot tuples at present, though it's
 * possible that it will be used within non-pivot tuples in the future.  Do
 * not assume that a tuple with INDEX_ALT_TID_MASK set must be a pivot
 * tuple.
 *
 * The 12 least significant offset bits are used to represent the number of
 * attributes in INDEX_ALT_TID_MASK tuples, leaving 4 bits that are reserved
 * for future use (BT_RESERVED_OFFSET_MASK bits). BT_N_KEYS_OFFSET_MASK should
 * be large enough to store any number <= INDEX_MAX_KEYS.
 */
#define INDEX_ALT_TID_MASK			INDEX_AM_RESERVED_BIT
#define BT_RESERVED_OFFSET_MASK		0xF000
#define BT_N_KEYS_OFFSET_MASK_s		0x0FFF

/* Get/set downlink block number */
#define BTreeInnerTupleGetDownLink_s(itup) \
	ItemPointerGetBlockNumberNoCheck_s(&((itup)->t_tid))
#define BTreeInnerTupleSetDownLink_s(itup, blkno) \
	ItemPointerSetBlockNumber_s(&((itup)->t_tid), (blkno))


/*
 *	We need to be able to tell the difference between read and write
 *	requests for pages, in order to do locking correctly.
 */

#define BT_READ			BUFFER_LOCK_SHARE
#define BT_WRITE		BUFFER_LOCK_EXCLUSIVE

/*
 *	BTStackData -- As we descend a tree, we push the (location, downlink)
 *	pairs from internal pages onto a private stack.  If we split a
 *	leaf, we use this stack to walk back up the tree and insert data
 *	into parent pages (and possibly to split them, too).  Lehman and
 *	Yao's update algorithm guarantees that under no circumstances can
 *	our private stack give us an irredeemably bad picture up the tree.
 *	Again, see the paper for details.
 */

typedef struct BTStackData
{
	BlockNumber bts_blkno;
	OffsetNumber bts_offset;
	BlockNumber bts_btentry;
	struct BTStackData *bts_parent;
}			BTStackData;

typedef BTStackData * BTStack;

/*
 * BTScanOpaqueData is the btree-private state needed for an indexscan.
 * This consists of preprocessed scan keys (see _bt_preprocess_keys() for
 * details of the preprocessing), information about the current location
 * of the scan, and information about the marked location, if any.  (We use
 * BTScanPosData to represent the data needed for each of current and marked
 * locations.)	In addition we can remember some known-killed index entries
 * that must be marked before we can move off the current page.
 *
 * Index scans work a page at a time: we pin and read-lock the page, identify
 * all the matching items on the page and save them in BTScanPosData, then
 * release the read-lock while returning the items to the caller for
 * processing.  This approach minimizes lock/unlock traffic.  Note that we
 * keep the pin on the index page until the caller is done with all the items
 * (this is needed for VACUUM synchronization, see nbtree/README).  When we
 * are ready to step to the next page, if the caller has told us any of the
 * items were killed, we re-lock the page to mark them killed, then unlock.
 * Finally we drop the pin and step to the next page in the appropriate
 * direction.
 *
 * If we are doing an index-only scan, we save the entire IndexTuple for each
 * matched item, otherwise only its heap TID and offset.  The IndexTuples go
 * into a separate workspace array; each BTScanPosItem stores its tuple's
 * offset within that array.
 */

typedef struct BTScanPosItem	/* what we remember about each match */
{
	ItemPointerData heapTid;	/* TID of referenced heap item */
	OffsetNumber indexOffset;	/* index item's location within page */
	LocationIndex tupleOffset;	/* IndexTuple's offset in workspace, if any */
}			BTScanPosItem;

typedef struct BTScanPosData
{
	Buffer		buf;			/* if valid, the buffer is pinned */

	BlockNumber currPage;		/* page referenced by items array */
	BlockNumber nextPage;		/* page's right link when we scanned it */

	/*
	 * moreLeft and moreRight track whether we think there may be matching
	 * index entries to the left and right of the current page, respectively.
	 * We can clear the appropriate one of these flags when _bt_checkkeys()
	 * returns continuescan = false.
	 */
	bool		moreLeft;
	bool		moreRight;

	/*
	 * If we are doing an index-only scan, nextTupleOffset is the first free
	 * location in the associated tuple storage workspace.
	 */
	int			nextTupleOffset;

	/*
	 * The items array is always ordered in index order (ie, increasing
	 * indexoffset).  When scanning backwards it is convenient to fill the
	 * array back-to-front, so we start at the last slot and fill downwards.
	 * Hence we need both a first-valid-entry and a last-valid-entry counter.
	 * itemIndex is a cursor showing which entry was last returned to caller.
	 */
	int			firstItem;		/* first valid index in items[] */
	int			lastItem;		/* last valid index in items[] */
	int			itemIndex;		/* current index in items[] */

	BTScanPosItem items[MaxIndexTuplesPerPage]; /* MUST BE LAST */
}			BTScanPosData;

typedef BTScanPosData * BTScanPos;


#define BTScanPosIsValid_s(scanpos) \
( \
	BlockNumberIsValid_s((scanpos).currPage) \
)
#define BTScanPosInvalidate_s(scanpos) \
	do { \
		(scanpos).currPage = InvalidBlockNumber; \
		(scanpos).nextPage = InvalidBlockNumber; \
	} while (0);

#define BT_N_KEYS_OFFSET_MASK		0x0FFF

#define BTreeTupleSetNAtts_s(itup, n) \
	do { \
		(itup)->t_info |= INDEX_ALT_TID_MASK; \
		ItemPointerSetOffsetNumber_s(&(itup)->t_tid, (n) & BT_N_KEYS_OFFSET_MASK); \
	} while(0)



typedef struct BTScanOpaqueData
{
	/* these fields are set by _bt_preprocess_keys(): */
	bool		qual_ok;		/* false if qual can never be satisfied */
	int			numberOfKeys;	/* number of preprocessed scan keys */
	ScanKey		keyData;		/* array of preprocessed scan keys */

	/* workspace for SK_SEARCHARRAY support */
	ScanKey		arrayKeyData;	/* modified copy of scan->keyData */
	int			numArrayKeys;	/* number of equality-type array keys (-1 if
								 * there are any unsatisfiable array keys) */
	int			arrayKeyCount;	/* count indicating number of array scan keys
								 * processed */

	/*
	 * If we are doing an index-only scan, these are the tuple storage
	 * workspaces for the currPos and markPos respectively.  Each is of size
	 * BLCKSZ, so it can hold as much as a full page's worth of tuples.
	 */
	char	   *currTuples;		/* tuple storage for currPos */
	char	   *markTuples;		/* tuple storage for markPos */

	/*
	 * If the marked position is on the same page as current position, we
	 * don't use markPos, but just keep the marked itemIndex in markItemIndex
	 * (all the rest of currPos is valid for the mark position). Hence, to
	 * determine if there is a mark, first look at markItemIndex, then at
	 * markPos.
	 */
	int			markItemIndex;	/* itemIndex, or -1 if not valid */

	/* keep these last in struct for efficiency */
	BTScanPosData currPos;		/* current position data */
	BTScanPosData markPos;		/* marked position, if any */
}			BTScanOpaqueData;

typedef BTScanOpaqueData * BTScanOpaque;

/*
 * external entry points for btree, in nbtree.c
 */
extern bool btinsert_s(VRelation indexRel, VRelation heapRel, ItemPointer ht_ctid, char *datum, unsigned int datumSize);
extern IndexScanDesc btbeginscan_s(VRelation rel, const char *key, int keysize);
extern bool btgettuple_s(IndexScanDesc scan);
extern void btendscan_s(IndexScanDesc scan);




/*
 * prototypes for functions in nbtinsert.c
 */
extern bool _bt_doinsert_s(VRelation rel, IndexTuple itup, char *datum, int size, VRelation heapRel);
extern Buffer _bt_getstackbuf_s(VRelation rel, BTStack stack, int access);

/*
 * prototypes for functions in nbtpage.c
 */
extern void _bt_initmetapage_s(VRelation rel, BlockNumber rootbknum, uint32 level);
extern void _bt_upgrademetapage_s(Page page);
extern Buffer _bt_getroot_s(VRelation rel, int access);
extern void _bt_checkpage_s(VRelation rel, Buffer buf);
extern Buffer _bt_getbuf_s(VRelation rel, BlockNumber blkno, int access);
extern void _bt_relbuf_s(VRelation rel, Buffer buf);
extern void _bt_pageinit_s(Page page, Size size);

/*
 * prototypes for functions in nbtsearch.c
 */
extern BTStack _bt_search_s(VRelation rel,
							int keysz, ScanKey scankey, bool nextkey,
							Buffer * bufP, int access, bool isDummy);
extern OffsetNumber _bt_binsrch_s(VRelation rel, Buffer buf, int keysz,
								  ScanKey scankey, bool nextkey);
extern int32 _bt_compare_s(VRelation rel, int keysz, ScanKey scankey,
						   Page page, OffsetNumber offnum);
extern bool _bt_first_s(IndexScanDesc scan);
extern bool _bt_next_s(IndexScanDesc scan);
extern void bt_dummy_search_s(VRelation rel, int maxHeight);


/*
 * prototypes for functions in nbtutils.c
 */
extern ScanKey _bt_mkscankey_s(VRelation rel, IndexTuple itup, char *datum, int dsize);
extern void _bt_freeskey_s(ScanKey skey);
extern void _bt_freestack_s(BTStack stack);
extern IndexTuple _bt_checkkeys_s(IndexScanDesc scan,
								  Page page, OffsetNumber offnum, bool *continuescan);
extern int	bpchartruelen_s(char *s, int len);

extern unsigned int getRandomInt_nb(void);


#endif							/* NBTREE_H */
