
/* #include "access/soe_genam.h" */
#include "access/soe_heapam.h"
#include "logger/logger.h"
#include "common/soe_prf.h"

void
heap_insert_s(VRelation rel, Item tup, Size len, HeapTuple tuple)
{

	Buffer		buffer;
	Page		page;
	OffsetNumber offnum;
	Size		pageFreeSpace,
				saveFreeSpace;
	Size		alignedSize;
	BlockNumber freeSpaceBlock;

	freeSpaceBlock = FreeSpaceBlock_s(rel);

	/* selog(DEBUG1, "Free space block returned is %d", freeSpaceBlock); */

	buffer = ReadBuffer_s(rel, freeSpaceBlock);

	/* selog(DEBUG1, "buffer id is %d", buffer); */
	if (buffer == DUMMY_BLOCK)
	{
		selog(ERROR, "An invalid block number was requested");
	}

	page = BufferGetPage_s(rel, buffer);
	/* selog(DEBUG1, " Going to align size %d ", len); */
	alignedSize = MAXALIGN_s(len);	/* be conservative */
	/* selog(DEBUG1, "Size %d aligned is %d", len, alignedSize); */
	/* Compute desired extra freespace due to fillfactor option */
	saveFreeSpace = 0;
	/* Assuming a save free space of 0. */

	/*
	 * saveFreeSpace = RelationGetTargetPageFreeSpace(rel,
	 * HEAP_DEFAULT_FILLFACTOR);
	 */
	/* selog(DEBUG1, "saveFreeSpace is %d", saveFreeSpace); */

	/*
	 * Check to see if the page still has free space to insert the item, if
	 * not, move to the next page. In this prototype we are assuming that
	 * items have more or less the same space and that no page has holes. Thus
	 * items are inserted sequentially insertedn the pages and never look up
	 * free space on previous pages. Copied from RelationGetBufferForTuple in
	 * hio.c
	 */
	pageFreeSpace = PageGetHeapFreeSpace_s(page);
	/* selog(DEBUG1, "current page free space is %d", pageFreeSpace); */

	if (alignedSize + saveFreeSpace > pageFreeSpace)
	{
		/* selog(WARNING, "Page has no free space %d",buffer); */
		BufferFull_s(rel, buffer);

		ReleaseBuffer_s(rel, buffer);
		buffer = ReadBuffer_s(rel, FreeSpaceBlock_s(rel));
		page = BufferGetPage_s(rel, buffer);
	}

	offnum = PageAddItem_s(page, tup, len, InvalidOffsetNumber, false, true);


	tuple->t_data = (HeapTupleHeader) tup;
	tuple->t_len = len;
	tuple->t_tableOid = RelationGetRelid_s(rel);

    //selog(DEBUG1, "Setting item pointer set to %d %d", BufferGetBlockNumber_s(buffer), offnum);
	/* Update tuple->t_self to the actual position where it was stored */
	ItemPointerSet_s(&(tuple->t_self), BufferGetBlockNumber_s(buffer), offnum);


	/*
	 * Insert the correct position into CTID of the stored tuple, too (unless
	 * this is a speculative insertion, in which case the token is held in
	 * CTID field instead)
	 */

	ItemId		itemId = PageGetItemId_s(page, offnum);
	HeapTupleHeader item = (HeapTupleHeader) PageGetItem_s(page, itemId);

	item->t_ctid = tuple->t_self;

	/*
	 * ItemPointerSet_s(&(item->t_ctid), BufferGetBlockNumber_s(buffer),
	 * offnum);
	 */

	/*
	 * selog(DEBUG1, "ITEM inserted in block %d and offnum %d",
	 * BufferGetBlockNumber(buffer), offnum );
	 */

	/*
	 * selog(DEBUG1, "Inserted item id has offset %d and length %d",
	 * ItemIdGetOffset(itemId),  ItemIdGetLength(itemId));
	 */
	if (!ItemIdIsNormal_s(itemId))
	{
		selog(ERROR, "Item ID is not normal");
	}

	MarkBufferDirty_s(rel, buffer);
	ReleaseBuffer_s(rel, buffer);
	UpdateFSM(rel);

}

