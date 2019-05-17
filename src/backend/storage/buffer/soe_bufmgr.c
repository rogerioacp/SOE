#include "storage/soe_bufmgr.h"
#include "access/soe_skey.h"
#include "logger/logger.h"
#include "storage/soe_heap_ofile.h"

#include <stdlib.h>

//zero index block
//If the VRelation has 1 block, it only has the offset 0.
BlockNumber NumberOfBlocks(VRelation rel){
    return (BlockNumber) rel->lastFreeBlock;
}


VRelation InitVRelation(ORAMState relstate, unsigned int oid, int total_blocks, pageinit_function pg_f){
    int offset;
	VRelation vrel = (VRelation) malloc(sizeof(struct VRelation));
	vrel->oram = relstate;
    vrel->rd_id = oid;
	vrel->currentBlock = 0;
    vrel->lastFreeBlock = 0;
    vrel->totalBlocks = total_blocks;
    vrel->pageinit = pg_f;
    vrel->fsm = (int*) malloc(sizeof(int)*total_blocks);

    for(offset=0; offset < total_blocks; offset++){
        vrel->fsm[offset] = 0;
    }
	list_new(&(vrel->buffer));
	return vrel;
}

/**
*
* TODO: this function should see if the blocknumber is present on the list
* before searching on the oram.
*
*/
Buffer ReadBuffer(VRelation relation, BlockNumber blockNum){

    bool isExtended;
    char* page = NULL;
    void* element;
    VBlock searchVBlock;
    OblivPageOpaque oopaque;

    isExtended = (blockNum == P_NEW);
    BlockNumber blockID;
    ListIter iter;
    int result;



    if(isExtended){
        //selog(DEBUG1, "1 - Going to oram read real block %d", relation->lastFreeBlock);

        result = read(&page, relation->lastFreeBlock, relation->oram);
        if( result == DUMMY_BLOCK){
            //selog(DEBUG1, "Found Dummy block, going to initialize to blkno %d", relation->lastFreeBlock);
            /**
             *  When the read returns a DUMMY_BLOCK page  it means its the 
             *  first time the page is read from the disk.
             *  As such, a new page needs to be allocated.
             **/

            page = (char*) malloc(BLCKSZ);
            relation->pageinit(page, relation->lastFreeBlock, BLCKSZ);
        }

        /**
         * When the read returns a DUMMY_BLOCK page  it means its the first time the page is read from the disk.
         * As such, a new page needs to be allocated and initialized so the tuple can be added.
         **/
        blockID = relation->lastFreeBlock;
        relation->lastFreeBlock += 1;

    }else{

        if(blockNum > relation->lastFreeBlock){
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
        while(list_iter_next(&iter, &element)!= CC_ITER_END){
            searchVBlock = (VBlock) element;
            if(searchVBlock->id == blockNum){
                return blockNum;
            }
        }

        //selog(DEBUG1, "2 - Going to oram read real block %d", blockNum);
        result = read(&page, blockNum, relation->oram);
        if(result == DUMMY_BLOCK){
            selog(ERROR, "READ a dummy block on READ BUFFER");
        }
        oopaque = (OblivPageOpaque) PageGetSpecialPointer((Page) page);
        //selog(DEBUG1, "Page special pointer has blkno %d", oopaque->o_blkno);
        blockID = blockNum;
    }

     VBlock block = (VBlock) malloc(sizeof(struct VBlock));
     block->id = blockID;
     block->page = page;
     list_add(relation->buffer, block);

    oopaque = (OblivPageOpaque) PageGetSpecialPointer((Page) block->page);
    // selog(DEBUG1, "Inserted block on buffer list with blkno %d and special %d", block->id, oopaque->o_blkno);

    return blockID;
}


Page BufferGetPage(VRelation relation, Buffer buffer)
{
    ListIter iter;
    VBlock vblock;
    void* element;
    list_iter_init(&iter, relation->buffer);

    while(list_iter_next(&iter, &element) != CC_ITER_END){
        vblock = (VBlock) element;
        if(vblock->id == buffer){
            return vblock->page;
        }
    }
    return NULL;
}


void MarkBufferDirty(VRelation relation, Buffer buffer){

    int result;
    ListIter iter;
    VBlock vblock;
    void* element;
    OblivPageOpaque oopaque;

    list_iter_init(&iter, relation->buffer);

    //Search with virtual block with buffer
    while(list_iter_next(&iter, &element) != CC_ITER_END){
        vblock = (VBlock) element;
        if(vblock->id == buffer){
            oopaque = (OblivPageOpaque) PageGetSpecialPointer( (Page) vblock->page);
            //selog(DEBUG1, "Found page on buffer list with blkno %d and special %d", vblock->id, oopaque->o_blkno);

            break;
        }
    }

    //selog(DEBUG1, "GOING to oblivsiou write to real blkno %d", vblock->id);
    result  = write(vblock->page, BLCKSZ, vblock->id, relation->oram);

    if(result != BLCKSZ){
        selog(ERROR, "Write failed to write a complete page");
    }

}

void ReleaseBuffer(VRelation relation, Buffer buffer){
    
    ListIter iter;
    VBlock vblock;
    void* element;
    void* toFree;

    list_iter_init(&iter, relation->buffer);

    //Search with virtual block with buffer
    while(list_iter_next(&iter, &element) != CC_ITER_END){
        vblock = (VBlock) element;
        if(vblock->id == buffer){
            break;
        }
    }
    
    list_remove(relation->buffer, vblock, &toFree);
    free(((VBlock)toFree)->page);
    free(toFree);
}


BlockNumber
BufferGetBlockNumber(Buffer buffer)
{
    return (BlockNumber) buffer;
}

BlockNumber FreeSpaceBlock(VRelation rel){

    selog(DEBUG1, "Current block is %d and has %d tuples", rel->currentBlock, rel->fsm[rel->currentBlock]);

    if(rel->fsm[rel->currentBlock] == 0){
        return P_NEW;
    }else{
        return rel->currentBlock;
    }
}

void UpdateFSM(VRelation rel){
    rel->fsm[rel->currentBlock] +=1;
}

void BufferFull(VRelation rel, Buffer buff){
    rel->currentBlock +=1;
}

