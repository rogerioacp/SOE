#include "storage/soe_bufmgr.h"
#include "access/soe_skey.h"
#include "logger/logger.h"
/* #include "storage/soe_heap_ofile.h" */

#include <stdlib.h>
#include <collectc/list.h>

/* zero index block */
/* If the VRelation has 1 block, it only has the offset 0. */
BlockNumber
NumberOfBlocks_s(VRelation rel)
{
	return (BlockNumber) rel->lastFreeBlock;
}


VRelation
InitVRelation(ORAMState relstate, unsigned int oid, int total_blocks, pageinit_function pg_f)
{
	int			offset;
	VRelation	vrel = (VRelation) malloc(sizeof(struct VRelation));

	vrel->oram = relstate;
	vrel->rd_id = oid;
	vrel->currentBlock = 0;
	vrel->lastFreeBlock = 0;
	vrel->totalBlocks = total_blocks;
	vrel->pageinit = pg_f;
	vrel->fsm = (int *) malloc(sizeof(int) * total_blocks);
	vrel->rd_amcache = NULL;
	for (offset = 0; offset < total_blocks; offset++)
	{
		vrel->fsm[offset] = 0;
	}
	list_new(&(vrel->buffer));
	vrel->tDesc = (TupleDesc) malloc(sizeof(struct tupleDesc));
	vrel->tDesc->attrs = NULL;

    vrel->tHeight = 0;
	return vrel;
}


Buffer 
ReadDummyBuffer(VRelation relation, BlockNumber blkno){
    int     result = 0;
    #ifdef DUMMYS
    char    *page = NULL;

    result = read_oram(&page, blkno, relation->oram, NULL);

    free(page);
    #endif
    return result;
}


/**
*
* TODO: this function should see if the blocknumber is present on the list
* before searching on the oram.
*
*/
Buffer
ReadBuffer_s(VRelation relation, BlockNumber blockNum)
{

	bool		isExtended;
	char	   *page = NULL;
	void	   *element;
	VBlock		searchVBlock;


	isExtended = (blockNum == P_NEW);
	BlockNumber blockID;
	ListIter	iter;
	int			result;


	if (isExtended)
	{

		result = read_oram(&page, relation->lastFreeBlock, relation->oram, NULL);
		if (result == DUMMY_BLOCK)
		{
			
			/**
             *  When the read returns a DUMMY_BLOCK page  it means its the
             *  first time the page is read from the disk.
             *  As such, a new page needs to be allocated.
             **/

			page = (char *) malloc(BLCKSZ); 
			relation->pageinit(page, relation->lastFreeBlock, BLCKSZ);
        }

		/**
         * When the read returns a DUMMY_BLOCK page  it means its the first time the page is read from the disk.
         * As such, a new page needs to be allocated and initialized so the tuple can be added.
         **/
		blockID = relation->lastFreeBlock;
		relation->lastFreeBlock += 1;

	}
	else
	{

		if (blockNum > relation->lastFreeBlock)
		{
			return DUMMY_BLOCK;
		}

		list_iter_init(&iter, relation->buffer);
		/**
            Search for existing buffer.
            This code is necessary since even in a single thread execution there
            can be multiple ReadBuffer calls to the same blockNum. For instance,
            the HashIndexes calls ReadBuffer twice on the MetaPage to get different lock types. Since this code is maintained,  we keep the
            invocation sequence the same and ignore the locks.  On the second call
            we just search the list, instead of going outside of the enclave.
        */
		while (list_iter_next(&iter, &element) != CC_ITER_END)
		{
			searchVBlock = (VBlock) element;
			if (searchVBlock->id == blockNum)
			{
				return blockNum;
			}
		}

		result = read_oram(&page, blockNum, relation->oram, NULL);
		if (result == DUMMY_BLOCK)
		{
			selog(ERROR, "READ a dummy block on READ BUFFER");
		}
		/* oopaque = (OblivPageOpaque) PageGetSpecialPointer((Page) page); */

		/*
		 * selog(DEBUG1, "Page special pointer has blkno %d",
		 * oopaque->o_blkno);
		 */
		blockID = blockNum;
	}

	VBlock		block = (VBlock) malloc(sizeof(struct VBlock));

	block->id = blockID;
	block->page = page;
	list_add(relation->buffer, block);

	return blockID;
}


Page
BufferGetPage_s(VRelation relation, Buffer buffer)
{
	ListIter	iter;
	VBlock		vblock;
	void	   *element;

	list_iter_init(&iter, relation->buffer);

	while (list_iter_next(&iter, &element) != CC_ITER_END)
	{
		vblock = (VBlock) element;
		if (vblock->id == buffer)
		{
			/* selog(DEBUG1, "found page for buffer %d", buffer); */
			return vblock->page;
		}
	}

	/* selog(DEBUG1, "could not find page for buffer %d", buffer); */

	return NULL;
}


void
MarkBufferDirty_s(VRelation relation, Buffer buffer)
{

	int			result;
	ListIter	iter;
	VBlock		vblock;
	void	   *element;
	bool		found = false;

	result = 0;

	list_iter_init(&iter, relation->buffer);

	/* Search with virtual block with buffer */
	while (list_iter_next(&iter, &element) != CC_ITER_END)
	{
		vblock = (VBlock) element;
		if (vblock->id == buffer)
		{
			found = true;
			break;
		}
	}
	if (found)
	{	
		result = write_oram(vblock->page, BLCKSZ, vblock->id, relation->oram, NULL);
	}
	else
	{
		selog(DEBUG1, "Did not find buffer %d to update", buffer);
	}

	if (result != BLCKSZ)
	{
		selog(ERROR, "Write failed to write a complete page");
	}

}

void
ReleaseBuffer_s(VRelation relation, Buffer buffer)
{

	ListIter	iter;
	VBlock		vblock;
	void	   *element;
	void	   *toFree;
	bool		found;

	found = false;
	list_iter_init(&iter, relation->buffer);

	/* Search with virtual block with buffer */
	while (list_iter_next(&iter, &element) != CC_ITER_END)
	{
		vblock = (VBlock) element;
		if (vblock->id == buffer)
		{
			found = true;
			break;
		}
	}

	/*
	 * The hash index might request to release buffer that has already been
	 * released previously during the search for a tuple.
	 */
	if (found)
	{
		list_remove(relation->buffer, vblock, &toFree);
		free(((VBlock) toFree)->page);
		free(toFree);
	}
	else
	{
		selog(DEBUG1, "Could not find buffer %d to release", buffer);
	}
}


BlockNumber
BufferGetBlockNumber_s(Buffer buffer)
{
	return (BlockNumber) buffer;
}

BlockNumber
FreeSpaceBlock_s(VRelation rel)
{

	if (rel->fsm[rel->currentBlock] == 0)
	{
		return P_NEW;
	}
	else
	{
		return rel->currentBlock;
	}
}

void
UpdateFSM(VRelation rel)
{
	rel->fsm[rel->currentBlock] += 1;
}

void
BufferFull_s(VRelation rel, Buffer buff)
{
	rel->currentBlock += 1;
}

void
destroyVBlock(void *block)
{
	free(((VBlock) block)->page);
	free(block);
}

void
closeVRelation(VRelation rel)
{
	close_oram(rel->oram, NULL);
	list_remove_all_cb(rel->buffer, &destroyVBlock);
	list_destroy(rel->buffer);
	if (rel->rd_amcache != NULL)
	{
		free(rel->rd_amcache);
	}
	if (rel->tDesc->attrs != NULL)
	{
		free(rel->tDesc->attrs);
	}
	free(rel->tDesc);
	free(rel->fsm);
	free(rel);
}
