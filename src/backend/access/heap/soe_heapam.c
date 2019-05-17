
//#include "access/soe_genam.h"
#include "access/soe_heapam.h"
#include "logger/logger.h"

void
heap_insert(VRelation rel, Item tup, Size len){

	Buffer buffer;
	Page page;
	OffsetNumber offnum;
	Size pageFreeSpace,
		 saveFreeSpace;
	Size alignedSize;
	BlockNumber freeSpaceBlock;

	freeSpaceBlock = FreeSpaceBlock(rel);

	selog(DEBUG1, "Free space block returned is %d", freeSpaceBlock);

	buffer = ReadBuffer(rel, freeSpaceBlock);

	selog(DEBUG1, "buffer id is %d", buffer);
	if(buffer == DUMMY_BLOCK){
		selog(ERROR, "An invalid block number was requested");
	}

	page = BufferGetPage(rel, buffer);
	//selog(DEBUG1, " Going to align size %d ", len);
	alignedSize = MAXALIGN(len);		/* be conservative */
	//selog(DEBUG1, "Size %d aligned is %d", len, alignedSize);
	/* Compute desired extra freespace due to fillfactor option */
	saveFreeSpace = 0; //Assuming a save free space of 0.
	/*saveFreeSpace = RelationGetTargetPageFreeSpace(rel,
												   HEAP_DEFAULT_FILLFACTOR);*/
	//selog(DEBUG1, "saveFreeSpace is %d", saveFreeSpace);
	/*
	* Check to see if the page still has free space to insert the item,
	* if not, move to the next page. In this prototype we are 
	* assuming that items have more or less the same space and 
	* that no page has holes. Thus items are inserted sequentially insertedn 
	* the pages and never look up free space on previous pages.
	* Copied from RelationGetBufferForTuple in hio.c
	*/
	pageFreeSpace = PageGetHeapFreeSpace(page);
	selog(DEBUG1, "current page free space is %d", pageFreeSpace);

	if (alignedSize + saveFreeSpace > pageFreeSpace)
	{
		selog(WARNING, "Page has no free space");
		BufferFull(rel, buffer);

		ReleaseBuffer(rel, buffer);
		buffer = ReadBuffer(rel, FreeSpaceBlock(rel));
		page = BufferGetPage(rel, buffer);
	}

	offnum = PageAddItem(page, tup, len, InvalidOffsetNumber, false, true);

	/* Update tuple->t_self to the actual position where it was stored */
	//ItemPointerSet(&(tup->t_self), BufferGetBlockNumber(buffer), offnum);

	ItemId itemId = PageGetItemId(page, offnum);
	HeapTupleHeader item = (HeapTupleHeader) PageGetItem(page, itemId);
	ItemPointerSet(&(item->t_ctid), BufferGetBlockNumber(buffer), offnum);
	//selog(DEBUG1, "ITEM inserted in block %d and offnum %d", BufferGetBlockNumber(buffer), offnum );
	//selog(DEBUG1, "Inserted item id has offset %d and length %d", ItemIdGetOffset(itemId),  ItemIdGetLength(itemId));
	if (!ItemIdIsNormal(itemId)){
		selog(ERROR, "Item ID is not normal");
	}
	//item->t_ctid = tup->t_self;

	MarkBufferDirty(rel, buffer);
	ReleaseBuffer(rel, buffer);
	UpdateFSM(rel);

}

/**
* The logic for this function was taken from the functions index_fetch_heap in
* indexam.c and from heap_hot_search_buffer in heapam.c.
* The major difference is the lack of support for locks and Hot-chains.
* So its just a simple tuple access.
**/
void heap_gettuple(VRelation rel, ItemPointer tid, HeapTuple tuple){

	BlockNumber blkno;
	Buffer buffer;
	Page page;
	OffsetNumber offnum;
	ItemId lp;

	blkno = ItemPointerGetBlockNumber(tid);
	buffer = ReadBuffer(rel, blkno);

	if(ItemPointerGetBlockNumber(tid) != BufferGetBlockNumber(buffer)){
		selog(ERROR, "Requested Pointer does not match block number. %d != %d",ItemPointerGetBlockNumber(tid), BufferGetBlockNumber(buffer));
	}

	page = BufferGetPage(rel, buffer);

	offnum = ItemPointerGetOffsetNumber(tid);

	tuple->t_self = *tid;

	lp = PageGetItemId(page, offnum);
	//selog(DEBUG1, "Item id has offset %zu ", ItemIdGetOffset(lp));
	if (!ItemIdIsNormal(lp)){
		selog(ERROR, "Item ID is not normal");
	}
	
	tuple->t_data = (HeapTupleHeader) PageGetItem(page, lp);
	tuple->t_len = ItemIdGetLength(lp);
	tuple->t_tableOid = RelationGetRelid(rel);
	ItemPointerSetOffsetNumber(&tuple->t_self, offnum);
	//TODO: Is this release freening the buffer correctly?
	ReleaseBuffer(rel, buffer);
}


