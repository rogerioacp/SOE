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

unsigned int init_offset;

/* number of blocks requested to be allocated for each oram level. */
int		   *o_nblocks;



void init_root(const char* filename){

    char	    *tmpPage;
    char        *destPage;
    sgx_status_t status;
    

    status = SGX_SUCCESS;


    tmpPage = malloc(BLCKSZ);
    destPage = malloc(BLCKSZ);


    ost_pageInit(tmpPage, DUMMY_BLOCK, BLCKSZ);

#ifndef CPAGES
    page_encryption((unsigned char *) tmpPage, (unsigned char *) destPage);
#else
    memcpy(destPage, tmpPage, BLCKSZ);
#endif

    status = outFileInit(filename, destPage, 1, BLCKSZ, BLCKSZ, 0);

	if (status != SGX_SUCCESS){
        selog(ERROR, "Could not initialize relation %s\n", filename);
	}

    free(tmpPage);
    free(destPage);
    init_offset++;

}

void
ost_status(OSTreeState state)
{
	ostate = state;
   o_nblocks = (int *) malloc(sizeof(int) * ostate->nlevels); 
}

void
ost_pageInit(Page page, int blkno, Size blocksize)
{
	BTPageOpaqueOST ovflopaque;

	PageInit_s(page, blocksize, sizeof(BTPageOpaqueDataOST));

	ovflopaque = (BTPageOpaqueOST) PageGetSpecialPointer_s(page);

	ovflopaque->btpo_prev = InvalidBlockNumber;
	ovflopaque->btpo_next = InvalidBlockNumber;
	ovflopaque->btpo.level = 0;
    ovflopaque->btpo_flags = 0;
    ovflopaque->o_blkno = blkno;

    ovflopaque->location[0] = 0;
    ovflopaque->location[1] = 0;
    memset(ovflopaque->counters, 0, sizeof(uint32)*300);
}

/**
 * TODO: initialize the file only once as this function will be called
 * multiple times, one for each ORAM.
 * */
