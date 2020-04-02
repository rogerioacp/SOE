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
#include "common/soe_pe.h"

#include <oram/plblock.h>
#include <string.h>
#include <stdlib.h>



void
nbtree_pageInit(Page page, int blkno, unsigned int locationSize, Size blocksize)
{
	BTPageOpaque ovflopaque;

	PageInit_s(page, blocksize, sizeof(BTPageOpaqueData));

	ovflopaque = (BTPageOpaque) PageGetSpecialPointer_s(page);

	ovflopaque->btpo_prev = InvalidBlockNumber;
	ovflopaque->btpo_next = InvalidBlockNumber;
	ovflopaque->btpo.level = 0;
	ovflopaque->btpo_flags = 0;
    ovflopaque->o_blkno = blkno;
   // ovflopaque->lsize = locationSize;
    
    ovflopaque->location[0] = 0;
    ovflopaque->location[1] = 0;
    memset(ovflopaque->counters,0, sizeof(uint32)*300);
}

/**
 *
 * This function follows a logic similar to the function
 * _hash_alloc_buckets in soe_hashpage.c.
 * */
FileHandler
nbtree_fileInit(const char *filename, unsigned int nblocks, unsigned int blocksize, unsigned int locationSize, void *appData)
{
	sgx_status_t status;
	char	   *blocks;
	char	   *destPage;
	char	   *tmpPage;
	int			tnblocks = nblocks;
	int			offset;
	int			allocBlocks = 0;
	int			boffset = 0;


	status = SGX_SUCCESS;
	
    do
	{
		/* BTPageOpaque oopaque; */
		allocBlocks = Min_s(tnblocks, BATCH_SIZE);

		blocks = (char *) malloc(BLCKSZ * allocBlocks);
		tmpPage = malloc(blocksize);

		for (offset = 0; offset < allocBlocks; offset++)
		{
			destPage = blocks + (offset * BLCKSZ);
			nbtree_pageInit(tmpPage, DUMMY_BLOCK, locationSize, BLCKSZ);
			#ifndef CPAGES
				page_encryption((unsigned char *) tmpPage, (unsigned char *) destPage);
			#else
				memcpy(destPage, tmpPage, BLCKSZ);
			#endif
		}

		status = outFileInit(filename, blocks, allocBlocks, BLCKSZ, allocBlocks * BLCKSZ, boffset);

		if (status != SGX_SUCCESS)
		{
			selog(ERROR, "Could not initialize relation %s\n", filename);
		}
        
		free(blocks);
		free(tmpPage);

		tnblocks -= BATCH_SIZE;
		boffset += BATCH_SIZE;
	} while (tnblocks > 0);

    return NULL;
}


void
nbtree_fileRead(FileHandler handler, PLBlock block, const char *filename, const BlockNumber ob_blkno, void *appData)
{
	sgx_status_t status;
	BTPageOpaque oopaque;

	/* selog(DEBUG1, "nbtree_fileRead %d", ob_blkno); */
	status = SGX_SUCCESS;
	char	   *ciphertextBlock;

	block->block = (void *) malloc(BLCKSZ);
	ciphertextBlock = (char *) malloc(BLCKSZ);

	status = outFileRead(ciphertextBlock, filename, ob_blkno, BLCKSZ);
	#ifndef CPAGES
		page_decryption((unsigned char *) ciphertextBlock, (unsigned char *) block->block);
	#else
    	memcpy(block->block, ciphertextBlock, BLCKSZ);
	#endif

	if (status != SGX_SUCCESS)
	{
		selog(ERROR, "Could not read %d from relation %s", ob_blkno, filename);
	}

	oopaque = (BTPageOpaque) PageGetSpecialPointer_s((Page) block->block);
	block->blkno = oopaque->o_blkno;
	block->size = BLCKSZ;
    block->location[0] = oopaque->location[0];
    block->location[1] = oopaque->location[1];
	free(ciphertextBlock);
}


void
nbtree_fileWrite(FileHandler handler, const PLBlock block, const char *filename, const BlockNumber ob_blkno, void *appData)
{
	sgx_status_t status = SGX_SUCCESS;
    BTPageOpaque oopaque;

	char	   *encpage;

	encpage = (char *) malloc(BLCKSZ);

	if (block->blkno == DUMMY_BLOCK)
	{
		/* selog(DEBUG1, "Requested write of DUMMY_BLOCK"); */
		/**
		* When the blocks to write to the file are dummy, they have to be
		* initialized to keep a consistent state for next reads. We might
		* be able to optimize and
		* remove this extra step by removing some verifications
		* on the ocalls.
		*/
		//selog(DEBUG1, "Going to write DUMMY_BLOCK");
		nbtree_pageInit((Page) block->block, DUMMY_BLOCK, 0, BLCKSZ);
	}

    oopaque = (BTPageOpaque) PageGetSpecialPointer_s((Page)block->block);
    oopaque->o_blkno = block->blkno;
    oopaque->location[0] = block->location[0];
    oopaque->location[1] = block->location[1];

     
	#ifndef CPAGES
		page_encryption((unsigned char *) block->block, (unsigned char *) encpage);
	#else
		 memcpy(encpage, block->block, BLCKSZ);
	#endif
	status = outFileWrite(encpage, filename, ob_blkno, BLCKSZ);

	if (status != SGX_SUCCESS)
	{
		selog(ERROR, "Could not write %d on relation %s\n", ob_blkno, filename);
	}
	free(encpage);
}


void
nbtree_fileClose(FileHandler handler, const char *filename, void *appData)
{
	sgx_status_t status = SGX_SUCCESS;

	status = outFileClose(filename);

	if (status != SGX_SUCCESS)
	{
		selog(ERROR, "Could not close relation %s\n", filename);
	}
}



AMOFile *
nbtree_ofileCreate()
{

	AMOFile    *file = (AMOFile *) malloc(sizeof(AMOFile));

	file->ofileinit = &nbtree_fileInit;
	file->ofileread = &nbtree_fileRead;
	file->ofilewrite = &nbtree_fileWrite;
	file->ofileclose = &nbtree_fileClose;
	return file;

}
