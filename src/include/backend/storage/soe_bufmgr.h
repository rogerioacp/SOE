

#ifndef SOE_BUFMGR_H
#define SOE_BUFMGR_H



#include "storage/soe_buf.h"
#include "storage/soe_bufpage.h"

#include <storage/block.h>
#include <oram/oram.h>
#include <oram/plblock.h>
#include <collectc/list.h>

/*

 * BufferGetPageSize
 *		Returns the page size within a buffer.
 *
 * Notes:
 *		Assumes buffer is valid.
 *
 *		The buffer can be a raw disk block and need not contain a valid
 *		(formatted) disk page.
 */
/* XXX should dig out of buffer descriptor */
#define BufferGetPageSize(vbuffer, buffer) \
( \
	(Size)BLCKSZ \
)

#define BufferIsValid(vbuffer, bufnum) \
( \
	(bufnum) != InvalidBuffer  \
)

/*
 * Buffer content lock modes (mode argument for LockBuffer())
 */
#define BUFFER_LOCK_UNLOCK		0
#define BUFFER_LOCK_SHARE		1
#define BUFFER_LOCK_EXCLUSIVE	2

typedef struct VRelation{
	int lastBlock;
	ORAMState oram;
	List *buffer; //Buffer containing relation pages
	/* available for use by index AM. 
	 * Similar to a normal relation 
	*/
	void *rd_amcache;
} *VRelation;

typedef struct VBlock{
    int id;
    char* page;
} *VBlock;


/* special block number for ReadBuffer() */
#define P_NEW	InvalidBlockNumber	/* grow the file to get a new page */


extern VRelation initVRelation(ORAMState relstate);

extern Buffer ReadBuffer(VRelation relation, BlockNumber blockNum);

extern Page BufferGetPage(VRelation relation, Buffer buffer);

extern void MarkBufferDirty(VRelation relation, Buffer buffer);

extern BlockNumber BufferGetBlockNumber(Buffer buffer);

extern BlockNumber getNumberOfBlocks(VRelation rel);

#endif /* SOE_BUFPAGE_H */