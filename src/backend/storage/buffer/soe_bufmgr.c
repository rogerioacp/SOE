#include "storage/soe_bufmgr.h"
#include "access/soe_skey.h"

#include <stdlib.h>

BlockNumber getNumberOfBlocks(VRelation rel){
    return (BlockNumber) rel->lastBlock;
}


VRelation initVRelation(ORAMState relstate){
	VRelation vrel = (VRelation) malloc(sizeof(struct VRelation));
	vrel->oram = relstate;
	vrel->lastBlock = 0;
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

    isExtended = (blockNum == P_NEW);
    int blockID;
    ListIter iter;
    int result;

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


    if(isExtended){
        result = read(&page, (BlockNumber) relation->lastBlock, relation->oram);

        //if(result != DUMMY_BLOCK){
            // Log exception
        //} 

        /**
         * When the read returns a DUMMY_BLOCK page  it means its the first time the page is read from the disk.
         * As such, a new page needs to be allocated and initialized so the tuple can be added.
         **/
        page = (char*) malloc(BLCKSZ);
        blockID = relation->lastBlock;
        relation->lastBlock +=1;

    }else{
        if(blockNum > relation->lastBlock){
            return DUMMY_BLOCK;
        }

        result = read(&page, (BlockNumber) relation->lastBlock, relation->oram);
        blockID = blockNum;
    }

     VBlock block = (VBlock) malloc(sizeof(struct VBlock));
     block->id = blockID;
     block->page = page;
     list_add(relation->buffer, block);

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
        if(vblock->id = buffer){
            return vblock->page;
        }
    }
    return NULL;
}


void MarkBufferDirty(VRelation relation, Buffer buffer){

    int result;
    ListIter iter;
    VBlock vblock;
    void* toFree;
    vblock->id = -1;
    void* element;

    list_iter_init(&iter, relation->buffer);
    //Search with virtual block with buffer
    while(list_iter_next(&iter, &element) != CC_ITER_END){
        vblock = (VBlock) element;
        if(vblock->id = buffer){
            break;
        }
    }

    /*if(vblock->id == -1){
        //elog("")
        //Log buffer does not exist
    }*/

    result  = write(vblock->page, BLCKSZ, vblock->id, relation->oram);
    if(result == BLCKSZ){
        list_remove(relation->buffer, vblock, &toFree);
        free(((VBlock)toFree)->page);
        free(toFree);
    }/*else{
        //log error in writing to oram.
    }*/


}

void ReleaseBuffer(VRelation * relation, Buffer buffer){
    free(relation);
}


/*
 * BufferGetBlockNumber
 *      Returns the block number associated with a buffer.
 *
 * Note:
 *      Assumes that the buffer is valid and pinned, else the
 *      value may be obsolete immediately...
 */
BlockNumber
BufferGetBlockNumber(Buffer buffer)
{
    return (BlockNumber) buffer;
}

