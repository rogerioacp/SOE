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
#include "common/soe_pe.h"


#include <oram/plblock.h>
#include <oram/ofile.h>
#include <string.h>
#include <stdlib.h>

void
heap_pageInit(Page page, int blkno, Size blocksize)
{

    int*     pblkno; 
	PageInit_s(page, blocksize, sizeof(int));
    pblkno = (int*) PageGetSpecialPointer_s(page);
    *pblkno = blkno;
}

/**
 *
 * This function follows a logic similar to the function RelationAddExtraBlocks in hio.c which  pre-extend a
 * relation by a calculated amount aof blocks.  The idea in this function (fileInit) is to initate every page in
 * the oram relation so that future read or write requests don't have to worry about this. Furthermore, since we know the
 * exact number of blocks the relation must have, we can allocate the space once and never worry about this again.
 * */
void
heap_fileInit(const char *filename, unsigned int nblocks, unsigned int blocksize, void *appData)
{
	sgx_status_t status;
	char	   *blocks;
	char	   *tmpPage;

	Page		destPage;
	int			allocBlocks;
	int			tnblocks = nblocks;

	status = SGX_SUCCESS;
	int			offset = 0;
	int			boffset = 0;
    //selog(DEBUG1," Heap file init");
	
    do
	{
		//selog(DEBUG1, "Going for boffset %d on heap init with tnblocks %d",boffset, tnblocks);
	

		allocBlocks = Min_s(tnblocks, BATCH_SIZE);

		blocks = (char *) malloc(BLCKSZ * allocBlocks);
		tmpPage = (char *) malloc(blocksize);

		
		//selog(DEBUG1, "going to initialize %u pages of relation  %s\n",nblocks, filename);
		

		for (offset = 0; offset < allocBlocks; offset++)
		{
			destPage = blocks + (offset * BLCKSZ);
			heap_pageInit(tmpPage, DUMMY_BLOCK, BLCKSZ);
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

}



void
heap_fileRead(PLBlock block, const char *filename, const BlockNumber ob_blkno, void *appData)
{

	sgx_status_t status;
	char	   *ciphertexBlock;
	int*    r_blkno;

	status = SGX_SUCCESS;

	block->block = (void *) malloc(BLCKSZ);
	ciphertexBlock = (char *) malloc(BLCKSZ);

	
    status = outFileRead(ciphertexBlock, filename, ob_blkno, BLCKSZ);

	#ifndef CPAGES
		page_decryption((unsigned char *) ciphertexBlock, (unsigned char *) block->block);
	#else
    	memcpy(block->block, ciphertexBlock, BLCKSZ);
	#endif

	if (status != SGX_SUCCESS)
	{
		selog(ERROR, "Could not read %d from relation %s\n", ob_blkno, filename);
	}
   
	r_blkno = (int*) PageGetSpecialPointer_s((Page) block->block);

   	block->blkno = *r_blkno;
	block->size = BLCKSZ;
	free(ciphertexBlock);
    //selog(DEBUG1, "Requested read oblivious block %d that has real block %d", ob_blkno, block->blkno);

}


void
heap_fileWrite(const PLBlock block, const char *filename, const BlockNumber ob_blkno, void *appData)
{
	sgx_status_t status = SGX_SUCCESS;
	char	   *encPage = (char *) malloc(BLCKSZ);
    int        *r_blkno;
    
    r_blkno = (int*) PageGetSpecialPointer_s((Page) block->block);
    
    //selog(DEBUG1, "Requested write  oblivious block %d that has real block %d", ob_blkno, *r_blkno);

    if(block->blkno != *r_blkno){
        selog(ERROR, "Block blkno %d  and page blkno %d do not match", block->blkno, *r_blkno);
    }
    
	if (block->blkno == DUMMY_BLOCK)
	{
		//selog(DEBUG1, "Requested write of DUMMY_BLOCK"); 
		/**
		* When the blocks to write to the file are dummy, they have to be
		* initialized to keep a consistent state for next reads. We might
		* be able to optimize and
		* remove this extra step by removing some verifications
		* on the ocalls.
		*/
		heap_pageInit((Page) block->block, DUMMY_BLOCK, BLCKSZ);
	}
	#ifndef CPAGES
		page_encryption((unsigned char *) block->block, (unsigned char *) encPage);
	#else
		memcpy(encPage, block->block, BLCKSZ);
 	#endif
	
	status = outFileWrite(encPage, filename, ob_blkno, BLCKSZ);

	if (status != SGX_SUCCESS)
	{
		selog(ERROR, "Could not write %d on relation %s\n", ob_blkno, filename);
	}

	free(encPage);
}


void
heap_fileClose(const char *filename, void *appData)
{
	sgx_status_t status = SGX_SUCCESS;

	status = outFileClose(filename);

	if (status != SGX_SUCCESS)
	{
		selog(ERROR, "Could not close relation %s\n", filename);
	}
}

AMOFile *
heap_ofileCreate()
{

	AMOFile    *file = (AMOFile *) malloc(sizeof(AMOFile));

	file->ofileinit = &heap_fileInit;
	file->ofileread = &heap_fileRead;
	file->ofilewrite = &heap_fileWrite;
	file->ofileclose = &heap_fileClose;
	return file;

}
