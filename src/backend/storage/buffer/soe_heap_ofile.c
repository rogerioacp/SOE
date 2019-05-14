/*-------------------------------------------------------------------------
 *
 * soe_ofile.c
 *     Functions that implemented the oblivious file used by the ORAM Library.
 *
 *
 * The goal of this code is to bridge the blocks used inside the enclave and
 * the actual blocks written to the database disk. This code is responsible
 * for leveraging the enclave ocalls to write directly in the database relation
 * files.
 * 
 * Copyright (c) 2018-2019, HASLab
 *
 * IDENTIFICATION
 *        backend/storage/buffer/soe_ofile.c
 *
 *-------------------------------------------------------------------------
 */


#ifdef UNSAFE
#include "Enclave_dt.h"
#else
#include "sgx_trts.h"
#include "Enclave_t.h"
#endif

#include "logger/logger.h"
#include "storage/soe_heap_ofile.h"

#include <oram/plblock.h>
#include <oram/ofile.h>
#include <string.h>
#include <stdlib.h>



void heap_pageInit(Page page, BlockNumber blkno, Size blocksize){
    OblivPageOpaque oopaque;
	PageInit(page,  blocksize, sizeof(OblivPageOpaqueData));
	oopaque = (OblivPageOpaque) PageGetSpecialPointer(page);
	oopaque->o_blkno = blkno;
}

/**
 *
 * This function follows a logic similar to the function RelationAddExtraBlocks in hio.c which  pre-extend a
 * relation by a calculated amount aof blocks.  The idea in this function (fileInit) is to initate every page in
 * the oram relation so that future read or write requests don't have to worry about this. Furthermore, since we know the
 * exact number of blocks the relation must have, we can allocate the space once and never worry about this again.
 * */
void 
heap_fileInit(const char *filename, unsigned int nblocks, unsigned int blocksize) {
	sgx_status_t status;
	char* blocks;
	Page page;
	int offset;
	status = SGX_SUCCESS;

	blocks = (char*) malloc(blocksize*nblocks);
	//selog(DEBUG1, "going to initialize %u pages of relation  %s\n", nblocks, filename);

	for(offset = 0; offset < nblocks; offset++){
		page =  blocks + (offset * BLCKSZ);
		heap_pageInit(page, DUMMY_BLOCK, (Size) blocksize);
	}


	status = outFileInit(filename, blocks, nblocks, blocksize, nblocks*BLCKSZ);
	if (status != SGX_SUCCESS) {
		selog(ERROR, "Could not initialize relation %s\n", filename);
	}
	free(blocks);
	
}



void 
heap_fileRead(PLBlock block, const char *filename, const BlockNumber ob_blkno) {
	
	//selog(DEBUG1, "heap_fileRead %d", ob_blkno);
	sgx_status_t status;
	OblivPageOpaque oopaque;

	status = SGX_SUCCESS;
 	block->block = (void*) malloc(BLCKSZ);

	status = outFileRead(block->block, filename, ob_blkno, BLCKSZ);

	if (status != SGX_SUCCESS) {
		selog(ERROR, "Could not read %d from relation %s\n", ob_blkno, filename);
	}

	oopaque = (OblivPageOpaque) PageGetSpecialPointer((Page) block->block);
	block->blkno = oopaque->o_blkno;
	block->size = BLCKSZ;

	//selog(DEBUG1, "requested %d and block has real blkno %d", ob_blkno, block->blkno);

}


void 
heap_fileWrite(const PLBlock block, const char *filename, const BlockNumber ob_blkno) {
	sgx_status_t status = SGX_SUCCESS;
	OblivPageOpaque oopaque;


	if(block->blkno == DUMMY_BLOCK){
		//selog(DEBUG1, "Requested write of DUMMY_BLOCK");
		/**
		* When the blocks to write to the fully are dummy, they have to be
		* initialized to keep a consistent state for next reads. We might
		* be able to optimize and
		* remove this extra step by removing some verifications
		* on the ocalls.
		*/
		//selog(DEBUG1, "Going to write DUMMY_BLOCK");
		heap_pageInit((Page) block->block, DUMMY_BLOCK, BLCKSZ);
	}
	oopaque = (OblivPageOpaque) PageGetSpecialPointer((Page) block->block);
	//selog(DEBUG1, "heap_fileWrite %d with block %d and special %d ", ob_blkno, block->blkno, oopaque->o_blkno);
	selog(DEBUG1, "heap_fileWrite for file %s", filename);
	status = outFileWrite(block->block, filename, ob_blkno, BLCKSZ);

	if (status != SGX_SUCCESS) {
		selog(ERROR, "Could not write %d on relation %s\n", ob_blkno, filename);
	}
}


void 
heap_fileClose(const char * filename) {
	sgx_status_t status = SGX_SUCCESS;
	status = outFileClose(filename);
	
	if (status != SGX_SUCCESS) {
		selog(ERROR, "Could not close relation %s\n", filename);
	}
}

AMOFile* heap_ofileCreate(){

    AMOFile *file = (AMOFile *) malloc(sizeof(AMOFile));
    file->ofileinit = &heap_fileInit;
    file->ofileread = &heap_fileRead;
    file->ofilewrite = &heap_fileWrite;
    file->ofileclose = &heap_fileClose;
    return file;

}