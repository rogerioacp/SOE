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

#include "sgx_trts.h"
#include "enclave/Enclave_t.h"
#include "logger/logger.h"

#include "oram/plblock.h"
#include "oram/ofile.h"
#include <string.h>
#include <stdlib.h>


/**
 *
 * This function follows a logic similar to the function RelationAddExtraBlocks in hio.c which  pre-extend a
 * relation by a calculated amount aof blocks.  The idea in this funciton (fileInit) is to initate every page in
 * the oram relation so that future read or write requests don't have to worry about this. Furthermore, since we know the
 * exact number of blocks the relation must have, we can allocate the space once and never worry about this again.
 * */
void fileInit(const char *filename, unsigned int nblocks, unsigned int blocksize) {
	sgx_status_t status = SGX_SUCCESS;
	status = outFileInit(filename, nblocks, blocksize);
	if (status != SGX_SUCCESS) {
		selog(ERROR, "Could not initialize relation %s\n", filename);
	}
}



void fileRead(PLBlock block, const char *filename, const BlockNumber ob_blkno) {
	sgx_status_t status = SGX_SUCCESS;
	status = outFileRead(block->block, filename, ob_blkno);

	if (status != SGX_SUCCESS) {
		selog(ERROR, "Could not read %d from relation %s\n", ob_blkno, filename);
	}
}


void fileWrite(const PLBlock block, const char *filename, const BlockNumber ob_blkno) {
	sgx_status_t status = SGX_SUCCESS;
	status = outFileWrite(block->block, filename, ob_blkno);

	if (status != SGX_SUCCESS) {
		selog(ERROR, "Could not write %d on relation %s\n", ob_blkno, filename);
	}
}


void fileClose(const char * filename) {
	sgx_status_t status = SGX_SUCCESS;
	status = outFileClose(filename);
	
	if (status != SGX_SUCCESS) {
		selog(ERROR, "Could not close relation %s\n", filename);
	}
}



AMOFile *ofileCreate(){

    AMOFile *file = (AMOFile *) malloc(sizeof(AMOFile));
    file->ofileinit = &fileInit;
    file->ofileread = &fileRead;
    file->ofilewrite = &fileWrite;
    file->ofileclose = &fileClose;
    return file;

}