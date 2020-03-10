
#include "storage/soe_ost_bufmgr.h"
#include "access/soe_skey.h"
#include "logger/logger.h"
#include "storage/soe_heap_ofile.h"
#include "storage/soe_ost_ofile.h"
#include "oram/coram.h"

#include <stdlib.h>


OSTRelation
InitOSTRelation(OSTreeState relstate, unsigned int oid, char *attrDesc, unsigned int attrDescLength)
{

	int			loffset;

	OSTRelation rel = (OSTRelation) malloc(sizeof(struct OSTRelation));

	rel->osts = relstate;
	rel->rd_id = oid;
	rel->rd_amcache = NULL;
	rel->level = 0;
	/* Current tree level being used. */

	/*
	 * Allocate an array of list pointers where each array entry stores the
	 * blocks of the buffers accessed per level.
	 */
	rel->buffers = (List * *) malloc(sizeof(List *) * (relstate->nlevels + 1));
	for (loffset = 0; loffset < relstate->nlevels + 1; loffset++)
	{
		list_new(&(rel->buffers[loffset]));
	}

	rel->tDesc = (TupleDesc) malloc(sizeof(struct tupleDesc));
	rel->tDesc->natts = 1;
	rel->tDesc->attrs = (FormData_pg_attribute *) malloc(sizeof(struct FormData_pg_attribute));
	memcpy(rel->tDesc->attrs, attrDesc, attrDescLength);
    
    rel->token = NULL;
    rel->leafCurrentCounter = 0;
    rel->heapBlockCounter = 0;

	return rel;
}

Buffer ReadDummyBuffer_ost(OSTRelation relation, int treeLevel, 
                           BlockNumber blkno){
    int result = 0;

    #ifdef DUMMYS 
    PLBlock plblock = NULL;
    char *page = NULL;

    int clevel = treeLevel;

    if(clevel == 0){
        plblock = createEmptyBlock();
        /*PLBList list = (PLBList) malloc(sizeof(PLBlock)); 
        list[0] = plblock;
        BNArray bnarray = (BNArray) malloc(sizeof(int));
        bnarray[0] = blkno*/

		/*
		 * The OST fileRead always allocates and writes the content of the
		 * file page, even if the content is a dummy page.
		 */
		ost_fileRead(NULL, relation->osts->iname, (PLBList) &plblock,
                    (BNArray) &blkno, 1, &clevel);
	    free(plblock);
        result = plblock->size;
    }else{
        result = read_oram(&page, blkno, relation->osts->orams[clevel - 1], &clevel);
        free(page); 
    }
    #endif

    return result;
    

}


Buffer
ReadBuffer_ost(OSTRelation relation, BlockNumber blockNum)
{

	int			result = 0;
	char	   *page = NULL;
	int			clevel = relation->level;
	PLBlock		plblock = NULL;
    ORAMState   oram = NULL;

	/*
	 * This code assumes that there are no consecutive accesses to read the
	 * same buffer from the same level. Otherwise, if this is not true, we can
	 * optimize the code to search first on the buffer list for the correct
	 * buffer before accessing the file.
	 */

	if (clevel == 0)
	{
		plblock = createEmptyBlock();

		/*
		 * The OST fileRead always allocates and writes the content of the
		 * file page, even if the content is a dummy page.
		 */
		ost_fileRead(NULL, relation->osts->iname, (PLBList) &plblock, 
                     (BNArray) &blockNum, 1, &clevel);
		page = plblock->block;
		free(plblock);
	}
	else
	{
        oram = relation->osts->orams[clevel-1];
        
        setToken(oram, relation->token);
        //selog(DEBUG1, "Read oram ost block %d at level %d", blockNum, clevel);
		result = read_oram(&page, blockNum, oram, &clevel);

		/**
         *  When the read returns a DUMMY_BLOCK page  it means its the
         *  first time the page is read from the disk.
         *  As such, a new page needs to be allocated.
         *  The content of the page is written by the application
         **/
		if (result == DUMMY_BLOCK)
		{
			page = (char *) malloc(BLCKSZ);
			memset(page, 0, BLCKSZ);
		}
	}

	OSTVBlock	block = (OSTVBlock) malloc(sizeof(struct OSTVBlock));

	block->id = blockNum;
	block->page = page;
	list_add(relation->buffers[clevel], block);

	return blockNum;
}

Page
BufferGetPage_ost(OSTRelation relation, Buffer buffer)
{
	ListIter	iter;
	OSTVBlock	vblock;
	void	   *element;
	int			clevel = relation->level;

	list_iter_init(&iter, relation->buffers[clevel]);

	while (list_iter_next(&iter, &element) != CC_ITER_END)
	{
		vblock = (OSTVBlock) element;
		if (vblock->id == buffer)
		{
			return vblock->page;
		}
	}


	return NULL;
}


void
MarkBufferDirty_ost(OSTRelation relation, Buffer buffer)
{

	int			result;
	ListIter	iter;
	VBlock		vblock;
	void	   *element;
	bool		found = false;

	result = 0;
	int			clevel = relation->level;
    ORAMState   oram = NULL;
	/* OblivPageOpaque oopaque; */

	list_iter_init(&iter, relation->buffers[clevel]);

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

		if (clevel == 0)
		{
			PLBlock		block = createEmptyBlock();

			block->blkno = vblock->id;
			block->block = vblock->page;
			block->size = BLCKSZ;
			ost_fileWrite(NULL, relation->osts->iname,(PLBList) &block, 
                          (BNArray) &vblock->id, 1, &clevel);
			free(block);
            result = BLCKSZ;
		}
		else
		{
            oram = relation->osts->orams[clevel - 1];
            setToken(oram, relation->token);
			result = write_oram(vblock->page, BLCKSZ, vblock->id, oram ,&clevel);
		}
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
ReleaseBuffer_ost(OSTRelation relation, Buffer buffer)
{

	ListIter	iter;
	OSTVBlock	vblock;
	void	   *element;
	void	   *toFree;
	bool		found;
	int			clevel = relation->level;

	found = false;
	list_iter_init(&iter, relation->buffers[clevel]);

	/* Search with virtual block with buffer */
	while (list_iter_next(&iter, &element) != CC_ITER_END)
	{
		vblock = (OSTVBlock) element;
        if (vblock->id == buffer)
		{
			found = true;
			break;
		}
	}

	if (found)
	{
		list_remove(relation->buffers[clevel], vblock, &toFree);
		free(((VBlock) toFree)->page);
		free(toFree);
	}
	else
	{
		selog(DEBUG1, "Could not find buffer %d to release", buffer);
	}
}

BlockNumber
BufferGetBlockNumber_ost(Buffer buffer)
{
	return (BlockNumber) buffer;
}

void
destroyOSTVBlock(void *block)
{
	free(((OSTVBlock) block)->page);
	free(block);
}

void
closeOSTRelation(OSTRelation rel)
{
	int			l;


	for (l = 0; l < rel->osts->nlevels; l++)
	{
		close_oram(rel->osts->orams[l], NULL);
	}
	free(rel->osts->orams);
	free(rel->osts->fanouts);
	free(rel->osts->iname);


	for (l = 0; l < rel->osts->nlevels + 1; l++)
	{

		list_remove_all_cb(rel->buffers[l], &destroyOSTVBlock);
		list_destroy(rel->buffers[l]);
	}

    free(rel->buffers);
	free(rel->osts);

	if (rel->tDesc->attrs != NULL)
	{
		free(rel->tDesc->attrs);
	}
	free(rel->tDesc);
	free(rel);
}
