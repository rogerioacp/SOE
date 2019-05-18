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

#include "access/soe_hash.h"
#include "logger/logger.h"
#include "storage/soe_hash_ofile.h"
#include "storage/soe_bufpage.h"

#include <oram/plblock.h>
#include <string.h>
#include <stdlib.h>


void hash_pageInit(Page page, BlockNumber blkno, Size blocksize){
    HashPageOpaque ovflopaque;

	PageInit_s(page,  blocksize, sizeof(HashPageOpaqueData));

	ovflopaque = (HashPageOpaque) PageGetSpecialPointer_s(page);

	ovflopaque->o_blkno = blkno;

	ovflopaque->hasho_prevblkno = InvalidBlockNumber;
	ovflopaque->hasho_nextblkno = InvalidBlockNumber;
	ovflopaque->hasho_bucket = -1;
	ovflopaque->hasho_flag = LH_UNUSED_PAGE;
	ovflopaque->hasho_page_id = HASHO_PAGE_ID;
}

/**
 *
 * This function follows a logic similar to the function 
 * _hash_alloc_buckets in soe_hashpage.c.
 * */
void 
hash_fileInit(const char *filename, unsigned int nblocks, unsigned int blocksize) {
	sgx_status_t status;
	char* blocks;
	char* page;
	int offset;
	status = SGX_SUCCESS;
	blocks = (char*) malloc(blocksize*nblocks);
	PGAlignedBlock zerobuf;

	for(offset = 0; offset < nblocks; offset++){
		page = (Page) zerobuf.data;
		hash_pageInit(page, DUMMY_BLOCK, (Size) blocksize);
		memcpy((char*) blocks + (offset*BLCKSZ), page, blocksize);
	}

	status = outFileInit(filename, blocks, nblocks, blocksize, nblocks*BLCKSZ);
	if (status != SGX_SUCCESS) {
		selog(ERROR, "Could not initialize relation %s\n", filename);
	}
	free(blocks);
}


void 
hash_fileRead(PLBlock block, const char *filename, const BlockNumber ob_blkno) {
	sgx_status_t status;
	HashPageOpaque oopaque;

	status = SGX_SUCCESS;
 	block->block = (void*) malloc(BLCKSZ);

	status = outFileRead(block->block, filename, ob_blkno, BLCKSZ);

	if (status != SGX_SUCCESS) {
		selog(ERROR, "Could not read %d from relation %s\n", ob_blkno, filename);
	}

	oopaque = (HashPageOpaque) PageGetSpecialPointer_s((Page) block->block);
	block->blkno = oopaque->o_blkno;
	block->size = BLCKSZ;
}


void 
hash_fileWrite(const PLBlock block, const char *filename, const BlockNumber ob_blkno) {
	sgx_status_t status = SGX_SUCCESS;

	status = outFileWrite(block->block, filename, ob_blkno, BLCKSZ);

	if (status != SGX_SUCCESS) {
		selog(ERROR, "Could not write %d on relation %s\n", ob_blkno, filename);
	}
}


void 
hash_fileClose(const char * filename) {
	sgx_status_t status = SGX_SUCCESS;
	status = outFileClose(filename);
	
	if (status != SGX_SUCCESS) {
		selog(ERROR, "Could not close relation %s\n", filename);
	}
}



AMOFile *hash_ofileCreate(){

    AMOFile *file = (AMOFile*) malloc(sizeof(AMOFile));
    file->ofileinit = &hash_fileInit;
    file->ofileread = &hash_fileRead;
    file->ofilewrite = &hash_fileWrite;
    file->ofileclose = &hash_fileClose;
    return file;

}