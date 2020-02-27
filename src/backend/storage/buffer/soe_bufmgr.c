#include "storage/soe_bufmgr.h"
#include "access/soe_skey.h"
#include "logger/logger.h"
#include "oram/coram.h"

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
    vrel->level = 0;
    vrel->token = NULL;
    //memset(vrel->token, 0, sizeof(unsigned int)*32);
    vrel->rCounter = 2; //counter starts at 2 as blocks do two oblivious operations at initialization
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

	char	   *page = NULL;
    VBlock      block;	
	int			result;
    
    setToken(relation->oram, relation->token);
    result = read_oram(&page, blockNum, relation->oram, NULL);
	

    /**
     *  When the read returns a DUMMY_BLOCK page  it means its the
     *  first time the page is read from the disk.
     *  As such, a new page needs to be allocated.
     **/

    if (result == DUMMY_BLOCK){
        page = (char *) malloc(BLCKSZ); 
        memset(page, 0, BLCKSZ);
    } 

    block = (VBlock) malloc(sizeof(struct VBlock));
    
  
    block->id = blockNum;
	block->page = page;
	list_add(relation->buffer, block);

	return blockNum;
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
			return vblock->page;
		}
	}


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
        setToken(relation->oram, relation->token);
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