void
heap_insert_block_s(VRelation rel, char *rpage, int blkno)
{
	Buffer		buffer;
    Page		page;
    int*        r_blkno;
    int*        p_blkno;
    unsigned int    token[4];
    int level = rel->tHeight +1;
    //selog(DEBUG1, "Inserting heap block %d, level %d", blkno, level);
    
    r_blkno = (int*) PageGetSpecialPointer_s(rpage);
    prf(level, blkno, 0, (unsigned char*) &token);
    //selog(DEBUG1, "Counters are %d %d %d %d\n", token[0], token[1], token[2], token[3]); 
    //selog(DEBUG1, "size of oopaque lsize %d\n", r_blkno[1]);
    //selog(DEBUG1, "Page has %d tuples",PageGetMaxOffsetNumber_s(rpage));  
    rel->token = token;
    if(*r_blkno != blkno){
        selog(ERROR, "Page block %d number does not match offset %d", *r_blkno, blkno);
    }


    buffer = ReadBuffer_s(rel, *r_blkno);
	page = BufferGetPage_s(rel, buffer);

    if(page == NULL){
        selog(ERROR, "Page accessed on block loading %d is null", *r_blkno);
    }

	memcpy(page, rpage, BLCKSZ);



    p_blkno = (int*) PageGetSpecialPointer_s(page);

    
    //selog(DEBUG1, "size of oopaque lsize %d and location is %d %d", p_blkno[1], p_blkno[2], p_blkno[3]);

    if(*r_blkno != *p_blkno){
        selog(ERROR, "Block numbers in heap page do not match %d %d", *r_blkno, *p_blkno);
    }

    prf(level, blkno, 1, (unsigned char*) &token);

    //selog(DEBUG1, "Counters are %d %d %d %d", token[0], token[1], token[2], token[3]);
      
	MarkBufferDirty_s(rel, buffer);
	ReleaseBuffer_s(rel, buffer);
}

/**
* The logic for this function was taken from the functions index_fetch_heap in
* indexam.c and from heap_hot_search_buffer in heapam.c.
* The major difference is the lack of support for locks and Hot-chains.
* So its just a simple tuple access.
**/
void
heap_gettuple_s(VRelation rel, ItemPointer tid, HeapTuple tuple)
{

	BlockNumber blkno;
	Buffer		buffer;
	Page		page;
	OffsetNumber offnum;
	ItemId		lp;
    uint32      token[4];
	blkno = ItemPointerGetBlockNumber_s(tid);
	//selog(DEBUG1, "Going to get heap block %d", blkno);

    prf(rel->tHeight, blkno, rel->heapBlockCounter, (unsigned char*) &token);
    //selog(DEBUG1, "counter of block %d are %d %d %d %d", blkno, token[0], token[1], token[2], token[3]);

    rel->token = token;
	buffer = ReadBuffer_s(rel, blkno);
	if (ItemPointerGetBlockNumber_s(tid) != BufferGetBlockNumber_s(buffer))
	{
		selog(ERROR, "Requested Pointer does not match block number. %d != %d", ItemPointerGetBlockNumber_s(tid), BufferGetBlockNumber_s(buffer));
        abort();
	}
    //selog(DEBUG1, "Heap read buffer %d", buffer);
	page = BufferGetPage_s(rel, buffer);

	offnum = ItemPointerGetOffsetNumber_s(tid);
	//selog(DEBUG1, "Item offset is %d", offnum); 
	tuple->t_self = *tid;

	lp = PageGetItemId_s(page, offnum);
	//selog(DEBUG1, "Item id has offset %zu ", ItemIdGetOffset_s(lp));
	if (!ItemIdIsNormal_s(lp))
	{
		selog(ERROR, "Item ID is not normal");
        abort();
        //exit(1);
	}

	tuple->t_len = ItemIdGetLength_s(lp);
	tuple->t_tableOid = RelationGetRelid_s(rel);
	tuple->t_data = (HeapTupleHeader) malloc(tuple->t_len);
    memcpy(tuple->t_data, PageGetItem_s(page, lp), tuple->t_len);
	ItemPointerSetOffsetNumber_s(&tuple->t_self, offnum);
    
    //MarkBufferDirty_s(rel, buffer);
	ReleaseBuffer_s(rel, buffer);
}
