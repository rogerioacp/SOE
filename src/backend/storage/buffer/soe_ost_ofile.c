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
#include "storage/soe_ost_ofile.h"
#include "storage/soe_bufpage.h"
#include "common/soe_pe.h"
#include "access/soe_ost.h"

#include <oram/plblock.h>
#include <string.h>
#include <stdlib.h>


OSTreeState ostate;
int			initialized;

/* number of blocks requested to be allocated for each oram level. */
int		   *o_nblocks;


void
ost_status(OSTreeState state)
{
	ostate = state;
	initialized = 0;
}

void
ost_pageInit(Page page, int blkno, Size blocksize)
{
	BTPageOpaqueOST ovflopaque;

	PageInit_s(page, blocksize, sizeof(BTPageOpaqueDataOST));

	ovflopaque = (BTPageOpaqueOST) PageGetSpecialPointer_s(page);


	ovflopaque->btpo_prev = InvalidBlockNumber;
	ovflopaque->btpo_next = InvalidBlockNumber;
	ovflopaque->btpo_flags = 0;
	ovflopaque->btpo.o_blkno = blkno;

}

/**
 * TODO: initialize the file only once as this function will be called
 * multiple times, one for each ORAM.
 * */
void
ost_fileInit(const char *filename, unsigned int nblocks, unsigned int blocksize, void *appData)
{
	sgx_status_t status;
	char	   *blocks;
	char	   *destPage;
	char	   *tmpPage;

	status = SGX_SUCCESS;

	/* At least one block for the root. */

	int			tnblocks = 1;

	int         offset;
	int			allocBlocks = 0;
	int			boffset = 0;
	int			l = 0;
	int			clevel = *((int *) appData);

	if (!initialized)
	{
		o_nblocks = (int *) malloc(sizeof(int) * ostate->nlevels);

		for (l = 0; l < ostate->nlevels; l++)
		{
			tnblocks += ostate->fanouts[l];
		}
		tnblocks = tnblocks * 2;

		do
		{
			/* BTPageOpaque oopaque; */
			allocBlocks = Min_s(tnblocks, BATCH_SIZE);

			blocks = (char *) malloc(BLCKSZ * allocBlocks);
			tmpPage = malloc(blocksize);

			for (offset = 0; offset < allocBlocks; offset++)
			{
				destPage = blocks + (offset * BLCKSZ);
				ost_pageInit(tmpPage, DUMMY_BLOCK, (Size) blocksize);
				page_encryption((unsigned char *) tmpPage, (unsigned char *) destPage);
			}

			status = outFileInit(filename, blocks, allocBlocks, blocksize, allocBlocks * BLCKSZ, boffset);

			if (status != SGX_SUCCESS)
			{
				selog(ERROR, "Could not initialize relation %s\n", filename);
			}
			free(blocks);
			free(tmpPage);

			tnblocks -= BATCH_SIZE;
			boffset += BATCH_SIZE;
		} while (tnblocks > 0);
		initialized = 1;
	}

	o_nblocks[clevel] = nblocks;

}


void
ost_fileRead(PLBlock block, const char *filename, const BlockNumber ob_blkno, void *appData)
{
	sgx_status_t status;
	BTPageOpaqueOST oopaque;
	int			clevel = *((int *) appData);

	status = SGX_SUCCESS;
	char	   *ciphertextBlock;
	unsigned int l_offset = 0;
	unsigned int l_index;
	unsigned int l_ob_blkno = 0;

	if (clevel > 0)
	{
		l_offset = 1;
		/* Fanout of previous levels */
		for (l_index = 0; l_index < clevel - 1; l_index++)
		{
			l_offset += o_nblocks[l_index];
		}
	}

	l_ob_blkno = ob_blkno + l_offset;

	block->block = (void *) malloc(BLCKSZ);
	ciphertextBlock = (char *) malloc(BLCKSZ);

	status = outFileRead(ciphertextBlock, filename, l_ob_blkno, BLCKSZ);
	page_decryption((unsigned char *) ciphertextBlock, (unsigned char *) block->block);

	if (status != SGX_SUCCESS)
	{
		selog(ERROR, "Could not read %d from relation %s\n", ob_blkno, filename);
	}

	oopaque = (BTPageOpaqueOST) PageGetSpecialPointer_s((Page) block->block);
	block->blkno = oopaque->btpo.o_blkno;
	block->size = BLCKSZ;
	free(ciphertextBlock);
}


void
ost_fileWrite(const PLBlock block, const char *filename, const BlockNumber ob_blkno, void *appData)
{

	sgx_status_t status = SGX_SUCCESS;
	BTPageOpaqueOST oopaque = NULL;
	char	   *encpage;
	unsigned int l_offset = 0;
	unsigned int l_index;
	unsigned int l_ob_blkno = 0;
	int			clevel = *((int *) appData);

	if (clevel > 0)
	{
		l_offset = 1;

		/* Fanout of previous levels */
		for (l_index = 0; l_index < clevel - 1; l_index++)
		{
			l_offset += o_nblocks[l_index];
		}
	}

	l_ob_blkno = ob_blkno + l_offset;

	encpage = (char *) malloc(BLCKSZ);

	if (block->blkno == DUMMY_BLOCK)
	{
		/**
		* When the blocks to write to the file are dummy, they have to be
		* initialized to keep a consistent state for next reads. We might
		* be able to optimize and
		* remove this extra step by removing some verifications
		* on the ocalls.
		*/
		/* selog(DEBUG1, "Going to write DUMMY_BLOCK"); */
		ost_pageInit((Page) block->block, DUMMY_BLOCK, BLCKSZ);
	}
	oopaque = (BTPageOpaqueOST) PageGetSpecialPointer_s((Page) block->block);
	oopaque->btpo.o_blkno = block->blkno;


	page_encryption((unsigned char *) block->block, (unsigned char *) encpage);
	status = outFileWrite(encpage, filename, l_ob_blkno, BLCKSZ);

	if (status != SGX_SUCCESS)
	{
		selog(ERROR, "Could not write %d on relation %s\n", ob_blkno, filename);
	}
	free(encpage);
}


void
ost_fileClose(const char *filename, void *appData)
{
	sgx_status_t status = SGX_SUCCESS;

	status = outFileClose(filename);
    free(o_nblocks);
	if (status != SGX_SUCCESS)
	{
		selog(ERROR, "Could not close relation %s\n", filename);
	}
}



AMOFile *
ost_ofileCreate()
{

	AMOFile    *file = (AMOFile *) malloc(sizeof(AMOFile));

	file->ofileinit = &ost_fileInit;
	file->ofileread = &ost_fileRead;
	file->ofilewrite = &ost_fileWrite;
	file->ofileclose = &ost_fileClose;
	return file;

}
