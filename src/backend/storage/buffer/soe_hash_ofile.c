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
#include "common/soe_pe.h"
#include "logger/logger.h"
#include "storage/soe_hash_ofile.h"
#include "storage/soe_bufpage.h"

#include <oram/plblock.h>
#include <string.h>
#include <stdlib.h>


void hash_pageInit(Page page, int blkno, Size blocksize){
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
hash_fileInit(const char *filename, unsigned int nblocks, unsigned int blocksize, void* appData) {
	sgx_status_t status;

	char* blocks;
	char* tmpPage;

	Page destPage;
	status = SGX_SUCCESS;
	
	int tnblocks = nblocks;
	int offset;
	int allocBlocks = 0;
	int boffset = 0;

	do{
		//selog(DEBUG1, "Going for boffset %d on btree init with tnblocks %d", boffset, tnblocks);

		allocBlocks = Min_s(tnblocks, BATCH_SIZE);

		blocks = (char*) malloc(blocksize*allocBlocks);
		tmpPage = (char*) malloc(blocksize);

	//	HashPageOpaque oopaque;

		for(offset = 0; offset < allocBlocks; offset++){
			destPage  =  blocks + (offset * BLCKSZ);
			hash_pageInit(tmpPage, DUMMY_BLOCK, (Size) blocksize);
			page_encryption((unsigned char*) tmpPage, (unsigned char*) destPage);
			//oopaque = (HashPageOpaque) PageGetSpecialPointer_s(page);
			//selog(DEBUG1, "hash_fileinit block %d has real block id %d", offset, oopaque->o_blkno);

		}

		status = outFileInit(filename, blocks, allocBlocks, blocksize, allocBlocks*BLCKSZ, boffset);
		if (status != SGX_SUCCESS) {
			selog(ERROR, "Could not initialize relation %s\n", filename);
		}

		free(blocks);
		free(tmpPage);
		tnblocks -= BATCH_SIZE;
		boffset += BATCH_SIZE;
	}while(tnblocks > 0 );
}


void 
hash_fileRead(PLBlock block, const char *filename, const BlockNumber ob_blkno, void* appData) {
	sgx_status_t status;
	HashPageOpaque oopaque;
	//selog(DEBUG1, "hash_fileRead %d", ob_blkno);
	status = SGX_SUCCESS;
	char* ciphertexBlock;

 	block->block = (void*) malloc(BLCKSZ);
 	ciphertexBlock = (char*) malloc(BLCKSZ);



	status = outFileRead(ciphertexBlock, filename, ob_blkno, BLCKSZ);
 	page_decryption((unsigned char*) ciphertexBlock, (unsigned char*) block->block);

	if (status != SGX_SUCCESS) {
		selog(ERROR, "Could not read %d from relation %s\n", ob_blkno, filename);
	}

	oopaque = (HashPageOpaque) PageGetSpecialPointer_s((Page) block->block);
	block->blkno = oopaque->o_blkno;
	block->size = BLCKSZ;
	free(ciphertexBlock);
	//selog(DEBUG1, "requested %d and block has real blkno %d", ob_blkno, block->blkno);
}


void 
hash_fileWrite(const PLBlock block, const char *filename, const BlockNumber ob_blkno, void* appData) {
	sgx_status_t status = SGX_SUCCESS;
	char* encPage = (char*) malloc(BLCKSZ);

	//HashPageOpaque oopaque = NULL;


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
		hash_pageInit((Page) block->block, DUMMY_BLOCK, BLCKSZ);
	}
	page_encryption((unsigned char*) block->block, (unsigned char*) encPage);

	//oopaque = (HashPageOpaque) PageGetSpecialPointer_s((Page) block->block);
	//selog(DEBUG1, "hash_fileWrite %d with block %d and special %d ", ob_blkno, block->blkno, oopaque->o_blkno);
	//selog(DEBUG1, "hash_fileWrite for file %s", filename);
	status = outFileWrite(encPage, filename, ob_blkno, BLCKSZ);

	if (status != SGX_SUCCESS) {
		selog(ERROR, "Could not write %d on relation %s\n", ob_blkno, filename);
	}

	free(encPage);
}


void 
hash_fileClose(const char * filename, void* appData) {
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