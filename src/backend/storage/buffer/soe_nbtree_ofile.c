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

#include "access/soe_nbtree.h"
#include "logger/logger.h"
#include "storage/soe_nbtree_ofile.h"
#include "storage/soe_bufpage.h"

#include <oram/plblock.h>
#include <string.h>
#include <stdlib.h>


void nbtree_pageInit(Page page, int blkno, Size blocksize){
    BTPageOpaque ovflopaque;

	PageInit_s(page,  blocksize, sizeof(BTPageOpaqueData));

	ovflopaque = (BTPageOpaque) PageGetSpecialPointer_s(page);

	ovflopaque->o_blkno = blkno;

	ovflopaque->btpo_prev = InvalidBlockNumber;
	ovflopaque->btpo_next = InvalidBlockNumber;
	ovflopaque->btpo.level = 0;
	ovflopaque->btpo_flags = 0;
}

/**
 *
 * This function follows a logic similar to the function 
 * _hash_alloc_buckets in soe_hashpage.c.
 * */
void 
nbtree_fileInit(const char *filename, unsigned int nblocks, unsigned int blocksize) {
	sgx_status_t status;
	char* blocks;
	char* page;
	int offset;
	status = SGX_SUCCESS;
	blocks = (char*) malloc(blocksize*nblocks);
	//BTPageOpaque oopaque;

	for(offset = 0; offset < nblocks; offset++){
		page =  blocks + (offset * BLCKSZ);
		nbtree_pageInit(page, DUMMY_BLOCK, (Size) blocksize);
		//memcpy((char*) blocks + (offset*BLCKSZ), page, blocksize);
		//oopaque = (BTPageOpaque) PageGetSpecialPointer_s(page);
		//selog(DEBUG1, "hash_fileinit block %d has real block id %d", offset, oopaque->o_blkno);

	}

	status = outFileInit(filename, blocks, nblocks, blocksize, nblocks*BLCKSZ);
	if (status != SGX_SUCCESS) {
		selog(ERROR, "Could not initialize relation %s\n", filename);
	}
	free(blocks);
}


void 
nbtree_fileRead(PLBlock block, const char *filename, const BlockNumber ob_blkno) {
	sgx_status_t status;
	BTPageOpaque oopaque;
	//selog(DEBUG1, "hash_fileRead %d", ob_blkno);
	status = SGX_SUCCESS;
 	block->block = (void*) malloc(BLCKSZ);

	status = outFileRead(block->block, filename, ob_blkno, BLCKSZ);

	if (status != SGX_SUCCESS) {
		selog(ERROR, "Could not read %d from relation %s\n", ob_blkno, filename);
	}

	oopaque = (BTPageOpaque) PageGetSpecialPointer_s((Page) block->block);
	block->blkno = oopaque->o_blkno;
	block->size = BLCKSZ;
	//selog(DEBUG1, "requested %d and block has real blkno %d", ob_blkno, block->blkno);
}


void 
nbtree_fileWrite(const PLBlock block, const char *filename, const BlockNumber ob_blkno) {
	sgx_status_t status = SGX_SUCCESS;
	//BTPageOpaque oopaque = NULL;


	if(block->blkno == DUMMY_BLOCK){
		//selog(DEBUG1, "Requested write of DUMMY_BLOCK");
		/**
		* When the blocks to write to the file are dummy, they have to be
		* initialized to keep a consistent state for next reads. We might
		* be able to optimize and
		* remove this extra step by removing some verifications
		* on the ocalls.
		*/
		//selog(DEBUG1, "Going to write DUMMY_BLOCK");
		nbtree_pageInit((Page) block->block, DUMMY_BLOCK, BLCKSZ);
	}
	/*oopaque = (BTPageOpaque) PageGetSpecialPointer_s((Page) block->block);

	if(block->blkno == 0 ){
		BTMetaPageData *metad;
		metad = BTPageGetMeta_s((Page) block->block);
		selog(DEBUG1, "2-Metapage current root is %d and level is %d and special %d",metad->btm_root,metad->btm_level, oopaque->o_blkno);
	}*/

	//selog(DEBUG1, "hash_fileWrite %d with block %d and special %d ", ob_blkno, block->blkno, oopaque->o_blkno);
	//selog(DEBUG1, "hash_fileWrite for file %s", filename);
	status = outFileWrite(block->block, filename, ob_blkno, BLCKSZ);

	if (status != SGX_SUCCESS) {
		selog(ERROR, "Could not write %d on relation %s\n", ob_blkno, filename);
	}
}


void 
nbtree_fileClose(const char * filename) {
	sgx_status_t status = SGX_SUCCESS;
	status = outFileClose(filename);
	
	if (status != SGX_SUCCESS) {
		selog(ERROR, "Could not close relation %s\n", filename);
	}
}



AMOFile *nbtree_ofileCreate(){

    AMOFile *file = (AMOFile*) malloc(sizeof(AMOFile));
    file->ofileinit = &nbtree_fileInit;
    file->ofileread = &nbtree_fileRead;
    file->ofilewrite = &nbtree_fileWrite;
    file->ofileclose = &nbtree_fileClose;
    return file;

}