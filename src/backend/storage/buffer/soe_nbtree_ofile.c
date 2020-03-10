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

		blocks = (char *) malloc(BLCKSZ * nblocks);
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
nbtree_fileRead(FileHandler handler, const char *filename,  PLBList blocks,
              BNArray blkns, unsigned int nblocks, void *appData)
{
	sgx_status_t    status;
	BTPageOpaque    oopaque;
    PLBlock         block;
	char            *cipherBlocks;
    unsigned char   *cblock;
    int             offset, cipherbSize, posSize;

	//selog(DEBUG1, "nbtree_fileRead %d nblocks", nblocks);
    status = SGX_SUCCESS;
    
    cipherbSize = BLCKSZ*nblocks;
    posSize = sizeof(unsigned int)*nblocks;

	cipherBlocks = (char *) malloc(cipherbSize);


	status = outFileRead(filename, cipherBlocks, cipherbSize, blkns, posSize);
	
    if (status != SGX_SUCCESS)
	{
		selog(ERROR, "Could not read blocks from relation %s", filename);
	}

    for(offset = 0; offset < nblocks; offset++){
    	block = blocks[offset];
		block->block = (void *) malloc(BLCKSZ);
        
        cblock = (unsigned char*) &cipherBlocks[offset*BLCKSZ];

		#ifndef CPAGES
			page_decryption(cBlock, (unsigned char *) block->block);
		#else
	    	memcpy(block->block, cblock, BLCKSZ);
		#endif

		oopaque = (BTPageOpaque) PageGetSpecialPointer_s((Page) block->block);
		block->blkno = oopaque->o_blkno;
		block->size = BLCKSZ;
	    block->location[0] = oopaque->location[0];
	    block->location[1] = oopaque->location[1];
    }
    
	free(cipherBlocks);
}


void
nbtree_fileWrite(FileHandler handler, const char *filename, PLBList blocks, 
          BNArray blkns, unsigned int nblocks, void *appData)
{
	sgx_status_t 	status = SGX_SUCCESS;
    BTPageOpaque 	oopaque;
    PLBlock         block;
    unsigned char   *encpage;
    int             offset, pagesSize, posSize;
	char	        *encPages;
    //selog(DEBUG1, "nbtree_fileWrite %d blocks", nblocks);
    
    pagesSize = BLCKSZ*nblocks;
    posSize = sizeof(unsigned int)*nblocks;

    encPages = (char *) malloc(pagesSize);

    for(offset = 0; offset < nblocks; offset++){

    	block 	=  	blocks[offset];
    	encpage = (unsigned char*)	&encPages[offset*BLCKSZ];

		if (block->blkno == DUMMY_BLOCK){
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
			page_encryption((unsigned char *) block->block, encpage);
		#else
			 memcpy(encpage, block->block, BLCKSZ);
		#endif

    }

    status = outFileWrite(filename, encPages, pagesSize, blkns, posSize);

	if (status != SGX_SUCCESS)
	{
		selog(ERROR, "Could not write blocks to relation %s\n", filename);
	}
	
	free(encPages);
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
