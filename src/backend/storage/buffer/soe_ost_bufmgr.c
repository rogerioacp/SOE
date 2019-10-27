
#include "storage/soe_ost_bufmgr.h"
#include "access/soe_skey.h"
#include "logger/logger.h"
#include "storage/soe_heap_ofile.h"
#include "storage/soe_ost_ofile.h"

#include <stdlib.h>
#include <collectc/list.h>

// Assuming a single execution thread which does one search at a time, this file defines the variable clevel that tracks the current tree level being searched.
unsigned int clevel;


void setclevel(unsigned int nlevel){
    //selog(DEBUG1, "setclevel %d", nlevel);
    clevel = nlevel;
}

OSTRelation InitOSTRelation(OSTreeState relstate, unsigned int oid, char* attrDesc, unsigned int attrDescLength){

	OSTRelation rel = (OSTRelation) malloc(sizeof(struct OSTRelation));
	rel->osts = relstate;
	rel->rd_id = oid;
	rel->rd_amcache = NULL;

	rel->tDesc = (TupleDesc) malloc(sizeof(struct tupleDesc));
	rel->tDesc->natts = 1;
	rel->tDesc->attrs = (FormData_pg_attribute*) malloc(sizeof(struct FormData_pg_attribute));
	memcpy(rel->tDesc->attrs, attrDesc, attrDescLength);
	list_new(&(rel->buffer));

	clevel = 1;
    return rel;
}

Buffer ReadBuffer_ost(OSTRelation relation, BlockNumber blockNum){

	int result;
	char* page = NULL;
    PLBlock plblock = NULL;

    result = 0;
	if(blockNum == 0){
        plblock = createEmptyBlock();
        selog(DEBUG1, "plblock has block %p", plblock->block);
        ost_fileRead(plblock, relation->osts->iname, blockNum);
        selog(DEBUG1, "plblock has block %p", plblock->block);

        page = plblock->block;
	}else{
        selog(DEBUG1, "Going to read block %d on oram at height %d",blockNum, clevel);
		result = read_oram(&page, blockNum, relation->osts->orams[clevel-1]);
	}

	OSTVBlock block = (OSTVBlock) malloc(sizeof(struct OSTVBlock));
	block->id = blockNum;
	block->page = page;
	list_add(relation->buffer, block);
	
	if(result == DUMMY_BLOCK){
		selog(ERROR, "READ a dummy block on READ BUFFER");
    }

    /*if(clevel == relation->osts->nlevels){
    	clevel = 1;
    }else{
    	clevel +=1;
	}*/

    return blockNum;
}

Page BufferGetPage_ost(OSTRelation relation, Buffer buffer){
 	ListIter iter;
    OSTVBlock vblock;
    void* element;
    list_iter_init(&iter, relation->buffer);

    while(list_iter_next(&iter, &element) != CC_ITER_END){
        vblock = (OSTVBlock) element;
        selog(DEBUG1, "Page has id %d", vblock->id);
        if(vblock->id == buffer){
            selog(DEBUG1, "found page for buffer %d", buffer);
            return vblock->page;
        }
    }

    //selog(DEBUG1, "could not find page for buffer %d", buffer);

    return NULL;
}



void ReleaseBuffer_ost(OSTRelation relation, Buffer buffer){
    
    ListIter iter;
    OSTVBlock vblock;
    void* element;
    void* toFree;
    bool found;

    found = false;
    list_iter_init(&iter, relation->buffer);

    //Search with virtual block with buffer
    while(list_iter_next(&iter, &element) != CC_ITER_END){
        vblock = (OSTVBlock) element;
        if(vblock->id == buffer){
            found = true;
            break;
        }
    }

    if(found){
        //selog(DEBUG1, "Going to release buffer %d", buffer);
        list_remove(relation->buffer, vblock, &toFree);
        free(((VBlock)toFree)->page);
        free(toFree);
    }else{
        selog(DEBUG1, "Could not find buffer %d to release", buffer);
    }
}

BlockNumber
BufferGetBlockNumber_ost(Buffer buffer)
{
    return (BlockNumber) buffer;
}

void destroyOSTVBlock(void* block){
    free(((OSTVBlock) block)->page);
    free(block);
}

void closeOSTRelation(OSTRelation rel){
	int l;

	for(l=0; l < rel->osts->nlevels; l++){
		close_oram(rel->osts->orams[l]);
	}
	free(rel->osts->orams);
	free(rel->osts->fanouts);
    free(rel->osts->iname);
	free(rel->osts);

    list_remove_all_cb(rel->buffer, &destroyOSTVBlock);
    list_destroy(rel->buffer);

    if(rel->tDesc->attrs != NULL){
     free(rel->tDesc->attrs);
    }
    free(rel->tDesc);
    free(rel);
}