FileHandler
ost_fileInit(const char *filename, unsigned int nblocks, 
             unsigned int blocksize, unsigned int lsize, void *appData)
{
	sgx_status_t status;
	char	   *blocks;
	char	   *destPage;
	char	   *tmpPage;

	status = SGX_SUCCESS;

	/* At least one block for the root. */

	int			tnblocks = nblocks;

	int         offset;
	int			allocBlocks = 0;
	int			boffset = init_offset;
	int			clevel = *((int *) appData);
    

    //selog(DEBUG1, "request ost_fileInit of %d nblocks\n", nblocks);

    do
    {
	    allocBlocks = Min_s(tnblocks, BATCH_SIZE);
        
        blocks = (char *) malloc(BLCKSZ * allocBlocks);
		tmpPage = malloc(blocksize);

        for (offset = 0; offset < allocBlocks; offset++)
        {
            destPage = blocks + (offset * BLCKSZ);
			ost_pageInit(tmpPage, DUMMY_BLOCK, (Size) blocksize);

        #ifndef CPAGES
			page_encryption((unsigned char *) tmpPage, (unsigned char *) destPage);
		#else
			memcpy(destPage, tmpPage, BLCKSZ);
		#endif
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

    init_offset += nblocks;
    selog(DEBUG1, "Init offset is at %d\n", init_offset);
	o_nblocks[clevel] = nblocks;

    return NULL;
}


/*void
ost_fileRead(FileHandler handler, PLBlock block, const char *filename, const BlockNumber ob_blkno, void *appData)*/
void ost_fileRead(FileHandler handler, 
				  const char *filename, 
				  PLBList blocks,
				  BNArray blkns, 
				  unsigned int nblocks, void *appData)
{
	sgx_status_t 		status;
	BTPageOpaqueOST 	oopaque;
	PLBlock         	block;
    unsigned char       *cblock;
	char                *encBlocks;
    int             	offset, ciphSize, posSizes, clevel;
	unsigned int 		l_offset =0;
    unsigned int        l_index = 0;
	int 				*l_ob_blknos;

	status = SGX_SUCCESS;
	clevel = *((int *) appData);

    ciphSize = BLCKSZ*nblocks;
    posSizes = sizeof(unsigned int)*nblocks;
    //selog(DEBUG1, "ost_fileRead %d blocks", nblocks);
	l_ob_blknos = (int*) malloc(sizeof(int)*nblocks);
	encBlocks = (char *) malloc(ciphSize);
    memset(l_ob_blknos, 0, sizeof(int)*nblocks);
    
    /**
     * We calculate an offset of where each level start as all of the levels
     * are stored in a single file, even tough they are independent ORAMS.
     **/

    /**
     * TODO: Can this loop be optimized to be computed once during 
     * initialization?
     * 
     */
	if (clevel > 0)
	{
		l_offset = 1;
		/* Fanout of previous levels */
		for (l_index = 0; l_index < clevel - 1; l_index++)
		{
			l_offset += o_nblocks[l_index];
		}
	}

	for(offset = 0; offset < nblocks; offset++){
        //selog(DEBUG1, "%d blkns %d and loffset %d", offset, blkns[offset], l_offset);
		l_ob_blknos[offset] = blkns[offset] + l_offset;
	}
    //selog(DEBUG1, "l_ob_blknos %d", offset);
	status = outFileRead(filename, encBlocks, ciphSize, l_ob_blknos, posSizes);
    
	if (status != SGX_SUCCESS)
	{
		selog(ERROR, "Could not read blocks from relation %s\n", filename);
	}

	for(offset = 0; offset < nblocks; offset++){
		block = blocks[offset];
		block->block = (void *) malloc(BLCKSZ);

		cblock = (unsigned char*) &encBlocks[offset*BLCKSZ];

		#ifndef CPAGES
			page_decryption(cblock, (unsigned char *) block->block);
		#else
	    	memcpy(block->block, cblock, BLCKSZ);
		#endif

		oopaque = (BTPageOpaqueOST) PageGetSpecialPointer_s((Page) block->block);
		block->blkno = oopaque->o_blkno;
		block->size = BLCKSZ;
	    block->location[0] = oopaque->location[0];
	    block->location[1] = oopaque->location[1];
	}

	free(encBlocks);
	free(l_ob_blknos);

}


void
ost_fileWrite(FileHandler handler,
			 const char *filename, 
			 PLBList blocks, 
			 BNArray blkns,
			 unsigned int nblocks,
			 void *appData)
{

	sgx_status_t 		status = SGX_SUCCESS;
	BTPageOpaqueOST 	oopaque = NULL;
	char                *encPages;    
	unsigned char	   	*encpage;
	PLBlock         	block;
    int             	offset, pagesSize, posSize;
	unsigned int 		l_offset = 0;
    unsigned int        l_index = 0;
	int					clevel = *((int *) appData);
	int 				*l_ob_blknos;
    //selog(DEBUG1, "ost_fileWrite %d blocks", nblocks);
    pagesSize = BLCKSZ*nblocks;
    posSize = sizeof(unsigned int)*nblocks;
    encPages = (char *) malloc(pagesSize);
    l_ob_blknos = (int*) malloc(sizeof(int)*nblocks);
    memset(l_ob_blknos, 0, sizeof(int)*nblocks);


	if (clevel > 0)
	{
		l_offset = 1;

		/* Fanout of previous levels */
		for (l_index = 0; l_index < clevel - 1; l_index++)
		{
			l_offset += o_nblocks[l_index];
		}
	}

	for(offset = 0; offset < nblocks; offset++){

		l_ob_blknos[offset] = blkns[offset] + l_offset;
	}

	for(offset = 0; offset < nblocks; offset++){
		block = blocks[offset];
		encpage = (unsigned char*) &encPages[offset*BLCKSZ];

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
		oopaque->o_blkno = block->blkno;
	    oopaque->location[0] = block->location[0];
	    oopaque->location[1] = block->location[1];

		#ifndef CPAGES
			page_encryption((unsigned char *) block->block, encpage);
		#else
	 		memcpy(encpage, block->block, BLCKSZ);
		#endif
	}

    status = outFileWrite(filename, encPages, pagesSize, l_ob_blknos, posSize);

	if (status != SGX_SUCCESS)
	{
		selog(ERROR, "Could not write blocks to relation %s\n", filename);
	}

	free(encPages);
	free(l_ob_blknos);
}


void
ost_fileClose(FileHandler handler, const char *filename, void *appData)
{
	sgx_status_t status = SGX_SUCCESS;
    if(o_nblocks != NULL){
	    status = outFileClose(filename);
        free(o_nblocks);
        o_nblocks = NULL;
	    if (status != SGX_SUCCESS)
	    {
		    selog(ERROR, "Could not close relation %s\n", filename);
	    }
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
